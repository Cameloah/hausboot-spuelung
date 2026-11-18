#include "main.h"

#include <atomic>

#include "version.h"

#define PIN_RELAIS_PUMP GPIO_NUM_17
#define PIN_RELAIS_SOLENOID GPIO_NUM_16
#define PIN_BUTTON GPIO_NUM_19
#define PIN_SENSOR_FLOW GPIO_NUM_14

#define SENSOR_LITER_PER_PULSE 0.0033333

// ------------ system variables ------------ //

// pulses of the flow sensor, counted in the interrupt
static std::atomic<uint32_t> flowPulses(0);

static void IRAM_ATTR count_pulse() {
    flowPulses.fetch_add(1, std::memory_order_relaxed);
}

static double liters() {
    return flowPulses.load() * SENSOR_LITER_PER_PULSE;
}

static Stream& console() {
    return ProjectUtils.console();
}

// ----------------- memory ----------------- //

// Same area and keys as the firmware before 1.2, so stored values carry over.
static pu::Settings config;

// ----------------- outputs ----------------- //

static bool valveOpen = false;
static bool pumpOn = false;

static void setValve(bool open) {
    digitalWrite(PIN_RELAIS_SOLENOID, open ? HIGH : LOW);
    valveOpen = open;
}

static void setPump(bool on) {
    digitalWrite(PIN_RELAIS_PUMP, on ? HIGH : LOW);
    pumpOn = on;
}

// ----------------- control ----------------- //

// The flush program runs step by step from loop(), so the web page and the network keep
// working while it runs and the emergency stop can end it.
enum class Phase : uint8_t { Idle, PreWater, FlushWater, PumpDelay, EndWater };

static Phase phase = Phase::Idle;
static unsigned long phaseStart = 0;

// values taken from memory when the program starts
static struct {
    float flushWater, flushWaterTimeout;
    float preWater, preWaterTimeout;
    float pumpDelay;
    float endWater, endWaterTimeout;
} program;

static bool manualWater = false;
static bool manualPump = false;

static const char* phaseName() {
    switch (phase) {
    case Phase::PreWater:
        return "Vorlauf";
    case Phase::FlushWater:
        return "Spülen";
    case Phase::PumpDelay:
        return "Pumpen-Nachlauf";
    case Phase::EndWater:
        return "Nachlauf";
    case Phase::Idle:
        break;
    }
    if (manualWater || manualPump) {
        return "Manuell";
    }
    return "Bereit";
}

static float secondsInPhase() {
    return (millis() - phaseStart) / 1000.0F;
}

static void enterPhase(Phase next) {
    phase = next;
    phaseStart = millis();
}

static void reportPhase() {
    console().println("Zeit: " + String(secondsInPhase()) + " Sekunden");
    console().println("Liter: " + String(liters()));
}

static void flush_start() {
    if (phase != Phase::Idle) {
        return;
    }
    // manual control ends when the program starts
    manualWater = false;
    manualPump = false;
    setPump(false);

    // ziehe Werte aus dem Speicher
    program.flushWater = config.getFloat("flush-water");
    program.flushWaterTimeout = config.getFloat("flush-water-s");
    program.preWater = config.getFloat("pre-water");
    program.preWaterTimeout = config.getFloat("pre-water-s");
    program.pumpDelay = config.getFloat("pump-delay");
    program.endWater = config.getFloat("end-water");
    program.endWaterTimeout = config.getFloat("end-water-s");

    // setze den Literzähler zurück, öffne das Ventil
    flowPulses = 0;
    setValve(true);
    enterPhase(Phase::PreWater);
    console().println("Ventil geöffnet");
}

static void flush_update() {
    switch (phase) {
    case Phase::Idle:
        return;

    case Phase::PreWater:
        // wir warten bis die Vorlaufmenge durchgeflossen ist
        if (liters() < program.preWater && secondsInPhase() < program.preWaterTimeout) {
            return;
        }
        reportPhase();
        setPump(true);
        enterPhase(Phase::FlushWater);
        console().println("Pumpe gestartet");
        return;

    case Phase::FlushWater:
        // wir warten bis die Spülwassermenge durchgeflossen ist, aber das Ventil soll maximal
        // die definierte Zeit geöffnet bleiben (der Zähler läuft seit dem Vorlauf weiter)
        if (liters() < program.flushWater && secondsInPhase() < program.flushWaterTimeout) {
            return;
        }
        reportPhase();
        setValve(false);
        enterPhase(Phase::PumpDelay);
        console().println("Ventil geschlossen");
        return;

    case Phase::PumpDelay:
        if (secondsInPhase() < program.pumpDelay) {
            return;
        }
        setPump(false);
        console().println("Pumpe gestoppt");
        flowPulses = 0;
        setValve(true);
        enterPhase(Phase::EndWater);
        console().println("Ventil geöffnet");
        return;

    case Phase::EndWater:
        if (liters() < program.endWater && secondsInPhase() < program.endWaterTimeout) {
            return;
        }
        reportPhase();
        setValve(false);
        enterPhase(Phase::Idle);
        console().println("Ventil geschlossen");
        return;
    }
}

static void emergency_stop() {
    setValve(false);
    setPump(false);
    manualWater = false;
    manualPump = false;
    if (phase != Phase::Idle) {
        ProjectUtils.log().info("Spülung abgebrochen");
    }
    enterPhase(Phase::Idle);
    console().println("Stopp");
}

// Manual control while the button on the page is held. Ends by itself when the browser stops
// responding for a second.
static void manual_water(bool pressed) {
    if (pressed && phase == Phase::Idle && !manualWater) {
        setValve(true);
        manualWater = true;
        console().println("Wasser go");
    } else if (!pressed && manualWater) {
        setValve(false);
        manualWater = false;
        console().println("Wasser stopp");
    }
}

static void manual_pump(bool pressed) {
    if (pressed && phase == Phase::Idle && !manualPump) {
        setPump(true);
        manualPump = true;
        console().println("Pumpe go");
    } else if (!pressed && manualPump) {
        setPump(false);
        manualPump = false;
        console().println("Pumpe stopp");
    }
}

static void setting(pu::Page& page, const char* key, const char* label, const char* unit) {
    page.number(config, key, label).unit(unit).min(0).step(0.1);
}

static void build_home_page() {
    pu::Page& home = ProjectUtils.web().home("Klospülung");

    home.section("Einstellungen");
    setting(home, "flush-water", "Spülwassermenge", "L");
    setting(home, "flush-water-s", "Spülwasser Timeout", "s");
    setting(home, "pre-water", "Wasservorlaufmenge", "L");
    setting(home, "pre-water-s", "Wasservorlauf Timeout", "s");
    setting(home, "pump-delay", "Pumpen-Nachlaufzeit", "s");
    setting(home, "end-water", "Nachlaufwassermenge", "L");
    setting(home, "end-water-s", "Nachlaufwassermenge Timeout", "s");

    home.section("Steuerung");
    home.holdButton("Wasser", manual_water);
    home.holdButton("Pumpe", manual_pump);
    home.button("Spülen", flush_start);
    home.button("Not-Stop", emergency_stop).color(pu::Color::Red);

    home.section("Status");
    home.value("Ablauf", []() { return String(phaseName()); });
    home.value("Wassermenge", liters).unit("L");
    home.indicator("Ventil offen", []() { return valveOpen; });
    home.indicator("Pumpe an", []() { return pumpOn; });
}

// ----------------- main ----------------- //

void setup() {
    Serial.begin(115200);

    // configure GPIOs
    pinMode(PIN_RELAIS_PUMP, OUTPUT);
    pinMode(PIN_RELAIS_SOLENOID, OUTPUT);
    setValve(false);
    setPump(false);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_SENSOR_FLOW, INPUT_PULLUP);
    attachInterrupt(PIN_SENSOR_FLOW, count_pulse, FALLING);

    // initialize project utility
    static pu::Config utils;
    utils.deviceName = "Neue Klospülung";
    utils.firmwareVersion = pu::Semver{FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH};
    utils.wifiConnectTimeoutMs = 5000;
    utils.apMode = pu::ApMode::Always;
    utils.apTimeoutMs = 300000; // 5 min
    utils.githubRepo = GITHUB_REPO;
    utils.githubFirmwareAsset = GITHUB_FW_BIN;
    ProjectUtils.begin(utils);

    // setup memory
    config.begin("nvs", &ProjectUtils.log());
    config.define("flush-water", 1.5F);
    config.define("flush-water-s", 10.0F);
    config.define("pre-water", 0.5F);
    config.define("pre-water-s", 10.0F);
    config.define("pump-delay", 2.0F);
    config.define("end-water", 1.0F);
    config.define("end-water-s", 10.0F);
    config.loadAll();

    build_home_page();

    // the control page used to be at /control
    ProjectUtils.server().on("/control", HTTP_GET,
                             [](AsyncWebServerRequest* request) { request->redirect("/"); });
}

void loop() {
    ProjectUtils.loop();
    ui_serial_comm_handler();

    if (digitalRead(PIN_BUTTON) == LOW && phase == Phase::Idle) {
        console().println("Taste gedrückt");
        flush_start();
    }
    flush_update();

    delay(20);
}

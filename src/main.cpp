#include "main.h"

#define PIN_RELAIS_PUMP                                   GPIO_NUM_17
#define PIN_RELAIS_SOLENOID                               GPIO_NUM_16
#define PIN_BUTTON                                        GPIO_NUM_19
#define PIN_SENSOR_FLOW                                   GPIO_NUM_14

#define SENSOR_LITER_PER_PULSE                               0.0033333
#define WATER_FLOW_FACTOR                                    0.75

// ------------ system variables ------------ //
double literCounter = 0;
unsigned long lastWifiCheck = 0;

// interrupt callback for the flow sensor
void IRAM_ATTR count_liters() {
  literCounter += SENSOR_LITER_PER_PULSE;
}

// ----------------- memory ----------------- //
MemoryModule config("nvs");
/*
Spülwassermenge
Pumpen-Nachlaufzeit
Nachlaufwassermenge
*/

void setup() {
  DualSerial.begin(115200);

  // configure GPIOs  
  pinMode(PIN_SENSOR_FLOW, INPUT_PULLUP);
  attachInterrupt(PIN_SENSOR_FLOW, count_liters, FALLING);

  pinMode(PIN_RELAIS_PUMP, OUTPUT);
  pinMode(PIN_RELAIS_SOLENOID, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  // initialize project utility
  project_utils_init("Neue Klospülung");

  // setup memory
  config.addParameter("flush-water", (float) 1.5);
  config.addParameter("pump-delay", (float) 2);
  config.addParameter("end-water", (float) 1.0);

  config.loadAllStrict();

  }

void loop() {
  project_utils_update();
  ui_serial_comm_handler();


  // ------------ Program ------------ //
  if (digitalRead(PIN_BUTTON) == LOW) {
    DualSerial.println("Taste gedrückt");
    
    // ziehe Werte aus dem Speicher
    float flush_water = *config.getFloat("flush-water");
    float pump_delay = *config.getFloat("pump-delay");
    float end_water = *config.getFloat("end-water");

    // setze den literzähler zurück
    literCounter = 0;

    // öffne das ventil und setze einen Zeitstempel
    digitalWrite(PIN_RELAIS_SOLENOID, HIGH);
    unsigned long time_start = millis();
    DualSerial.println("Ventil geöffnet");

    // wir warten bis 1 Liter durchgeflossen ist
    while(literCounter < 1*WATER_FLOW_FACTOR && millis() < time_start + 10000) {
      delay(20);
    }
    DualSerial.println("Zeit: " + String((millis() - time_start) / 1000.0) + " Sekunden");
    DualSerial.println("Liter: " + String(literCounter));

    // starte die pumpe
    digitalWrite(PIN_RELAIS_PUMP, HIGH);
    DualSerial.println("Pumpe gestartet");

    // wir warten bis 1 Liter durchgeflossen ist, aber das ventil soll maximal 10 Sekunden geöffnet bleiben
    while(literCounter < 2*WATER_FLOW_FACTOR && millis() < time_start + 20000) {
      delay(20);
    }
    DualSerial.println("Zeit: " + String((millis() - time_start) / 1000.0) + " Sekunden");
    DualSerial.println("Liter: " + String(literCounter));

    // schliesse das ventil
    digitalWrite(PIN_RELAIS_SOLENOID, LOW);
    DualSerial.println("Ventil geschlossen");

    // warte 2 Sekunden
    delay(2000);

    // schalte die pumpe aus
    digitalWrite(PIN_RELAIS_PUMP, LOW);
    DualSerial.println("Pumpe gestoppt");

    // öffne das ventil und setze einen Zeitstempel
    literCounter = 0;
    digitalWrite(PIN_RELAIS_SOLENOID, HIGH);
    time_start = millis();
    DualSerial.println("Ventil geöffnet");

    // wir warten bis 1 Liter durchgeflossen ist, aber das ventil soll maximal 10 Sekunden geöffnet bleiben
    while(literCounter < 1 && millis() < time_start + 10000) {
      delay(100);
    }
    DualSerial.println("Zeit: " + String((millis() - time_start) / 1000.0) + " Sekunden");
    DualSerial.println("Liter: " + String(literCounter));

    // schliesse das ventil
    digitalWrite(PIN_RELAIS_SOLENOID, LOW);
    DualSerial.println("Ventil geschlossen");

  }
  // ------------ Wifi ------------ //
  // prevent rollover problems
  if (millis() > lastWifiCheck)
    lastWifiCheck = millis();

  // regularily check wifi connection
  if(!network_manager_is_connected() && millis() - lastWifiCheck > WIFI_SEARCH_INTERVAL) {
    network_manager_wifi_connect();
    lastWifiCheck = millis();
  }


  delay(20);
}
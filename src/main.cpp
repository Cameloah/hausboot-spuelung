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


// ----------------- control ----------------- //
bool flag_flush = false;
bool flag_manual_water = false;
bool flag_manual_pump = false;
unsigned long time_manual_water = 0;
unsigned long time_manual_pump = 0;

void control_serve(AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/control.html", "text/html");
}

void control_values_get(AsyncWebServerRequest *request) {

    String payload = String(*config.getFloat("flush-water")) + "\n"
                    + String(*config.getFloat("pre-water")) + "\n"
                    + String(*config.getFloat("pump-delay")) + "\n"
                    + String(*config.getFloat("end-water"));
    
    request->send(200, "text/plain", payload);
}

void control_manual_water(AsyncWebServerRequest *request) {
    time_manual_water = millis();
    if (!flag_manual_water && !flag_flush) {
      digitalWrite(PIN_RELAIS_SOLENOID, HIGH);
      flag_manual_water = true;
      DualSerial.println("Wasser go");
    }
    request->send(200, "application/json", "{\"status\":\"water go\"}");
}

void control_manual_pump(AsyncWebServerRequest *request) {
    time_manual_pump = millis();
    if (!flag_manual_pump && !flag_flush) {
      digitalWrite(PIN_RELAIS_PUMP, HIGH);
      flag_manual_pump = true;
      DualSerial.println("Pumpe go");
    }
    request->send(200, "application/json", "{\"status\":\"pump go\"}");
}

void control_manual_off(AsyncWebServerRequest *request) {
    if (!flag_flush) {
      flag_manual_water = false;
      flag_manual_pump = false;
      digitalWrite(PIN_RELAIS_SOLENOID, LOW);
      digitalWrite(PIN_RELAIS_PUMP, LOW);
      DualSerial.println("Stopp");
    }
    request->send(200, "application/json", "{\"status\":\"stop\"}");
}

void control_flush(AsyncWebServerRequest *request) {
  if(flag_flush == false)
    flag_flush = true;

  request->send(200, "OK");
}

void control_values_post(AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
            if (p->name() == "flushWater") {
                config.set("flush-water", p->value().toFloat(), true);
                DualSerial.print("Spülwassermenge gesetzt auf [L]: ");
                DualSerial.println(p->value());
            }
            
            if (p->name() == "preWater") {
                config.set("pre-water", p->value().toFloat(), true);
                DualSerial.print("Vorspülwassermenge gesetzt auf [L]: ");
                DualSerial.println(p->value());
            }

            if (p->name() == "pumpDelay") {
                config.set("pump-delay", p->value().toFloat(), true);
                DualSerial.print("Pumpen-Verzögerung gesetzt auf [s]: ");
                DualSerial.println(p->value());
            }
    
            if (p->name() == "endWater") {
                config.set("end-water", p->value().toFloat(), true);
                DualSerial.print("Nachspülwassermenge gesetzt auf [L]: ");
                DualSerial.println(p->value());
            }
        }
    }

    config.saveAll();
    control_serve(request);
}


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
  config.addParameter("pre-water", (float) 0.5);
  config.addParameter("pump-delay", (float) 2);
  config.addParameter("end-water", (float) 1.0);

  config.loadAllStrict();

  // setup control page
  server.on("/control", HTTP_POST, control_values_post);
  server.on("/control/pull", HTTP_GET, control_values_get); 
  server.on("/control/water", HTTP_GET, control_manual_water);
  server.on("/control/pump", HTTP_GET, control_manual_pump);
  server.on("/control/off", HTTP_GET, control_manual_off);
  server.on("/control/flush", HTTP_GET, control_flush);
  server.on("/control", HTTP_GET, control_serve); 

  }

void loop() {
  project_utils_update();
  ui_serial_comm_handler();

  // ------------ Manual ------------ //
  if (flag_manual_water && millis() - time_manual_water > 1000) {
    digitalWrite(PIN_RELAIS_SOLENOID, LOW);
    flag_manual_water = false;
    DualSerial.println("Ventil Timout");
  }

  if (flag_manual_pump && millis() - time_manual_pump > 1000) {
    digitalWrite(PIN_RELAIS_PUMP, LOW);
    flag_manual_pump = false;
    DualSerial.println("Pumpe Timeout");
  }


  // ------------ Program ------------ //
  if (digitalRead(PIN_BUTTON) == LOW || flag_flush) {
    flag_flush = true;
    DualSerial.println("Taste gedrückt");
    
    // ziehe Werte aus dem Speicher
    float flush_water = *config.getFloat("flush-water");
    float pre_water = *config.getFloat("pre-water");
    float pump_delay = *config.getFloat("pump-delay");
    float end_water = *config.getFloat("end-water");

    // setze den literzähler zurück
    literCounter = 0;

    // öffne das ventil und setze einen Zeitstempel
    digitalWrite(PIN_RELAIS_SOLENOID, HIGH);
    unsigned long time_start = millis();
    DualSerial.println("Ventil geöffnet");

    // wir warten bis vorlaufmenge durchgeflossen ist
    while(literCounter < pre_water && millis() < time_start + 10000) {
      delay(20);
    }
    DualSerial.println("Zeit: " + String((millis() - time_start) / 1000.0) + " Sekunden");
    DualSerial.println("Liter: " + String(literCounter));

    // starte die pumpe
    digitalWrite(PIN_RELAIS_PUMP, HIGH);
    DualSerial.println("Pumpe gestartet");

    // wir warten bis die Spühlwassermenge durchgeflossen ist, aber das ventil soll maximal 10 Sekunden geöffnet bleiben
    while(literCounter < flush_water && millis() < time_start + 20000) {
      delay(20);
    }
    DualSerial.println("Zeit: " + String((millis() - time_start) / 1000.0) + " Sekunden");
    DualSerial.println("Liter: " + String(literCounter));

    // schliesse das ventil
    digitalWrite(PIN_RELAIS_SOLENOID, LOW);
    DualSerial.println("Ventil geschlossen");

    // warte die Pumpenverzögerung ab
    delay(pump_delay * 1000);

    // schalte die pumpe aus
    digitalWrite(PIN_RELAIS_PUMP, LOW);
    DualSerial.println("Pumpe gestoppt");

    // öffne das ventil und setze einen Zeitstempel
    literCounter = 0;
    digitalWrite(PIN_RELAIS_SOLENOID, HIGH);
    time_start = millis();
    DualSerial.println("Ventil geöffnet");

    // wir warten bis 1 Liter durchgeflossen ist, aber das ventil soll maximal 10 Sekunden geöffnet bleiben
    while(literCounter < end_water && millis() < time_start + 10000) {
      delay(100);
    }
    DualSerial.println("Zeit: " + String((millis() - time_start) / 1000.0) + " Sekunden");
    DualSerial.println("Liter: " + String(literCounter));

    // schliesse das ventil
    digitalWrite(PIN_RELAIS_SOLENOID, LOW);
    DualSerial.println("Ventil geschlossen");

    flag_flush = false;
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
#include <Arduino.h>

#define PIN_RELAIS_PUMP                                   GPIO_NUM_16
#define PIN_RELAIS_SOLENOID                               GPIO_NUM_17
#define PIN_BUTTON                                        GPIO_NUM_19
#define PIN_SENSOR_FLOW                                   GPIO_NUM_14

#define SENSOR_LITER_PER_PULSE                               0.0033333


// ------------ system variables ------------ //
double literCounter = 0;

void count_liters() {
  literCounter += SENSOR_LITER_PER_PULSE;
}


void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_SENSOR_FLOW, INPUT_PULLUP);
  attachInterrupt(PIN_SENSOR_FLOW, count_liters, FALLING);

  pinMode(PIN_RELAIS_PUMP, OUTPUT);
  pinMode(PIN_RELAIS_SOLENOID, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  }

void loop() {

  // ------------ Program ------------ //
  if (digitalRead(PIN_BUTTON) == LOW) {
    Serial.println("Button pressed");
    

    // setze den literzähler zurück
    literCounter = 0;

    // öffne das ventil und setze einen Zeitstempel
    digitalWrite(PIN_RELAIS_SOLENOID, HIGH);
    unsigned long time_start = millis();
    Serial.println("Valve opened");

    // warte 2 Sekunden
    delay(2000);

    // starte die pumpe
    digitalWrite(PIN_RELAIS_PUMP, HIGH);
    Serial.println("Pump started");

    // wir warten bis 1 Liter durchgeflossen ist, aber das ventil soll maximal 10 Sekunden geöffnet bleiben
    while(literCounter < 1 && millis() < time_start + 10000)
      delay(100);

    // schliesse das ventil
    digitalWrite(PIN_RELAIS_SOLENOID, LOW);
    Serial.println("Valve closed");

    // warte 2 Sekunden
    delay(2000);

    // schalte die pumpe aus
    digitalWrite(PIN_RELAIS_PUMP, LOW);
    Serial.println("Pump stopped");

  }
}
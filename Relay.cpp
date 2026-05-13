#include <Arduino.h>
#include "Relay.h"

#define RELAY_PIN 21

static bool relayState = false;

void relayBegin() {
  pinMode(RELAY_PIN, OUTPUT);
  relayState = false;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("[RELAY] Initialized relay output to OFF (LOW)");
}

void relayOn() {
  relayState = true;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("[RELAY] GPIO" + String(RELAY_PIN) + " set to ON (HIGH)");
}

void relayOff() {
  relayState = false;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("[RELAY] GPIO" + String(RELAY_PIN) + " set to OFF (LOW)");
}

bool relayIsOn() {
  return relayState;
}
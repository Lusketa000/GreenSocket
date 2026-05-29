#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <time.h>

#define MIN_VALID_YEAR 120

void mqttSetup(const String& deviceId);
void mqttLoop();
bool isTimeValid(const tm &timeinfo);

#endif

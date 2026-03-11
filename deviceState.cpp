// código de teste!!!! cuidado!!!! 
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

enum DeviceState {
    ACTIVE,
    STANDBY
};

const float currentThreshold = 0.5;

DeviceState deviceState(float current) {
    if (current > currentThreshold) {
        return ACTIVE;
    } else {
        return STANDBY;
    }
}

void setup() {
    Serial.begin(115200);
}

void loop() {
    float current = 0.9; //alguma corrente que a gente vai ler
    
    DeviceState state = deviceState(current);
    if (state == ACTIVE) {
        Serial.println("Dispositivo está ligado");
    } else {
        Serial.println("Dispositivo está em desligado, mas consumindo energia");
    }

    delay(1000);
}
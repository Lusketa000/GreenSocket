// código de teste!!!! cuidado!!!! ⚠️⚠️☢️⚠️⚠️ 
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

bool isDeviceActive = false;

float current() {
    // Função para capturar o valor de corrente do dispositivo
    // Substituir esta função pela que captura o valor de corrente do dispositivo
    return 0.9;
};

float currentThreshold() {
    // Função para capturar o limiar do dispositivo
    // Substituir esta função pela que captura o limiar do dispositivo
    return 0.5;
};

bool deviceState(float current) {
    if (current > currentThreshold()) {
        return isDeviceActive = true;
    } else {
        return isDeviceActive = false;
    }
}

void setup() {
    Serial.begin(115200);
}

void loop() {    
    bool isActive = deviceState(current());
    if (!isActive) {
        Serial.println("Dispositivo está ligado");
    } else {
        Serial.println("Dispositivo em standby");
    }

    delay(1000);
}
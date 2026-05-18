#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "WifiSetup.h"

bool wifiSetupBegin(const char* apName) {
  Serial.println("[WIFI] Starting WiFi initialization...");
  
  // First, completely turn off WiFi
  WiFi.mode(WIFI_OFF);
  delay(500);
  
  // Power on WiFi radio
  esp_wifi_start();
  delay(500);
  
  Serial.println("[WIFI] Setting WiFi mode to AP...");
  WiFi.mode(WIFI_AP);
  delay(500);
  
  // Configure AP with more parameters
  Serial.println("[WIFI] Configuring WiFi AP...");
  
  wifi_config_t ap_config = {};
  strcpy((char*)ap_config.ap.ssid, apName);
  strcpy((char*)ap_config.ap.password, "12345678");
  ap_config.ap.ssid_len = strlen(apName);
  ap_config.ap.channel = 1;
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  
  esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  if (err != ESP_OK) {
    Serial.print("[WIFI] Failed to set config: ");
    Serial.println(esp_err_to_name(err));
    return false;
  }
  
  delay(100);
  
  // Try using native Arduino function one more time
  bool res = WiFi.softAP(apName, "12345678");
  
  if (!res) {
    Serial.println("[WIFI] WiFi.softAP() failed, trying manual start...");
    err = esp_wifi_start();
    if (err != ESP_OK) {
      Serial.print("[WIFI] esp_wifi_start failed: ");
      Serial.println(esp_err_to_name(err));
      return false;
    }
    delay(500);
  }

  Serial.println("[WIFI] ✓ Access Point created successfully!");
  Serial.print("[WIFI] SSID: ");
  Serial.println(apName);
  Serial.println("[WIFI] Password: 12345678");
  
  IPAddress ip = WiFi.softAPIP();
  Serial.print("[WIFI] IP Address: ");
  Serial.println(ip);
  
  Serial.print("[WIFI] Stations connected: ");
  Serial.println(WiFi.softAPgetStationNum());

  Serial.println("[TIME] Time synchronization disabled (AP mode)");
  Serial.println("[TIME] Use web interface to set time manually");

  // Try to start mDNS (optional)
  MDNS.begin("esp");
  Serial.println("[MDNS] mDNS advertiser started");

  return true;
}

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <time.h>
#include "WifiSetup.h"

bool wifiSetupBegin(const char* apName, int timeoutSeconds) {
  WiFi.mode(WIFI_STA);
  Serial.println("[WIFI] Mode set to WIFI_STA");

  WiFiManager wifiMan;
  wifiMan.setConnectTimeout(timeoutSeconds);
  Serial.print("[WIFI] Trying autoConnect with timeout (s): ");
  Serial.println(timeoutSeconds);

  bool res = wifiMan.autoConnect(apName);
  if (!res) {
    Serial.println("Falha na conexao!");
    return false;
  }

  Serial.println("WiFi connected.");
  Serial.print("[WIFI] SSID: ");
  Serial.println(WiFi.SSID());
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("[TIME] NTP configured with GMT-3");

  if (MDNS.begin("esp")) {
    Serial.println("[MDNS] mDNS started at esp.local");
  } else {
    Serial.println("[MDNS] Failed to start mDNS");
  }

  Serial.println(WiFi.localIP());
  return true;
}

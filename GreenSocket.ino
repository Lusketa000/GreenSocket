#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "ACS712.h"
#include "Memory.h"
#include "Relay.h"
#include "WifiSetup.h"

#define MEASUREMENT_INTERVAL 30000 // 30 segundos
#define TIME_CHECK_INTERVAL 60000 // 1 minuto
#define SAVING_INTERVAL 1800000 // 30 minutos
#define ROUTINE_CHECK_INTERVAL 86400000 // 1 dia
#define BIN_SIZE 40
#define MAX_VALUE 4095
#define NUM_BINS (MAX_VALUE / BIN_SIZE + 1)
#define SENSOR_PIN 1
#define WIFI_TIMEOUT_SECONDS 10
#define MINUTES_PER_DAY 1440
#define SLOT_MINUTES 30
#define MIN_VALID_YEAR 120

ACS712 ACS(SENSOR_PIN, 3.3, 4095, 123.33);

unsigned long measurement_timer = 0;
unsigned long time_check_timer = 0;
unsigned long saving_timer = 0;
unsigned long routine_timer = 0;

time_t manual_on_timestamp = -1;
time_t manual_off_timestamp = -1;
bool auto_schedule[COLS] = {false};

enum Modes {
  MANUAL,
  AUTO
};

Modes mode = MANUAL;

int standby = 0;
int16_t reading = 0;
int16_t max_reading = 0;
bool pending_off = false;

WebServer server(80);

static bool hasManualSchedule() {
  return manual_on_timestamp >= 0 && manual_off_timestamp >= 0;
}

static bool isTimeValid(const tm &timeinfo) {
  return timeinfo.tm_year >= MIN_VALID_YEAR;
}

static int minutesToSlot(int minutes) {
  if (minutes < 0) {
    return 0;
  }
  int slot = minutes / SLOT_MINUTES;
  if (slot >= COLS) {
    return COLS - 1;
  }
  return slot;
}

void handleRelayOn() {
  Serial.println("[HTTP] /relay/on requested");
  relayOn();
  handleRoot();
}

void handleRelayOff() {
  Serial.println("[HTTP] /relay/off requested");
  relayOff();
  handleRoot();
}

void handleModeManual() {
  Serial.println("[HTTP] Switching mode to MANUAL");
  mode = MANUAL;
  handleRoot();
}

void handleModeAuto() {
  Serial.println("[HTTP] Switching mode to AUTO");
  mode = AUTO;
  handleRoot();
}

void handleRoot() {
  Serial.println("[HTTP] Serving /");
  bool relay_on = relayIsOn();
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 20px; margin: 10px; cursor: pointer;}";
  html += "</style></head>";
  html += "<body><h1>ESP32 ScheduleTimer</h1>";
  html += "<form action=\"/set\" method=\"GET\">";
  html += "<p>Turn ON time:</p>";
  html += "<input type=\"time\" name=\"on_time\" required>";
  html += "<p>Turn OFF time:</p>";
  html += "<input type=\"time\" name=\"off_time\" required>";
  html += "<br><br><input type=\"submit\" class=\"button\" value=\"Set ScheduleTime\">";
  html += "</form>";

  html += "<p>Relay - State ";
  html += relay_on ? "LIGADO" : "DESLIGADO";
  html += "</p>";
  if (relay_on) {
    html += "<p><a href=\"/relay/off\"><button class=\"button\">OFF</button></a></p>";
  } else {
    html += "<p><a href=\"/relay/on\"><button class=\"button button2\">ON</button></a></p>";
  }
  html += "<p>Mode: " + String(mode == MANUAL ? "MANUAL" : "AUTO") + "</p>";
  if (mode == MANUAL) {
    html += "<p><a href=\"/mode/auto\"><button class=\"button button2\">Switch to AUTO</button></a></p>";
  } else {
    html += "<p><a href=\"/mode/manual\"><button class=\"button button2\">Switch to MANUAL</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSet() {
  Serial.println("[HTTP] /set requested");
  if (server.hasArg("on_time") && server.hasArg("off_time")) {
    String str_on = server.arg("on_time");
    String str_off = server.arg("off_time");

    Serial.print("[SCHEDULE] Received on_time=");
    Serial.print(str_on);
    Serial.print(" off_time=");
    Serial.println(str_off);

    int on_hour = str_on.substring(0, 2).toInt();
    int on_min = str_on.substring(3, 5).toInt();
    int off_hour = str_off.substring(0, 2).toInt();
    int off_min = str_off.substring(3, 5).toInt();

    if (on_hour < 0 || on_hour > 23 || off_hour < 0 || off_hour > 23 ||
        on_min < 0 || on_min > 59 || off_min < 0 || off_min > 59) {
      Serial.println("[SCHEDULE] Invalid time received; ignoring update");
      handleRoot();
      return;
    }

    int ini = on_hour * 60 + on_min;
    int end = off_hour * 60 + off_min;
    if (ini == end) {
      Serial.println("[SCHEDULE] on_time equals off_time; ignoring update");
      handleRoot();
      return;
    }

    manual_on_timestamp = ini;
    manual_off_timestamp = end;

    Serial.print("Manual start minute set: ");
    Serial.println(ini);
    Serial.print("Manual end minute set: ");
    Serial.println(end);
  } else {
    Serial.println("[SCHEDULE] Missing on_time/off_time args");
  }
  handleRoot();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  ACS.suppressNoise(true);
  memoryBegin();
  relayBegin();
  wifiSetupBegin("ESP32C3-Setup", WIFI_TIMEOUT_SECONDS);
  server.on("/", handleRoot);
  server.on("/relay/on", handleRelayOn);
  server.on("/relay/off", handleRelayOff);
  server.on("/mode/manual", handleModeManual);
  server.on("/mode/auto", handleModeAuto);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("[BOOT] Setup complete");
}

void loop() {
  // Measurement
  if (millis() - measurement_timer >= MEASUREMENT_INTERVAL) {
    Serial.println("[LOOP] Measurement interval reached");
    reading = ACS.readSensor();
    max_reading = reading > max_reading ? reading : max_reading;
    Serial.print("[MEASUREMENT] reading=");
    Serial.print(reading);
    Serial.print(" max_reading=");
    Serial.println(max_reading);
    measurement_timer = millis();
  }

  // Routine check
  if (millis() - routine_timer >= ROUTINE_CHECK_INTERVAL) {
    Serial.println("[LOOP] Routine interval reached; rebuilding standby and auto schedule");
    int hist[NUM_BINS] = {0};

    for (int i = 0; i < ROWS; i++) {
      for (int j = 0; j < COLS; j++) {
        int v = readReading(i, j);
        if (v <= 10) {
          continue;
        }
        int bin = v / BIN_SIZE;
        if (bin >= NUM_BINS) {
          continue;
        }
        hist[bin]++;
      }
    }

    int maxCount = 0;
    int bestBin = 0;

    for (int i = 0; i < NUM_BINS; i++) {
      if (hist[i] > maxCount) {
        maxCount = hist[i];
        bestBin = i;
      }
    }

    standby = bestBin * BIN_SIZE + BIN_SIZE - 1;
    Serial.print("[TIME CHECK] Histogram bestBin=");
    Serial.print(bestBin);
    Serial.print(" count=");
    Serial.print(maxCount);
    Serial.print(" standby=");
    Serial.println(standby);

    int activeSlots = 0;
    for (int i = 0; i < COLS; i++) {
      auto_schedule[i] = false;
      for (int j = 0; j < ROWS; j++) {
        int currentReading = readReading(j, i);
        if (currentReading > standby + 50) {
          auto_schedule[i] = true;
        }
      }
      if (auto_schedule[i]) {
        activeSlots++;
      }
    }

    Serial.print("[ROUTINE] Active auto slots=");
    Serial.print(activeSlots);
    Serial.print("/");
    Serial.println(COLS);

    routine_timer = millis();
  }

  // Time check
  if (millis() - time_check_timer >= TIME_CHECK_INTERVAL) {
    Serial.println("[LOOP] Time check interval reached");

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if (!isTimeValid(timeinfo)) {
      Serial.println("[TIME] Clock not synchronized yet; skipping time check");
      time_check_timer = millis();
      return;
    }

    int now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    bool inInterval = false;
    if (mode == MANUAL) {
      if (hasManualSchedule()) {
        inInterval = ((now_min - manual_on_timestamp + MINUTES_PER_DAY) % MINUTES_PER_DAY) <
                     ((manual_off_timestamp - manual_on_timestamp + MINUTES_PER_DAY) % MINUTES_PER_DAY);
      }
    } else if (mode == AUTO) {
      inInterval = auto_schedule[minutesToSlot(now_min)];
    }

    Serial.print("[TIME] now=");
    Serial.print(timeinfo.tm_hour);
    Serial.print(":");
    if (timeinfo.tm_min < 10) {
      Serial.print("0");
    }
    Serial.print(timeinfo.tm_min);
    Serial.print(" mode=");
    Serial.print(mode == MANUAL ? "MANUAL" : "AUTO");
    Serial.print(" slot=");
    Serial.print(minutesToSlot(now_min));
    Serial.print(" inInterval=");
    Serial.print(inInterval ? "true" : "false");
    Serial.print(" outputState=");
    Serial.print(relayIsOn() ? "ON" : "OFF");
    Serial.print(" reading=");
    Serial.print(reading);
    Serial.print(" standby=");
    Serial.println(standby);

    if (inInterval) {
      if (relayIsOn()) {
        if (reading > standby && mode == AUTO) {
          if (!pending_off) {
            Serial.println("Desligamento pendente: consumo acima do standby.");
          }
          pending_off = true;
        } else {
          Serial.println("desligando");
          handleRelayOff();
          pending_off = false;
        }
      } else {
        pending_off = false;
      }
    } else {
      if (!relayIsOn()) {
        Serial.println("ligando");
        handleRelayOn();
      }
      pending_off = false;
    }

    time_check_timer = millis();
  }

  // Saving
  if (millis() - saving_timer >= SAVING_INTERVAL) {
    Serial.println("[LOOP] Saving interval reached");
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if (!isTimeValid(timeinfo)) {
      Serial.println("[TIME] Clock not synchronized yet; skipping save");
      saving_timer = millis();
      return;
    }

    int now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int slot = timeinfo.tm_hour * 2 + timeinfo.tm_min / 30;
    Serial.print("[SAVE] wday=");
    Serial.print(timeinfo.tm_wday);
    Serial.print(" hour=");
    Serial.print(timeinfo.tm_hour);
    Serial.print(" min=");
    Serial.print(timeinfo.tm_min);
    Serial.print(" now_min=");
    Serial.print(now_min);
    Serial.print(" slot=");
    Serial.print(slot);
    Serial.print(" max_reading=");
    Serial.println(max_reading);

    saveReading(timeinfo.tm_wday, slot, max_reading);
    max_reading = 0;
    Serial.println("[SAVE] max_reading reset to 0");
    saving_timer = millis();
  }

  handleSerialDebugCommands();
  server.handleClient();
  delay(100);
}

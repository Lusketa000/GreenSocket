#include <WiFi.h>
#include <WebServer.h>
#include <stdint.h>
#include <ESPmDNS.h>
#include <WiFiManager.h> 
#include <time.h>
#include <Preferences.h>
#include "ACS712.h"

#define MEASUREMENT_INTERVAL 30000
#define TIME_CHECK_INTERVAL 60000
#define SAVING_INTERVAL 1800000
#define TIMEOUT 10
//#define ROUTINE_CHECK_INTERVAL 864000000
#define ROUTINE_CHECK_INTERVAL 20000
#define BIN_SIZE 40
#define MAX_VALUE 4095
#define NUM_BINS (MAX_VALUE / BIN_SIZE + 1)

#define ROWS 7
#define COLS 48


Preferences prefs;

ACS712 ACS(1, 3.3, 4095, 123.33);

unsigned long measurement_timer = 0;
unsigned long time_check_timer = 0;
unsigned long saving_timer = 0;
unsigned long routine_timer = 0;

int16_t max_reading = 0;

time_t manual_on_timestamp = -1;
time_t manual_off_timestamp = -1;
bool auto_schedule[48] = {false};

enum Modes {
  MANUAL,
  AUTO
};

Modes mode = MANUAL;

int standby = 0;
int16_t reading = 0;
bool pending_off = false;

int16_t read_sensor() {
  Serial.println("[SENSOR] Starting sensor acquisition");
  ACS.autoMidPoint(30, 2);
  int16_t current_mA = ACS.mA_AC_sampling(60, 4);
  current_mA = current_mA * 0.4933 - 32.368;
  if (current_mA < 0) current_mA = 0;
  Serial.print("[SENSOR] Current (mA): ");
  Serial.println(current_mA);
  return current_mA;
}

// Função para salvar uma leitura
void saveReading(int r, int c, int valor) {
  int indice = (r * COLS) + c; // Transforma 2D em 1D
  char chave[6];
  itoa(indice, chave, 10); // Converte o número do índice em string (ex: "335")

  Serial.print("[NVS] Saving reading at row=");
  Serial.print(r);
  Serial.print(" col=");
  Serial.print(c);
  Serial.print(" key=");
  Serial.print(chave);
  Serial.print(" value=");
  Serial.println(valor);
  prefs.putShort(chave, valor);
}

int readReading(int r, int c) {
  int indice = (r * COLS) + c;
  char chave[6];
  itoa(indice, chave, 10);

  // O segundo parâmetro (0) é o valor retornado caso a chave ainda não exista
  return prefs.getShort(chave, 0);
}

void debugPrintNVSMemory() {
  Serial.println("[NVS] Dump start");
  for (int r = 0; r < ROWS; r++) {
    Serial.print("[NVS] row=");
    Serial.print(r);
    Serial.print(" values=");
    for (int c = 0; c < COLS; c++) {
      Serial.print(readReading(r, c));
      if (c < COLS - 1) {
        Serial.print(',');
      }
    }
    Serial.println();
  }
  Serial.println("[NVS] Dump end");
}

void clearNVSMemory() {
  Serial.println("[NVS] Clearing namespace 'matrix'");
  prefs.clear();
  Serial.println("[NVS] Namespace cleared");
}

void handleSerialDebugCommands() {
  static String serialBuffer;

  while (Serial.available() > 0) {
    char incoming = Serial.read();
    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      serialBuffer.trim();
      serialBuffer.toLowerCase();

      if (serialBuffer == "dump" || serialBuffer == "dumpnvs") {
        debugPrintNVSMemory();
      } else if (serialBuffer == "clear" || serialBuffer == "clearnvs" || serialBuffer == "zero") {
        clearNVSMemory();
      } else if (serialBuffer.length() > 0) {
        Serial.print("[SERIAL] Unknown command: ");
        Serial.println(serialBuffer);
        Serial.println("[SERIAL] Available commands: dump, clear");
      }

      serialBuffer = "";
      continue;
    }

    serialBuffer += incoming;
  }
}

// Assign output variables to GPIO pins
const int output32 = 21;

bool outputState = false;

// Create a web server object
WebServer server(80);

// Function to handle turning GPIO 32 on
void handleGPIO32On() {
  Serial.println("[HTTP] /32/on requested");
  outputState = true;
  digitalWrite(output32, HIGH);
  Serial.println("[RELAY] GPIO32 set to ON (HIGH)");
  handleRoot();
}

// Function to handle turning GPIO 32 off
void handleGPIO32Off() {
  Serial.println("[HTTP] /32/off requested");
  outputState = false;
  digitalWrite(output32, LOW);
  Serial.println("[RELAY] GPIO32 set to OFF (LOW)");
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

// Function to handle the root URL and show the current states
void handleRoot() {
  Serial.println("[HTTP] Serving /");
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
  html += "</body></html>";

  // Display GPIO 32 controls
  html += "<p>GPIO 32 - State " ;
  html += outputState ? "LIGADO" : "DESLIGADO";  
  html += "</p>";
  if (outputState) {
    html += "<p><a href=\"/32/off\"><button class=\"button\">OFF</button></a></p>";
  } else {
    html += "<p><a href=\"/32/on\"><button class=\"button button2\">ON</button></a></p>";
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

    uint8_t on_hour = str_on.substring(0, 2).toInt();
    uint8_t on_min = str_on.substring(3, 5).toInt();
    uint8_t off_hour = str_off.substring(0, 2).toInt();
    uint8_t off_min = str_off.substring(3, 5).toInt();

      int ini = on_hour * 60 + on_min;
      int end = off_hour * 60 + off_min;
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

/*void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %hour:%min:%S");
  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%hour", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println();
}*/

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[BOOT] Setup start");

  ACS.suppressNoise(true);
  Serial.println("[BOOT] ACS noise suppression enabled");

  prefs.begin("matrix", false);
  Serial.println("[BOOT] Preferences namespace 'matrix' opened");
  Serial.println("[BOOT] Serial debug commands: dump, clear");

  // Initialize the output variables as outputs
  pinMode(output32, OUTPUT);
  // Set outputs to off
  digitalWrite(output32, LOW);
  Serial.println("[BOOT] GPIO32 initialized as OUTPUT and set OFF (LOW)");

  WiFi.mode(WIFI_STA); //esp em modo de estacao
  Serial.println("[WIFI] Mode set to WIFI_STA");
  WiFiManager wifiMan; //cria um objeto WiFiManager
  wifiMan.setConnectTimeout(TIMEOUT);
  Serial.print("[WIFI] Trying autoConnect with timeout (s): ");
  Serial.println(TIMEOUT);
  bool res = wifiMan.autoConnect("ESP32C3-Setup"); //caso nao consiga se conectar a ultima rede salva, cria a propria rede de configuração
if(!res)
{
  Serial.println("Falha na conexão!");
}
else
{
  // Connect to Wi-Fi network
  Serial.println("WiFi connected.");
  Serial.print("[WIFI] SSID: ");
  Serial.println(WiFi.SSID());
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("[TIME] NTP configured with GMT-3");
  Serial.println("IP address: ");
  IPAddress local_IP(192,168,1,200);

  if (MDNS.begin("esp")) {
    Serial.println("[MDNS] mDNS started at esp.local");
  } else {
    Serial.println("[MDNS] Failed to start mDNS");
  }

  Serial.println(WiFi.localIP());
}
  // Set up the web server to handle different routes
  Serial.println("[HTTP] Registering routes");
  server.on("/", handleRoot);
  server.on("/32/on", handleGPIO32On);
  server.on("/32/off", handleGPIO32Off);
  server.on("/mode/manual", handleModeManual);
  server.on("/mode/auto", handleModeAuto);

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");

  server.on("/set", handleSet);
  Serial.println("[BOOT] Setup complete");
}

void loop() {
  if (millis() - measurement_timer >= MEASUREMENT_INTERVAL) {
    Serial.println("[LOOP] Measurement interval reached");
    reading = read_sensor();
    max_reading = reading > max_reading ? reading : max_reading;
    Serial.print("[LOOP] reading=");
    Serial.print(reading);
    Serial.print(" max_reading=");
    Serial.println(max_reading);
    measurement_timer = millis();
  }

  if(millis() - routine_timer >=  ROUTINE_CHECK_INTERVAL){
    Serial.println("[LOOP] Routine interval reached; rebuilding standby and auto schedule");
    int hist[NUM_BINS] = {0};

    // 1. Preencher histograma
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            int v = readReading(i, j);
            if (v <= 10) continue; // ignora ruído
            int bin = v / BIN_SIZE;
            if (bin >= NUM_BINS) continue;
            hist[bin]++;
        }
    }

    // 2. Encontrar bin mais frequente
    int maxCount = 0;
    int bestBin = 0;

    for (int i = 0; i < NUM_BINS; i++) {
        if (hist[i] > maxCount) {
            maxCount = hist[i];
            bestBin = i;
        }
    }

    // 3. Converter bin para valor representativo
    standby = bestBin * BIN_SIZE + BIN_SIZE - 1;
    Serial.print("[ROUTINE] Histogram bestBin=");
    Serial.print(bestBin);
    Serial.print(" count=");
    Serial.print(maxCount);
    Serial.print(" standby=");
    Serial.println(standby);

    // TODO: Usar o valor de standby para ajustar o horário automático

    int activeSlots = 0;
    for (int i = 0; i < COLS; i++) {
      auto_schedule[i] = false;
      for (int j = 0; j < ROWS; j++) {
        int currentReading = readReading(j, i);
        if (currentReading > standby + 50) {
          auto_schedule[i] = true;
        }
      }
      if (auto_schedule[i]) activeSlots++;
    }

    Serial.print("[ROUTINE] Active auto slots=");
    Serial.print(activeSlots);
    Serial.print("/");
    Serial.println(COLS);

    routine_timer = millis();
  }

  if (millis() - time_check_timer >= TIME_CHECK_INTERVAL) {
    Serial.println("[LOOP] Time check interval reached");
    // Verifica se é hora de ligar/desligar automaticamente.
    // Se for para desligar, só desliga quando a leitura estiver <= standby.
    
    // Handle incoming client requests
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    bool inInterval = 0;
    if (mode == MANUAL) {
      inInterval = ((now_min - manual_on_timestamp + 1440) % 1440) < ((manual_off_timestamp - manual_on_timestamp + 1440) % 1440);
    }
    else if (mode == AUTO) {
      inInterval = auto_schedule[now_min / 30];
    }

    Serial.print("[TIME] now=");
    Serial.print(timeinfo.tm_hour);
    Serial.print(":");
    if (timeinfo.tm_min < 10) Serial.print("0");
    Serial.print(timeinfo.tm_min);
    Serial.print(" mode=");
    Serial.print(mode == MANUAL ? "MANUAL" : "AUTO");
    Serial.print(" slot=");
    Serial.print(now_min / 30);
    Serial.print(" inInterval=");
    Serial.print(inInterval ? "true" : "false");
    Serial.print(" outputState=");
    Serial.print(outputState ? "ON" : "OFF");
    Serial.print(" reading=");
    Serial.print(reading);
    Serial.print(" standby=");
    Serial.println(standby);

    if(inInterval) {
      if(outputState) {
        if (reading > standby && mode == AUTO) {
          if (!pending_off) {
            Serial.println("Desligamento pendente: consumo acima do standby.");
          }
          pending_off = true;
        } else {
          Serial.println("desligando");
          handleGPIO32Off();
          pending_off = false;
        }
      } else {
        pending_off = false;
      }
    }
    else {
      if(!outputState) {
        Serial.println("ligando");
        handleGPIO32On();
      }
      pending_off = false;
    }
    
    time_check_timer = millis();
  }

  if (millis() - saving_timer >= SAVING_INTERVAL) {
    Serial.println("[LOOP] Saving interval reached");
    // readings[dia_da_semana(0 a 6)][hora_do_dia(0 a 47)] = max_reading;
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
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
    
    // Passar para memória não volátil
    saveReading(timeinfo.tm_wday, slot, max_reading);

    max_reading = 0;
    Serial.println("[SAVE] max_reading reset to 0");
    saving_timer = millis();
  }


    handleSerialDebugCommands();

  // Delay o menor intervalo (talvez não, pra conectar o wifi)
  // delay(MEASUREMENT_INTERVAL); // TODO: Deep sleep  
  server.handleClient();
  delay(100);
}

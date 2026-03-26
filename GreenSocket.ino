#include <WiFi.h>
#include <WebServer.h>
#include <stdint.h>
#include <ESPmDNS.h>
#include <WiFiManager.h> 
#include <time.h>
#include "ACS712.h"
#include <Preferences.h>

#define MEASUREMENT_INTERVAL 30000
#define TIME_CHECK_INTERVAL 60000
#define SAVING_INTERVAL 1800000
#define TIMEOUT 10

Preferences prefs;

ACS712 ACS(33, 3.3, 4095, 123.33);

unsigned long measurement_timer = 0;
unsigned long time_check_timer = 0;
unsigned long saving_timer = 0;
int16_t max_reading = 0;

int16_t read_sensor() {
  ACS.autoMidPoint(30, 2);
  int16_t current_mA = ACS.mA_AC_sampling(60, 4);
  current_mA = current_mA * 0.4933 - 32.368;
  if (current_mA < 0) current_mA = 0;
  Serial.println(current_mA);
  return current_mA;
}

// Função para salvar uma leitura
void saveReading(int r, int c, int valor) {
  int indice = (r * 7) + c; // Transforma 2D em 1D
  char chave[6];
  itoa(indice, chave, 10); // Converte o número do índice em string (ex: "335")

  prefs.putInt(chave, valor);
}

int readReading(int r, int c) {
  int indice = (r * 7) + c;
  char chave[6];
  itoa(indice, chave, 10);

  // O segundo parâmetro (0) é o valor retornado caso a chave ainda não exista
  return prefs.getInt(chave, 0);
}

typedef struct{
  uint8_t hour;
  uint8_t min;
} ScheduleTime;

time_t startTimestamp = -1;
time_t endTimestamp = -1;

// Assign output variables to GPIO pins
const int output32 = 32;

bool outputState = false;

// Create a web server object
WebServer server(80);

// Function to handle turning GPIO 32 on
void handleGPIO32On() {
  outputState = true;
  digitalWrite(output32, LOW);
  handleRoot();
}

// Function to handle turning GPIO 32 off
void handleGPIO32Off() {
  outputState = false;
  digitalWrite(output32, HIGH);
  handleRoot();
}

// Function to handle the root URL and show the current states
void handleRoot() {
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

  html += "</body></html>";
  server.send(200, "text/html", html);
}

ScheduleTime on;
ScheduleTime off;


void handleSet() {
  if (server.hasArg("on_time") && server.hasArg("off_time")) {
    String str_on = server.arg("on_time");
    String str_off = server.arg("off_time");

    on.hour = str_on.substring(0, 2).toInt();
    on.min = str_on.substring(3, 5).toInt();

    off.hour = str_off.substring(0, 2).toInt();
    off.min = str_off.substring(3, 5).toInt();

      int ini = on.hour * 60 + on.min;
      int end = off.hour * 60 + off.min;
      startTimestamp = ini;
      endTimestamp   = end;

      Serial.print("Start minute: ");
      Serial.println(ini);

      Serial.print("End minute: ");
      Serial.println(end);
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
  ACS.suppressNoise(true);
  
  Serial.begin(115200);

  prefs.begin("matrix", false);

  // Initialize the output variables as outputs
  pinMode(output32, OUTPUT);
  // Set outputs to off
  digitalWrite(output32, HIGH);

  WiFi.mode(WIFI_STA); //esp em modo de estacao
  WiFiManager wifiMan; //cria um objeto WiFiManager
  wifiMan.setConnectTimeout(TIMEOUT);
  bool res = wifiMan.autoConnect("ESP32C3-Setup"); //caso nao consiga se conectar a ultima rede salva, cria a propria rede de configuração
if(!res)
{
  Serial.println("Falha na conexão!");
}
else
{
  // Connect to Wi-Fi network
  Serial.println("WiFi connected.");
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("IP address: ");
  IPAddress local_IP(192,168,1,200);

  MDNS.begin("esp");

  Serial.println(WiFi.localIP());
}
  // Set up the web server to handle different routes
  server.on("/", handleRoot);
  server.on("/32/on", handleGPIO32On);
  server.on("/32/off", handleGPIO32Off);

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");

  server.on("/set", handleSet);
}

int timeToMinutes(String t) {
  int hours = t.substring(0, 2).toInt();
  int minutes = t.substring(3, 5).toInt();
  return hours * 60 + minutes;
}
void loop() {
  if (millis() - measurement_timer >= MEASUREMENT_INTERVAL) {
    int16_t reading = read_sensor();
    max_reading = reading > max_reading ? reading : max_reading;
    measurement_timer = millis();
  }

  if (millis() - time_check_timer >= TIME_CHECK_INTERVAL) {
    // TODO: Ver se é hora de ligar/desligar automaticamente
    
    // Handle incoming client requests
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    bool inInterval = ((now_min - startTimestamp + 1440) % 1440) < ((endTimestamp - startTimestamp + 1440) % 1440);

    if(inInterval) {
      if(outputState) {
          Serial.println("deligando");
          handleGPIO32Off();
      }
    }
    else {
      if(!outputState) {
        Serial.println("ligando");
        handleGPIO32On();
      }
    }
    
    time_check_timer = millis();
  }

  if (millis() - saving_timer >= SAVING_INTERVAL) {
    // readings[dia_da_semana(0 a 6)][hora_do_dia(0 a 47)] = max_reading;
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // Passar para memória não volátil
    saveReading(timeinfo.tm_wday, timeinfo.tm_hour * 2 + timeinfo.tm_min / 30, max_reading);

    max_reading = 0;
    saving_timer = millis();
  }



  // Delay o menor intervalo (talvez não, pra conectar o wifi)
  // delay(MEASUREMENT_INTERVAL); // TODO: Deep sleep  
  server.handleClient();
  delay(100);
}

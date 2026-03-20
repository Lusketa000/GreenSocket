#include <WiFi.h>
#include <WebServer.h>
#include <stdint.h>
#include <ESPmDNS.h>
#include <WiFiManager.h> 
#include <time.h>
#include "ACS712.h"

#define MEASUREMENT_INTERVAL 30000
#define TIME_CHECK_INTERVAL 60000
#define SAVING_INTERVAL 1800000

ACS712 ACS(33, 3.3, 4095, 123.33);

unsigned long measurement_timer = 0;
unsigned long time_check_timer = 0;
unsigned long saving_timer = 0;
int16_t max_reading = 0;
int16_t readings[7][48]; // TODO: Passar para memória não volátil

int16_t read_sensor() {
  int16_t current_mA = ACS.mA_AC_sampling(60, 4);
  current_mA = current_mA * 0.4933 - 32.368;
  if (current_mA < 0) current_mA = 0;
  return current_mA;
}

typedef struct{
  uint8_t H;
  uint8_t M;
}Tempo;

time_t startTimestamp = -1;
time_t endTimestamp = -1;

// Assign output variables to GPIO pins
const int output32 = 8;

bool outputState = false;

// Create a web server object
WebServer server(80);

// Function to handle turning GPIO 32 on
void handleGPIO32On() {
  outputState = true;
  digitalWrite(output32, HIGH);
  handleRoot();
}

// Function to handle turning GPIO 32 off
void handleGPIO32Off() {
  outputState = false;
  digitalWrite(output32, LOW);
  handleRoot();
}

// Function to handle the root URL and show the current states
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 20px; margin: 10px; cursor: pointer;}";
  html += "</style></head>";
  html += "<body><h1>ESP32 Scheduler</h1>";
  html += "<form action=\"/set\" method=\"GET\">";
  html += "<p>Turn ON time:</p>";
  html += "<input type=\"time\" name=\"on_time\" required>";
  html += "<p>Turn OFF time:</p>";
  html += "<input type=\"time\" name=\"off_time\" required>";
  html += "<br><br><input type=\"submit\" class=\"button\" value=\"Set Schedule\">";
  html += "</form>";
  html += "</body></html>";

  // Display GPIO 32 controls
  html += "<p>GPIO 32 - State " ;
  html += outputState ? "LIGADO" : "DESLIGADO";  
  html += "</p>";
  if (outputState) {
    html += "<p><a href=\"/32/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/32/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

Tempo on;
Tempo off;


void handleSet() {
  if (server.hasArg("on_time") && server.hasArg("off_time")) {
    String str_on = server.arg("on_time");
    String str_off = server.arg("off_time");

    on.H = str_on.substring(0, 2).toInt();
    on.M = str_on.substring(3, 5).toInt();

    off.H = str_off.substring(0, 2).toInt();
    off.M = str_off.substring(3, 5).toInt();

      int ini = on.H * 60 + on.M;
      int end = off.H * 60 + off.M;
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
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println();
}*/

void setup() {
  ACS.suppressNoise(true);
  ACS.autoMidPoint(30, 2);
  
  Serial.begin(115200);

  // Initialize the output variables as outputs
  pinMode(output32, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output32, LOW);

  WiFi.mode(WIFI_STA); //esp em modo de estacao
  WiFiManager wifiMan; //cria um objeto WiFiManager
  wifiMan.setConnectTimeout(10);
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

  outputState = true;
  digitalWrite(output32, HIGH);
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
    max_reading = read_sensor() > max_reading ? read_sensor() : max_reading;
    measurement_timer = millis();
  }

  if (millis() - time_check_timer >= TIME_CHECK_INTERVAL) {
    // TODO: Ver se é hora de ligar/desligar
    /*
    Handle incoming client requests
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    bool inInterval = ((now_min - startTimestamp + 1440) % 1440) < ((endTimestamp - startTimestamp + 1440) % 1440);

    if(inInterval)
    {
      if(outputState)
          println("deligando");
          handleGPIO32Off();
    }
    else
    {
      if(!outputState)
        println("ligando");
        handleGPIO32On();
    }
    */
    time_check_timer = millis();
  }

  if (millis() - saving_timer >= SAVING_INTERVAL) {
    // readings[dia_da_semana(0 a 6)][hora_do_dia(0 a 47)] = max_reading;
    max_reading = 0;
    saving_timer = millis();
  }

  // Delay o menor intervalo (talvez não, pra conectar o wifi)
  // delay(MEASUREMENT_INTERVAL); // TODO: Deep sleep  
  server.handleClient();
}


  #include <WiFi.h>
  #include <WebServer.h>
  #include <stdint.h>
  #include <ESPmDNS.h>
  #include <WiFiManager.h> 
  #include <time.h>

  typedef struct{
    uint8_t H;
    uint8_t M;
  }Tempo;

time_t startTimestamp = -1;
time_t endTimestamp = -1;

  // Replace with your network credentials
  const char* ssid = "A52";
  const char* password = "npnt0774";

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

  void setup() {
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
    // Handle incoming client requests

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

  int now_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
      if (endTimestamp < startTimestamp)
  {
    if (now_min >= startTimestamp || now_min < endTimestamp)
    {
      if(outputState)
      {
        handleGPIO32Off();
        Serial.println("desligando");
      }
    }
    else if(!outputState)
    {
      Serial.println("ligando");
      handleGPIO32On();
    }
  }
  else
  {
    if (now_min >= startTimestamp && now_min < endTimestamp)
    {
      if(outputState)
      {
        Serial.println("desligando");
        handleGPIO32Off();
      }
    }
    else if(!outputState)
    {
      Serial.println("ligando");
      handleGPIO32On();
    }
  }

      /*bool inInterval = ((now_min - startTimestamp + 1440) % 1440) < ((endTimestamp - startTimestamp + 1440) % 1440);

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
    server.handleClient();
  }

  void printLocalTime(){
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
}

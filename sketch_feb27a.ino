
  #include <WiFi.h>
  #include <WebServer.h>
  #include "time.h"

  // Replace with your network credentials
  const char* ssid = "Iensen";
  const char* password = "itonfo123";

 // NTP Server details
  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = 0;
  const int   daylightOffset_sec = 3600;

  // Assign output variables to GPIO pins
  const int output32 = 32;
  String output32State = "off";

  // Create a web server object
  WebServer server(80);

  // Function to handle turning GPIO 32 on
  void handleGPIO32On() {
    output32State = "on";
    digitalWrite(output32, HIGH);
    handleRoot();
  }

  // Function to handle turning GPIO 32 off
  void handleGPIO32Off() {
    output32State = "off";
    digitalWrite(output32, LOW);
    handleRoot();
  }

  // Function to handle the root URL and show the current states
  void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<link rel=\"icon\" href=\"data:,\">";
    html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
    html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
    html += ".button2 { background-color: #555555; }</style></head>";
    html += "<body><h1>ESP32 Web Server</h1>";

    // Display GPIO 32 controls
    html += "<p>GPIO 32 - State " + output32State + "</p>";
    if (output32State == "off") {
      html += "<p><a href=\"/32/on\"><button class=\"button\">ON</button></a></p>";
    } else {
      html += "<p><a href=\"/32/off\"><button class=\"button button2\">OFF</button></a></p>";
    }

    html += "</body></html>";
    server.send(200, "text/html", html);
  }

  void setup() {
    Serial.begin(115200);

    // Initialize the output variables as outputs
    pinMode(output32, OUTPUT);
    // Set outputs to LOW
    digitalWrite(output32, LOW);

    // Connect to Wi-Fi network
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Set up the web server to handle different routes
    server.on("/", handleRoot);
    server.on("/32/on", handleGPIO32On);
    server.on("/32/off", handleGPIO32Off);

    // Start the web server
    server.begin();
    Serial.println("HTTP server started");

    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
  }

  void loop() {
    // Handle incoming client requests
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

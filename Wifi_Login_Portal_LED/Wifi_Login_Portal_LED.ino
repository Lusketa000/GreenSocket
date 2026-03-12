#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h> //requer library, baixar no próprio IDE
#include <ESPmDNS.h>

WebServer server(80); //CRIA UM SERVIDOR NA PORTA 80(WEB)

const int GPIO_out = 0; //GPIO que controla o rele

bool outputState = false; //estado padrao do rele (ligado)

void GPIO_on()
{
  outputState = true;

  digitalWrite(GPIO_out, HIGH);

  handleRoot();
}

void GPIO_off()
{
  outputState = false;

  digitalWrite(GPIO_out, LOW);

  handleRoot();
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32 Web Server</h1>";
  html += "<p>State:</p>";

  if (outputState)
    html += "<a href=\"/off\"><button>OFF</button></a>";
  else
    html += "<a href=\"/on\"><button>ON</button></a>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  
  Serial.begin(115200); //inicia comunicação serial para debug

  pinMode(GPIO_out, OUTPUT); //define pino como output
  digitalWrite(GPIO_out, LOW); //inicia com rele desligado

  WiFi.mode(WIFI_STA); //esp em modo de estacao
  WiFiManager wifiMan; //cria um objeto WiFiManager

  bool res = wifiMan.autoConnect("ESP32C3-Setup"); //caso nao consiga se conectar a ultima rede salva, cria a propria rede de configuração

  if(!res)
  {
    Serial.println("Falha na conexão!");
  }
  else
  {
    Serial.println("Conectado!");

    IPAddress local_IP(192,168,1,200); //teste para ver se funciona 
    MDNS.begin("rele");

    Serial.println(WiFi.localIP());

    outputState = true;
    digitalWrite(GPIO_out, HIGH);
  }

  server.on("/", handleRoot);

  // Liga GPIO
  server.on("/on", GPIO_on);

  // Desliga GPIO
  server.on("/off", GPIO_off);

  // Inicia servidor HTTP
  server.begin();

}

void loop() {

  server.handleClient();
}

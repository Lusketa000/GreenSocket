// ESP se conecta no wifi e da inicio no ESP Now
// ESP vai mandar um Hello, contendo o ID dele e MAC Addres, esperando uma repsosta 
// ESP tenta até 3 vezes, se não receber nada supoe virar o Mestre
// se receber o ACK, salva o MAC Addres recebido, e responde com ACK, para confirmar a recepção
// Se foi ele quem enviou o ACK para o Hello, Envia o seu MAC Address e espera pelo ACK resposta para iniciar conexão
// Se for o Mestre, realiza a conexão com o Server TCP
// Se ouver multiplos ESPs, realiza asm conexões

#include <WiFi.h>
#include <esp_now.h>
#define MAX_PEERS 20
#define HELLO_INTERVAL 5000
#define TIMEOUT 15000
#define CHANNEL 1


typedef enum {
	HELLO,
	ACK_HELLO,
	ACK_CONFIRM
}msg_type;

typedef struct {
	msg_type;
	uint8_t mac[6];
	uint32_t id;
}msg_header;

typedef struct {
	uint8_t mac[6];
	unsigned long last_ping_ms;
	bool connected;
}peer;

peer peer_list [MAX_PEERS];
uint8_t my_mac[6];
uint32_t msg_counter = 0;
unsigned long last_hello = 0;

bool mac_equals(uint8_t *a, uint8_t *b){
	return memcmp(a,b,6) == 0;
}

void print_mac(uint8_t *mac){
	Serial.printf("%02X:%02X;%02X:%02X;%02X:%02X",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/*int scan_peer (uint8_t *mac){
	
}*/

void add_or_update (uint8_t *mac){
	int offset = scan_peer(mac);
	if(offset == -1){
		//TODO: Criar novo Peer
	}else{ //TODO: Atualizar Peer }
}

void Setup()
{
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	
	if (esp_now_init != ESP_OK) {
		Serial.println("erro ao iniciar ESP_NOW");
		return;
	}
	esp_now_register_recv_cb(Receiver);
}

void Receiver (const uint8_t *mac, const uint8_t *data, int len){
	char mac_Str[18];
	snprintf(macStr, sizeof(macStr, "%02X:%02X:%02X:%02x:%02X:%02X",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

void loop(){

}

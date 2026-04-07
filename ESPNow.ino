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

//DEFINICAO QUE INDICA O TIPO DE MENSAGEM DO PACOTE
typedef enum {
	HELLO,
	ACK_HELLO,
	ACK_CONFIRM
}msg_type;

//HEADER DOS PACOTES, COM TIPO, MAC E ID LOCAL DO ESP
typedef struct {
	msg_type;
	uint8_t mac[6];
	uint32_t id;
}msg_header;

//STRUCT DE PEER QUE FARAO PARTE DA LISTA DE PEERS, COM BOOLEANA PARA SABER SE ESTA CONECTADO E LAST_SEEN PARA SABER QUANDO PEER SINALIZOU ESTAR "VIVO"
typedef struct {
	uint8_t mac[6];
	unsigned long last_seen;
	bool connected;
}peer;

//DEFINICAO DE INFORMACOES QUE CADA ESP CONHECERA SOBRE SI MESMO E SOBRE OS OUTROS
peer peer_list [MAX_PEERS];
uint8_t this_mac[6];
uint32_t msg_counter = 0;
unsigned long last_hello = 0;

//RECEBE DOIS MACS E RETORNA TRUE PARA MACS IGUAIS E FALSO, CASO CONTRARIO
bool mac_equals(uint8_t *a, uint8_t *b){
	return memcmp(a,b,6) == 0;
}

//PRINTA O MAC, FORMATADO
void print_mac(uint8_t *mac){
	Serial.printf("%02X:%02X;%02X:%02X;%02X:%02X",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

//FUNCAO QUE BUSCA POR UM PEER ESPECIFICO NA LISTA LOCAL DO ESP
int scan_peer (uint8_t *mac){
	for (int i =0; i < MAX_PEERS; i++) //mudar para while !NULL, caso seja inciado nulo igual C
	{
		if (peer_list[i].last_seen != 0 && mac_equals(peer_list[i].mac, mac)) {
			return i;
		}
	}
	return -1;
}

//INSERE NOVOS PEERS NA LISTA OU ATUALIZA O STATUS DOS EXISTENTES
void add_or_update (uint8_t *mac){
	int offset = scan_peer(mac);
	if(offset == -1){
		for (int i = 0; i < MAX_PEERS; i++) {  //mudar para while !NULL, caso seja inciado nulo igual C
			if (peer_list[i].last_seen == 0) {
				memcpy(peer_list[i].mac, mac, 6);
				peer_list[i].last_seen = millis();
				peer_list[i].connected = false;

				Serial.print("Novo Peer:\t");
				print_mac(mac);
				Serial.println();
				return;
			}
		}
	}else{ //TODO: Atualizar Peer
		peer_list[offset].last_seen = millis();
	}
}


void remove_peer(int id) {
	Serial.print("Removendo Peer:\t");
	print_mac(mac);
	Serial.println();

	memset(&peer_list[id], 0, sizeof(peer));
}

void add_peer_espnow(uint8_t *mac) {
	if (esp_now_is_peer_exist(mac)) return;

	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, mac, 6);
	peerInfo.channel = CHANNEL;
	peerInfo.encrypt = false;

	if (esp_now_add_peer(&perrInfo) == ESP_OK) {
		Serial.print("Peer adicionado ao ESP-NOW: ");
    print_mac(mac);
    Serial.println();
	}
}

void send_message(uint8_t *dest_mac, msg_type type) {
	msg_header msg;
	msg.type = type;
	memcpy(msg.mac, this_mac, 6);
	msg.id = msg_counter++;

	esp_now_send(dest_mac, (uint8_t*)&msg, sizeof(msg));
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
	msg_header msg;
	memcpy(&msg, data, sizeof(msg));

	add_or_update_peer((uint8_t*)mac);

	Serial.print("Recebido de ");
  print_mac((uint8_t*)mac);
  Serial.print(" tipo: ");
  Serial.println(msg.type);

	switch (msg.type) {
		case MSG_HELLO:
			send_message((uint8_t*)mac, MSG_ACK);
			break;
		case MSG_ACK:
			send_message((uint8_t*)mac, MSG_ACK_CONFIRM);
			break;
		case MSG_ACK_CONFIRM:
			int id = scan_peer((uint8_t*)mac);
			if (id != -1 && !peer_list[i].connected) {
				peer.list[id].connected = true;
				add_peer_espnow((uint8_t*)mac);
			}
			break;
		
	}
}

bool is_master() {
	for (int i = 0; i < MAX_PEERS; i++) {
		if (peer_list[i].last_seen != 0) {
			if (memcmp(peer_list[i].mac, this_mac, 6) < 0) {
        return false;
			}
		}
	}
	return true;
}

void Setup()
{
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	
	esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
	WiFi.macAddress(this_mac);

	Serial.print("Meu MAC: ");
  print_mac(this_mac);
  Serial.println();

	if (esp_now_init != ESP_OK) {
		Serial.println("erro ao iniciar ESP_NOW");
		return;
	}
	
	esp_now_register_recv_cb(onReceive);

	memset(peers, 0, sizeof(peers));
}

void loop(){
	// Envia HELLO broadcast
  if (millis() - lastHello > HELLO_INTERVAL) {
    lastHello = millis();

    uint8_t broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    send_message(broadcast, MSG_HELLO);

    Serial.println("HELLO enviado");
  }

	// Timeout peers
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peer_list[i].last_seen != 0) {
      if (millis() - peer_list[i].last_seen > TIMEOUT) {
        remove_peer(i);
      }
    }
  }

  // Debug mestre
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();

    Serial.print("Sou mestre? ");
    Serial.println(is_master() ? "SIM" : "NAO");
  }

  delay(10);
}

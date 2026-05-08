// ESP se conecta no wifi e da inicio no ESP Now
// ESP vai mandar um Hello, contendo o ID dele e MAC Addres, esperando uma repsosta
// ESP tenta até 3 vezes, se não receber nada supoe virar o Mestre
// se receber o ACK, salva o MAC Addres recebido, e responde com ACK, para confirmar a recepção
// Se foi ele quem enviou o ACK para o Hello, Envia o seu MAC Address e espera pelo ACK resposta para iniciar conexão
// Se for o Mestre, realiza a conexão com o Server TCP
// Se ouver multiplos ESPs, realiza asm conexões

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#define MAX_PEERS 20 //NUMERO MAX SUPORTADO PELO ESP-NOW
#define HELLO_INTERVAL 5000 //5 SEGUNDOS ENTRE CADA HELLO, PARA DESCOBERTA DE NOVOS PEERS E ATUALIZACAO DE VIDA
#define TIMEOUT 15000 //TIMEOUT DEPOIS DE 15 SEGUNDOS SEM RECEBER HELLO
#define CHANNEL 1 //CANAL DO WIFI EM QUE VAO OPERAR

//REDE DE BROADCAST, PARA HELLO
const uint8_t BROADCAST[6] = {
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF
};

//TIPOS DE MENSAGENS
typedef enum {
	MSG_HELLO,
	MSG_ACK_HELLO,
	MSG_ACK_CONFIRM
} msg_type;

//ESTADOS POSSIVEIS PARA OS PEERS, DEFAULT = EMPTY
typedef enum {
	EMPTY,
	DISCOVERED,
	ASSOCIATED,
	CONNECTED,
	DISCONNECTED
} peer_state;

//HEADER DOS PACOTES (TIPO DA MENSAGEM, MAC DESTINO)//OBS: MAC_ORIGEM JA EH PASSADO PELO PROTOCOLO
typedef struct {
	msg_type type;
	uint8_t mac_d[6];
} msg_header;

//STRUCT DE UM PEER (MAC DO PEER, TEMPO DESDE ULTIMA MENSAGEM E STATUS DE CONEXAO)
typedef struct {
	uint8_t mac[6];
	unsigned long last_seen;
	peer_state state;
} peer;

//LISTA DE PEERS CONHECIDOS
peer peer_list[MAX_PEERS];

//MAC DESTE ESP
uint8_t this_mac[6];

//NUMERO DE MENSAGENS ENVIADAS
uint32_t msg_counter = 0;

//TEMPO DESDE O ULTIMO HELLO
unsigned long last_hello = 0;

//RETORNA TRUE SE MAC A == MAC B, CASO CONTRARIO RETORNA FALSE
bool mac_equals(uint8_t *mac_a, uint8_t *mac_b) {
	return memcmp(mac_a, mac_b, 6) == 0;
}

//FORMATA MAC PARA VISUALIZACAO
void print_mac(uint8_t *mac) {
	Serial.printf("%02X:%02X;%02X:%02X;%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	Serial.println();
}

//BUSCA POR UM MAC NA PEER_LIST, RETORNA O INDICE PARA ACESSA-LO OU -1 SE NAO ENCONTRAR CORRESPONDENTE
int find_peer(uint8_t *mac) {
	for (int i = 0; i < MAX_PEERS; i++)
	{
		if (peer_list[i].state != EMPTY && mac_equals(peer_list[i].mac, mac)) {
			return i;
		}
	}
	return -1;
}


int add_peer(uint8_t *mac) {
	for (int i = 0; i < MAX_PEERS; i++) {
		if (peer_list[i].state == EMPTY) {
			memcpy(peer_list[i].mac, mac, 6);
			peer_list[i].last_seen = millis();
			peer_list[i].state = DISCOVERED;

			Serial.print("Novo Peer descoberto:\t");
			print_mac(mac);

			Serial.println("Estabelecendo conexao...\n");
			return i;
		}
	}
	return -1;
}

int update_peer(uint8_t *mac, int id) {
	if (id == -1) {
		int id = find_peer(mac);
		if (id == -1)
			return -1;
	}

	peer_list[id].last_seen = millis();
	return id;
}

void  remove_peer(int id) {
	if (id == -1 || id >= MAX_PEERS)
		return;

	Serial.print("Removendo Peer:\t");
	print_mac(peer_list[id].mac);
	Serial.println();

	esp_now_del_peer(peer_list[id].mac);
	memset(&peer_list[id], 0, sizeof(peer));
}

bool add_peer_espnow(uint8_t *mac) {
	if (esp_now_is_peer_exist(mac)) return true;

	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, mac, 6);
	peerInfo.channel = CHANNEL;
	peerInfo.encrypt = false;

	esp_err_t result = esp_now_add_peer(&peerInfo);
	if (result == ESP_OK) {
		Serial.print("Peer adicionado ao ESP-NOW: ");
		print_mac(mac);
		Serial.println();
		return true;
	}

	Serial.print("Erro ao adicionar peer: ");
	Serial.println(result);

	return false;
}

bool send_message(uint8_t *dest_mac, msg_type type) {
	if (mac_equals(dest_mac, this_mac))
		return false;

	msg_header msg = {};
	msg.type = type;
	memcpy(msg.mac_d, dest_mac, 6);
	//adicionar id para evitar pacotes duplicados

	esp_err_t result = esp_now_send(dest_mac, (uint8_t *)&msg, sizeof(msg));
	if (result != ESP_OK) {
		Serial.print("Erro ao enviar mensagem: ");
		Serial.println(result);
		return false;
	}
	return true;
}

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
	if (len != sizeof(msg_header)) return;

	msg_header msg;
	memcpy(&msg, data, sizeof(msg));

	const uint8_t *mac = info->src_addr;
	int id = find_peer((uint8_t *)mac);

	switch (msg.type) {
		case MSG_HELLO:
			Serial.println("HELLO recebido de ");
			print_mac((uint8_t *)mac);
			if (id == -1) {
				Serial.println("Novo peer");
				id = add_peer((uint8_t *)mac); //adicionar funcao so para add
				add_peer_espnow((uint8_t *)mac);
				peer_list[id].state = ASSOCIATED; //MOVER PARA ADD_PEER_ESPNOW
				Serial.println("respondendo com ACK HELLO");
				send_message((uint8_t *)mac, MSG_ACK_HELLO);
			}
			else if (peer_list[id].state == CONNECTED){
				Serial.println("peer conhecido");
				update_peer((uint8_t *)mac, id);
			}
		break;
		case MSG_ACK_HELLO:
			if (id == -1 && mac_equals(msg.mac_d, this_mac)) {
				id = add_peer((uint8_t *)mac);
				add_peer_espnow((uint8_t *)mac); //RETORNAR ID
				peer_list[id].state = CONNECTED;
				Serial.println("respondendo com ACK CONFIRM");
				send_message((uint8_t *)mac, MSG_ACK_CONFIRM);
			}
		break;
		case MSG_ACK_CONFIRM:
			if (id != -1 && mac_equals(msg.mac_d, this_mac) && peer_list[id].state == ASSOCIATED) {
				Serial.println("ACK CONFIRM RECEBIDO");
				update_peer((uint8_t *)mac, id);
				peer_list[id].state = CONNECTED;
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

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();

	esp_wifi_set_promiscuous(true);
	esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
	esp_wifi_set_promiscuous(false);
	WiFi.macAddress(this_mac);

	Serial.print("Meu MAC: ");
	print_mac(this_mac);
	Serial.println();
	

	if (esp_now_init() != ESP_OK) {
		Serial.println("erro ao iniciar ESP_NOW");
		return;
	}

	add_peer_espnow((uint8_t *)BROADCAST);

	esp_now_register_recv_cb(onReceive);

	memset(peer_list, 0, sizeof(peer_list));
}

void loop() {
	// Envia HELLO broadcast
	if (millis() - last_hello > HELLO_INTERVAL) {
		last_hello = millis();

		send_message((uint8_t *)BROADCAST, MSG_HELLO);

		Serial.println("HELLO enviado via broadcast\n");
	}

	// Timeout peers
	for (int id = 0; id < MAX_PEERS; id++) {
		if (peer_list[id].last_seen != 0) {
			if (millis() - peer_list[id].last_seen > TIMEOUT) {
				remove_peer(id);
			}
		}
	}

	// Debug mestre
	static unsigned long lastPrint = 0;
	if (millis() - lastPrint > 5000) {
		lastPrint = millis();

		Serial.print("Sou mestre?\n");
		Serial.println(is_master() ? "SIM\n\n" : "NAO\n\n");
	}

	//delay(10);
}

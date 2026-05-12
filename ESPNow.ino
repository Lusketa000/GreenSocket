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
#define MSG_INTERVAL 5000 //5 SEGUNDOS ENTRE CADA HELLO, PARA DESCOBERTA DE NOVOS PEERS E ATUALIZACAO DE VIDA
#define TIMEOUT 15000 //TIMEOUT DEPOIS DE 15 SEGUNDOS SEM RECEBER HELLO
#define CHANNEL 1 //CANAL DO WIFI EM QUE VAO OPERAR

int RAND = 0;
//REDE DE BROADCAST, PARA HELLO
const uint8_t BROADCAST[6] = {
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF
};

//TIPOS DE MENSAGENS
typedef enum {
	MSG_HELLO,
	MSG_ACK_HELLO,
	MSG_ACK_CONFIRM,
	MSG_ACK_CONNECTED,
	MSG_ALIVE
} msg_type;

//ESTADOS POSSIVEIS PARA OS PEERS, DEFAULT = EMPTY
typedef enum {
	//PEER_LIST STATUS
	EMPTY,
	DISCOVERED,
	ASSOCIATED,
	CONNECTED,
	DISCONNECTED,

	//ESP STATES
	SEARCHING,
	WORKING,
	PAIRING,
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

peer_state this_state = SEARCHING;

//MAC DESTE ESP
uint8_t this_mac[6];

//NUMERO DE MENSAGENS ENVIADAS
uint32_t msg_counter = 0;

//TEMPO DESDE O ULTIMO ALIVE
unsigned long last_alive = 0;

//TEMPO DESDE O ULTIMO HELLO
unsigned long last_hello = 0;

//TEMPO RANDOM PARA PROXIMO HELLO
unsigned long next_hello = random(500, 3000);

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
	for (int id = 0; id < MAX_PEERS; id++) {
		if (peer_list[id].state == EMPTY) {
			memcpy(peer_list[id].mac, mac, 6);
			peer_list[id].last_seen = millis();
			peer_list[id].state = DISCOVERED;

			Serial.print("[DEBUG_add_peer]: PEER CRIADO COM SUCESSO ->\t");
			print_mac(mac);
			return id;
		}
	}
	Serial.print("[DEBUG_add_peer]: ERRO NA CRIAÇÃO DO PEER ->\t");
	print_mac(mac);
	return -1;
}

int update_peer(uint8_t *mac, int id) {
	if (id == -1) {
		int i = find_peer(mac);
		if (i == -1)
			return -1;
		id = i;
	}
	peer_list[id].last_seen = millis();
	Serial.printf("millis(): %lu\tlast_seen: %lu\n", millis(), peer_list[id].last_seen);
	return id;
}

void  remove_peer(int id) {
	if (id == -1 || id >= MAX_PEERS)
		return;

	Serial.print("Removendo Peer:\t");
	print_mac(peer_list[id].mac);
	Serial.printf("removendo pois... -> millis(): %lu\tlast_seen: %lu\n", millis(), peer_list[id].last_seen);
	Serial.println();

	esp_now_del_peer(peer_list[id].mac);
	memset(&peer_list[id], 0, sizeof(peer));

	if (this_state == PAIRING)
		this_state = SEARCHING;

	return;
}

bool add_peer_espnow(uint8_t *mac) {
	//SE JA HOUVER CONEXAO, RETORNA SUCESSO
	if (esp_now_is_peer_exist(mac)) return true;

	//CONFIGURA PEER
	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, mac, 6);
	peerInfo.channel = CHANNEL;
	peerInfo.encrypt = false;

	//TENTA REGISTRAR PEER NO PROTOCOLO
	esp_err_t result = esp_now_add_peer(&peerInfo);
	if (result == ESP_OK) {
		Serial.print("[DEBUG_add_peer_espnow]: PEER ADICIONADO AO ESP-NOW: ");
		print_mac(mac);
		return true;
	}

	//REGISTRA MENSAGEM DE ERRO
	Serial.print("[DEBUG_add_peer_espnow]: ERRO AO ADICIONAR PEER: ");
	print_mac(mac);
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
	//if(this_state == WORKING) return;
		//this_state = WORKING;

	//SE MENSAGEM NAO TIVER FORMATO DESEJADO, DESCARTA PACOTE
	if (len != sizeof(msg_header)) {
		this_state = SEARCHING;
		return;
	}

	//COPIA DADOS DA MENSAGEM PARA UM BUFFER LOCAL
	msg_header msg;
	memcpy(&msg, data, sizeof(msg));

	//SE NAO FOR BROADCAST OU NAO FOR PARA MIM, DESCARTA PACOTE
	if (!mac_equals(msg.mac_d, (uint8_t *)BROADCAST) && !mac_equals(msg.mac_d, this_mac)) {
		this_state = SEARCHING;
		return;
	}


	//PEGA MAC ORIGEM DO PACOTE, FORNECIDO PELA STRUCT INFO DO ESP-NOW E TENTA LOCALIZAR NA PEER_LIST
	const uint8_t *mac = info->src_addr;
	int id = find_peer((uint8_t *)mac);

	Serial.printf("[DEBUG_OnReceive]: MENSAGEM INTERCEPTADA!\n");
	Serial.printf("ID: %d     state: %d     mac: ", id, this_state);
	print_mac((uint8_t *)mac);
	
	//SE NAO ESTIVER NA PEER_LIST
	if (id == -1 && this_state == SEARCHING) {
		//HELLO - BROADCAST
		if (msg.type == MSG_HELLO) {
			this_state = PAIRING;
			Serial.printf("[DEBUG_OnReceive]: NOVO HELLO RECEBIDO DE ");
			print_mac((uint8_t *)mac);

			//ADICIONA NA PEER_LIST
			Serial.println("[DEBUG_OnReceive]: CRIANDO PEER");
			id = add_peer(((uint8_t *)mac));
			if (id == -1) return;
			
			//ADICIONA NO PROTOCOLO ESP-NOW
			if (!add_peer_espnow((uint8_t *)mac)) {
				//remove peer da lista
				this_state = SEARCHING;
				return;
			}

			//PEER CONECTADO UNIDIRECIONALMENTE, ASSOCIADO
			peer_list[id].state = ASSOCIATED;
			Serial.println("[DEBUG_OnReceive]: RESPONDENDO COM ACK HELLO\n");
			if (!send_message((uint8_t *)mac, MSG_ACK_HELLO))
			{
				//TODO: remover peer da lista e do espnow
				Serial.println("[DEBUG_OnReceive]: ERRO AO ENVIAR ACK HELLO\n");
				this_state = SEARCHING;
			}
			return;
		}
		//HELLO ACK - UNICAST PARA ESSE MAC 
		else if (msg.type == MSG_ACK_HELLO && mac_equals(msg.mac_d, this_mac)) {
			this_state = PAIRING;
			Serial.printf("[DEBUG_OnReceive]: ACK HELLO RECEBIDO DE ");
			print_mac((uint8_t *)mac);

			//ADICIONA NA PEER_LIST
			Serial.println("[DEBUG_OnReceive]: CRIANDO PEER");
			id = add_peer((uint8_t *)mac);
			if (id == -1) return;

			//ADICONA NO PROTOCOLO ESP-NOW
			if (!add_peer_espnow((uint8_t *)mac)) {
				//remove peer
				this_state = SEARCHING;
				return;
			}

			//PEER CONECTADO BIDIRECIONALMENTE, ASSOCIADO
			peer_list[id].state = ASSOCIATED;
			Serial.println("[DEBUG_OnReceive]: RESPONDENDO COM ACK CONFIRM");
			if (!send_message((uint8_t *)mac, MSG_ACK_CONFIRM))
			{
				//REMOVE PEER DA LISTA E DO ESP-NOW
				remove_peer(id);

				Serial.println("[DEBUG_OnReceive]: ERRO AO ENVIAR ACK CONFIRM\n");
				this_state = SEARCHING;
			}
			return;
		}
	}
	//SE ESTIVER NA PEER_LIST E FOR PARA ESTE ESP
	else if (mac_equals(msg.mac_d, this_mac)) {
		if (this_state == PAIRING) {
			if (msg.type == MSG_ACK_CONFIRM) {
				Serial.println("[DEBUG_OnReceive]: ACK CONFIRM RECEBIDO DE:");
				print_mac((uint8_t *)mac);

				//ESTABELECE CONEXAO
				peer_list[id].state = CONNECTED;
				Serial.println("[DEBUG_OnReceive]: RESPONDENDO COM ACK CONNECTED");
				if (!send_message((uint8_t *)mac, MSG_ACK_CONNECTED))
				{
					//REMOVE PEER DA LISTA E DO ESP-NOW
					remove_peer(id);

					Serial.println("[DEBUG_OnReceive]: ERRO AO ENVIAR ACK CONNECTED\n");
				}
				this_state = SEARCHING;
				return;

			}
			else if (msg.type == MSG_ACK_CONNECTED) {
				Serial.println("[DEBUG_OnReceive]: ACK CONNECTED RECEBIDO DE:");
				print_mac((uint8_t *)mac);

				peer_list[id].state = CONNECTED;
				this_state = SEARCHING;
				return;
			}
		}
		else if (msg.type == MSG_ALIVE) {
			update_peer((uint8_t *)mac, id);
		}
	}
	this_state = SEARCHING;
	return;
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
	if ((millis() - last_hello > MSG_INTERVAL + next_hello)) {
		last_hello = millis();
		next_hello =random(500, 3000);

		if (this_state == SEARCHING) {
			send_message((uint8_t *)BROADCAST, MSG_HELLO);
			Serial.println("HELLO enviado via broadcast\n");
		}
	}

	//ENVIO DE ALIVE
	if (millis() - last_alive > 6000) {
        last_alive = millis();

        bool enviado = false;
        for (int id = 0; id < MAX_PEERS; id++) {
            if (peer_list[id].state == CONNECTED) {
                send_message(peer_list[id].mac, MSG_ALIVE);
                Serial.print("ALIVE enviado para: ");
                print_mac(peer_list[id].mac);
                enviado = true;
            }
        }
        if (!enviado) {
            Serial.println("Nenhum peer CONNECTED para enviar ALIVE");
        }
    }

	// Timeout peers
	for (int id = 0; id < MAX_PEERS; id++) {
		if (peer_list[id].last_seen != 0) {
			if (millis() - peer_list[id].last_seen > TIMEOUT) {
				Serial.printf("CHECK id=%d millis=%lu last_seen=%lu diff=%lu\n",
											id,
											millis(),
											peer_list[id].last_seen,
											millis() - peer_list[id].last_seen);
				remove_peer(id);
			}
		}
	}
	
	// Debug mestre
	static unsigned long lastPrint = 0;
	if (millis() - lastPrint > 5000 + RAND) {
		RAND = random(500, 3000);
		lastPrint = millis();

		Serial.print("Sou mestre?\n");
		Serial.println(is_master() ? "SIM\n\n" : "NAO\n\n");
	}

	//DELAY CURTO PARA EVITAR RODAR LOOP TANTAS VEZES POR SEGUNDO
	delay(10);
}

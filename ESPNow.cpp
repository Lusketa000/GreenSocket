#include "ESPNOW_protocol.h"

int RAND = 0;

const uint8_t BROADCAST[6] = {
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF
};

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
unsigned long next_hello = 0;

//TRUE SE O ESP ATUAL FOR MESTRE
bool master = false;

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

//ADICIONA PEER NA PEER_LIST
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

//PROLONGA VIDA DE PEER NA PEER_LIST
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

//REMOVE PEER DA PEER_LIST E DO ESP-NOW
void  remove_peer(int id) {
	if (id == -1 || id >= MAX_PEERS)
		return;

	Serial.print("Removendo Peer:\t");
	print_mac(peer_list[id].mac);
	Serial.printf("removendo pois... -> millis(): %lu\tlast_seen: %lu\n", millis(), peer_list[id].last_seen);
	Serial.println();

	esp_now_del_peer(peer_list[id].mac);
	memset(&peer_list[id], 0, sizeof(peer));

	define_master();

	if (this_state == PAIRING)
		this_state = SEARCHING;

	return;
}

//ADICIONA PEER NO ESP-NOW
bool add_peer_espnow(uint8_t *mac, uint8_t channel) {
	//SE JA HOUVER CONEXAO, RETORNA SUCESSO
	if (esp_now_is_peer_exist(mac)) return true;

	//CONFIGURA PEER
	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, mac, 6);
	peerInfo.channel = channel;
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

//ENVIA MENSAGEM DE UM TIPO ESPECIFICADO PARA O MAC INDICADO
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

//PROCESSA PACOTES RECEBIDOS
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
				master = define_master();
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
				master = define_master();
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

//RETORNA TRUE SE ESP FOR MESTRE, SE NAO, RETORNA FALSE
bool define_master() {
	uint8_t temp_mac[6];
	memcpy(temp_mac, this_mac, 6);
	int master_index = -1;

	for (int i = 0; i < MAX_PEERS; i++) {
		peer_list[i].master = false;

		if (peer_list[i].state == CONNECTED) {
			if (memcmp(peer_list[i].mac, temp_mac, 6) < 0) {
				memcpy(temp_mac, peer_list[i].mac, 6);
				master_index = i;
			}
		}
	}

	if (master_index == -1) {
		return true;
	}
	
	peer_list[master_index].master = true;
	return false;
}

void restartEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Falha ao reiniciar ESP_NOW");
    return;
  }
  
	delay(80);
  esp_now_register_recv_cb(onReceive);
  Serial.println("[ESP-NOW] Reiniciado com sucesso");

  // Readiciona o broadcast
  add_peer_espnow((uint8_t *)BROADCAST);
	Serial.println("[ESP-NOW] Broadcast readicionado");
  
	int count = 0;
  // Readiciona todos os peers da lista
  for (int i = 0; i < MAX_PEERS; i++) {
		print_mac(peer_list[i].mac);
		Serial.println(peer_list[i].state);
		Serial.println(peer_list[i].last_seen);
    if (peer_list[i].state == CONNECTED) {
      bool success = add_peer_espnow(peer_list[i].mac);
      
      if (success) {
        Serial.printf("[ESP-NOW] Peer %d readicionado com sucesso\tmac:", i);
				update_peer(peer_list[i].mac, i);
				count++;
      } else {
        Serial.printf("[ESP-NOW] Falha ao readicionar peer %d\tmac:", i);
      }
			print_mac(peer_list[i].mac);
    }
  }
	Serial.printf("[ESP-NOW] Total de peers readicionados: %d\n", count);
}

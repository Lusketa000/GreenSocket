#ifndef ESPNOW_PROTOCOL_H
#define ESPNOW_PROTOCOL_H

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#define MAX_PEERS 20 //NUMERO MAX SUPORTADO PELO ESP-NOW
#define MSG_INTERVAL 5000 //5 SEGUNDOS ENTRE CADA HELLO, PARA DESCOBERTA DE NOVOS PEERS E ATUALIZACAO DE VIDA
#define TIMEOUT 25000 //TIMEOUT DEPOIS DE 15 SEGUNDOS SEM RECEBER HELLO
#define CHANNEL 2 //CANAL DO WIFI EM QUE VAO OPERAR

//TIPOS DE MENSAGENS
enum msg_type {
	MSG_HELLO,
	MSG_ACK_HELLO,
	MSG_ACK_CONFIRM,
	MSG_ACK_CONNECTED,
	MSG_ALIVE
};

//ESTADOS POSSIVEIS PARA OS PEERS, DEFAULT = EMPTY
enum peer_state {
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
};

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
	bool master;
} peer;

//REDE DE BROADCAST, PARA HELLO
extern const uint8_t BROADCAST[6];

//VARIAVEL USADA PARA GERAR NUMEROS PSEUDO-ALEATORIOS
extern int RAND;

//LISTA DE PEERS CONHECIDOS
extern peer peer_list[MAX_PEERS];

//ESTADO DO PROPRIO ESP
extern peer_state this_state;

//MAC DESTE ESP
extern uint8_t this_mac[6];

//CANAL DE COMUNICAÇÃO
extern uint8_t wifi_channel;

//NUMERO DE MENSAGENS ENVIADAS
extern uint32_t msg_counter;

//TEMPO DESDE O ULTIMO ALIVE
extern unsigned long last_alive;

//TEMPO DESDE O ULTIMO HELLO
extern unsigned long last_hello;

//TEMPO RANDOM PARA PROXIMO HELLO
extern unsigned long next_hello;

//TRUE SE O ESP ATUAL FOR MESTRE
extern bool master;

//RETORNA TRUE SE MAC A == MAC B, CASO CONTRARIO RETORNA FALSE
bool mac_equals(uint8_t *mac_a, uint8_t *mac_b);

//FORMATA MAC PARA VISUALIZACAO
void print_mac(uint8_t *mac);

//BUSCA POR UM MAC NA PEER_LIST, RETORNA O INDICE PARA ACESSA-LO OU -1 SE NAO ENCONTRAR CORRESPONDENTE
int find_peer(uint8_t *mac);

//ADICIONA PEER NA PEER_LIST
int add_peer(uint8_t *mac);

//PROLONGA VIDA DE PEER NA PEER_LIST
int update_peer(uint8_t *mac, int id);

//REMOVE PEER DA PEER_LIST E DO ESP-NOW
void  remove_peer(int id);

//ADICIONA PEER NO ESP-NOW
bool add_peer_espnow(uint8_t *mac);

//ENVIA MENSAGEM DE UM TIPO ESPECIFICADO PARA O MAC INDICADO
bool send_message(uint8_t *dest_mac, msg_type type);

//PROCESSA PACOTES RECEBIDOS
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len);

//RETORNA TRUE SE ESP FOR MESTRE, SE NAO, RETORNA FALSE
bool define_master();

void restartEspNow();

#endif

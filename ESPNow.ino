// ESP se conecta no wifi e da inicio no ESP Now
// ESP vai mandar um Hello, contendo o ID dele e MAC Addres, esperando uma repsosta
// ESP tenta até 3 vezes, se não receber nada supoe virar o Mestre
// se receber o ACK, salva o MAC Addres recebido, e responde com ACK, para confirmar a recepção
// Se foi ele quem enviou o ACK para o Hello, Envia o seu MAC Address e espera pelo ACK resposta para iniciar conexão
// Se for o Mestre, realiza a conexão com o Server TCP
// Se ouver multiplos ESPs, realiza asm conexões
#include "ESPNOW_protocol.h"

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();

	randomSeed(micros());
	next_hello = random(500, 3000);

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

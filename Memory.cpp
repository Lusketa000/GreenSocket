#include <Arduino.h>
#include <Preferences.h>
#include "Memory.h"

static Preferences prefs;

void memoryBegin() {
  prefs.begin("matrix", false);
  Serial.println("[BOOT] Preferences namespace 'matrix' opened");
  Serial.println("[BOOT] Serial debug commands: dump, clear");
}

// Funcao para salvar uma leitura
void saveReading(int r, int c, int valor) {
  int indice = (r * COLS) + c; // Transforma 2D em 1D
  char chave[6];
  itoa(indice, chave, 10); // Converte o numero do indice em string (ex: "335")

  Serial.print("[NVS] Saving reading at row=");
  Serial.print(r);
  Serial.print(" col=");
  Serial.print(c);
  Serial.print(" key=");
  Serial.print(chave);
  Serial.print(" value=");
  Serial.println(valor);
  prefs.putShort(chave, valor);
}

int readReading(int r, int c) {
  int indice = (r * COLS) + c;
  char chave[6];
  itoa(indice, chave, 10);

  // O segundo parametro (0) e o valor retornado caso a chave ainda nao exista
  return prefs.getShort(chave, 0);
}

void debugPrintNVSMemory() {
  Serial.println("[NVS] Dump start");
  for (int r = 0; r < ROWS; r++) {
    Serial.print("[NVS] row=");
    Serial.print(r);
    Serial.print(" values=");
    for (int c = 0; c < COLS; c++) {
      Serial.print(readReading(r, c));
      if (c < COLS - 1) {
        Serial.print(',');
      }
    }
    Serial.println();
  }
  Serial.println("[NVS] Dump end");
}

void clearNVSMemory() {
  Serial.println("[NVS] Clearing namespace 'matrix'");
  prefs.clear();
  Serial.println("[NVS] Namespace cleared");
}

void handleSerialDebugCommands() {
  static String serialBuffer;

  while (Serial.available() > 0) {
    char incoming = Serial.read();
    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      serialBuffer.trim();
      serialBuffer.toLowerCase();

      if (serialBuffer == "dump" || serialBuffer == "dumpnvs") {
        debugPrintNVSMemory();
      } else if (serialBuffer == "clear" || serialBuffer == "clearnvs" || serialBuffer == "zero") {
        clearNVSMemory();
      } else if (serialBuffer.length() > 0) {
        Serial.print("[SERIAL] Unknown command: ");
        Serial.println(serialBuffer);
        Serial.println("[SERIAL] Available commands: dump, clear");
      }

      serialBuffer = "";
      continue;
    }

    serialBuffer += incoming;
  }
}

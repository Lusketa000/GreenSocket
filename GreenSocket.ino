#include "ACS712.h"

#define MEASUREMENT_INTERVAL 30000
#define TIME_CHECK_INTERVAL 60000
#define SAVING_INTERVAL 1800000


ACS712 ACS(33, 3.3, 4095, 123.33);

unsigned long measurement_timer = 0;
unsigned long time_check_timer = 0;
unsigned long saving_timer = 0;
int16_t max_reading = 0;
int16_t readings[7][48]; // TODO: Passar para memória não volátil

int16_t read_sensor() {
  int16_t current_mA = ACS.mA_AC_sampling(60, 4);
  current_mA = current_mA * 0.4933 - 32.368;
  if (current_mA < 0) current_mA = 0;
  return current_mA;
}

void setup()
{
  ACS.suppressNoise(true);
  ACS.autoMidPoint(30, 2);
}


void loop()
{
  if (millis() - measurement_timer >= MEASUREMENT_INTERVAL) {
    max_reading = read_sensor() > max_reading ? read_sensor() : max_reading;
    measurement_timer = millis();
  }

  if (millis() - time_check_timer >= TIME_CHECK_INTERVAL) {
    // TODO: Ver se é hora de ligar/desligar
    time_check_timer = millis();
  }

  if (millis() - saving_timer >= SAVING_INTERVAL) {
    // readings[dia_da_semana(0 a 6)][hora_do_dia(0 a 47)] = max_reading;
    max_reading = 0;
    saving_timer = millis();
  }

  // Delay o menor intervalo (talvez não, pra conectar o wifi)
  // delay(MEASUREMENT_INTERVAL); // TODO: Deep sleep
}

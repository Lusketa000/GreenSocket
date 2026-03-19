#include "ACS712.h"


ACS712  ACS(33, 3.3, 4095, 123.33);


int16_t ler_sensor() {
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
  // Ex de uso: int16_t corrente = ler_sensor();
}

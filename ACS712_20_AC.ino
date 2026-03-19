//
//    FILE: ACS712_20_AC.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: demo AC measurement with point to point
//     URL: https://github.com/RobTillaart/ACS712


#include "ACS712.h"


//  Arduino UNO has 5.0 volt with a max ADC value of 1023 steps
//  ACS712 5A  uses 185 mV per A
//  ACS712 20A uses 100 mV per A
//  ACS712 30A uses  66 mV per A


//  ACS712  ACS(A0, 5.0, 1023, 100);
//  ESP 32 example (might requires resistors to step down the logic voltage)
ACS712  ACS(33, 3.3, 4095, 123.33);


void setup()
{
  Serial.begin(115200);
  while (!Serial);
  Serial.println();
  Serial.println(__FILE__);
  Serial.print("ACS712_LIB_VERSION: ");
  Serial.println(ACS712_LIB_VERSION);
  Serial.println();

  ACS.suppressNoise(true);
  ACS.autoMidPoint(30, 2);
}


void loop()
{
  delay(30);

  float raw = analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  delay(1);
  raw += analogRead(33);
  raw /= 10;
  Serial.print("raw:");
  Serial.println(raw);


  float ptp = ACS.mA_peak2peak(60, 4);
  Serial.print("ptp:");
  Serial.println(ptp);


  float mA = ACS.mA_AC_sampling(60, 4);
  Serial.print("mA:");
  Serial.println(mA);
  Serial.print("mA2:");
  Serial.println(mA * 0.4933 - 32.368);
  

  int midpoint = ACS.getMidPoint();
  Serial.print("midpoint:");
  Serial.println(midpoint);
}


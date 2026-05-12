//
//    FILE: ACS712.cpp
//  AUTHOR: Rob Tillaart, Pete Thompson
// VERSION: 0.4.0
//    DATE: 2020-08-02
// PURPOSE: ACS712 library - current measurement
//     URL: https://github.com/RobTillaart/ACS712

#include "ACS712.h"


//  CONSTRUCTOR
ACS712::ACS712(uint8_t analogPin, float volts, uint16_t maxADC, float mVperAmpere)
{
  _pin         = analogPin;
  _mVperAmpere = mVperAmpere;
  _noisemV     = ACS712_DEFAULT_NOISE;    //  21mV according to datasheet

  //  set in setADC()
  //  keep it here until after experimental.
  _maxADC      = maxADC;
  _mVperStep   = 1000.0 * volts / maxADC;  //  1x 1000.0 for V -> mV
  _mAPerStep   = 1000.0 * _mVperStep / _mVperAmpere;
  _midPoint    = maxADC / 2;

  //  default ADC is internal.
  setADC(NULL, volts, maxADC);
}


float ACS712::mA_AC_sampling(float frequency, uint16_t cycles)
{
  uint32_t period = round(1000000UL / frequency);

  if (cycles == 0) cycles = 1;
  float sum = 0;

  float noiseLevel = _noisemV/_mVperStep;

  for (uint16_t i = 0; i < cycles; i++)
  {
    uint16_t samples    = 0;
    float    sumSquared = 0;

    uint32_t start = micros();
    while (micros() - start < period)
    {
      samples++;
      int value = _analogRead(_pin);
      if (_suppresNoise)  //  average 10 samples.
      {
        for (int j = 0; j < 9; j++)
        {
          // delay 1 ms
          delayMicroseconds(100);
          value += _analogRead(_pin);
        }
        value /= 10;
      }
      float current = value - _midPoint;
      sumSquared += (current * current);
      //  not adding noise squared might be more correct for small currents.
      if (abs(current) > noiseLevel)
      {
        sumSquared += (current * current);
      }
    }
    sum += sqrt(sumSquared / samples);
  }
  float mA = sum * _mAPerStep;
  if (cycles > 1) mA /= cycles;

  return mA;
}


//  configure by sampling for 2 cycles of AC
//  Also works for DC as long as no current flowing
//  note this is blocking!
uint16_t ACS712::autoMidPoint(float frequency, uint16_t cycles)
{
  uint16_t twoPeriods = round(2000000UL / frequency);

  if (cycles == 0) cycles = 1;

  uint32_t total = 0;
  for (uint16_t i = 0; i < cycles; i++)
  {
    uint32_t subTotal = 0;
    uint32_t samples  = 0;
    uint32_t start    = micros();
    while (micros() - start < twoPeriods)
    {
      uint16_t reading = _analogRead(_pin);
      subTotal += reading;
      samples++;
      //  Delaying prevents overflow
      //  since we'll perform a maximum of 40,000 reads @ 50 Hz.
      delayMicroseconds(1);
    }
    total += (subTotal / samples);
  }
  _midPoint = (total + (cycles/2))/ cycles;    //  rounding.
  return _midPoint;
}


void ACS712::suppressNoise(bool flag)
{
  _suppresNoise = flag;
}


void ACS712::setADC(uint16_t (* f)(uint8_t), float volts, uint16_t maxADC)
{
  _readADC = f;

  _maxADC      = maxADC;
  _mVperStep   = 1000.0 * volts / maxADC;  //  1x 1000 for V -> mV
  _mAPerStep   = 1000.0 * _mVperStep / _mVperAmpere;
  _midPoint    = maxADC / 2;
}


uint16_t ACS712::_analogRead(uint8_t pin)
{
  //  if external ADC is defined use it.
  if (_readADC != NULL) return _readADC(pin);
  return analogRead(pin);
}


int16_t ACS712::readSensor()
{
  Serial.println("[SENSOR] Starting sensor acquisition");
  autoMidPoint(30, 2);
  int16_t current_mA = (int16_t)mA_AC_sampling(60, 4);
  current_mA = (int16_t)(current_mA * 0.4933 - 32.368);
  if (current_mA < 0) current_mA = 0;
  Serial.print("[SENSOR] Current (mA): ");
  Serial.println(current_mA);
  return current_mA;
}

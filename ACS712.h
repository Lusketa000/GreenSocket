#pragma once
//
//    FILE: ACS712.h
//  AUTHOR: Rob Tillaart, Pete Thompson
// VERSION: 0.4.0
//    DATE: 2020-08-02
// PURPOSE: ACS712 library - current measurement
//     URL: https://github.com/RobTillaart/ACS712
//
// Tested with a RobotDyn ACS712 20A breakout + UNO.
//


#include "Arduino.h"


#define ACS712_DEFAULT_NOISE            21


class ACS712
{
  public:
    //  NOTE:
    //  One can quite precisely tune the value of the sensor
    //      (1) the milliVolt per Ampere and
    //      (2) the volts parameter.
    //
    //  TYPE   mV per Ampere
    //  5A        185.0
    //  20A       100.0
    //  30A        66.0
    ACS712(uint8_t analogPin, float volts = 5.0, uint16_t maxADC = 1023, float mVperAmpere = 100);

    //  returns mA
    //  blocks 20-21 ms per cycle to sample a whole 50 or 60 Hz period.
    //  works with sampling.
    //  lower frequencies block longer.
    //  does NOT call yield() as that would disrupt measurement
    float    mA_AC_sampling(float frequency = 50, uint16_t cycles = 1);
    
    //  Auto midPoint, assuming zero DC current or any AC current
    uint16_t autoMidPoint(float frequency = 50, uint16_t cycles = 1);
    
    //  enable/disable noiseSuppression for this measurement as needed.
    void     suppressNoise(bool flag);
    
    //  function returning 16 bit max, with pin or channel as parameter
    void setADC(uint16_t (*)(uint8_t), float volts, uint16_t maxADC);

    //  convenience wrapper used by the sketch
    int16_t  readSensor();


  private:
    uint8_t   _pin;
    uint16_t  _maxADC;
    float     _mVperStep;
    float     _mVperAmpere;
    float     _mAPerStep;
    int       _midPoint;
    uint8_t   _noisemV;
    bool      _suppresNoise = false;

    //  EXPERIMENTAL 0.3.4
    //  supports up to 16 bits ADC.
    uint16_t (* _readADC)(uint8_t);
    uint16_t _analogRead(uint8_t pin);

};

#pragma once
#include "rf430_device.h"

struct MeasurementResult {
 uint32_t timestampMs=0; bool valid=false; String error; uint16_t adc1Raw=0,adc2Raw=0,adc0Raw=0;
 double sensorResistance=0,referenceResistance=0,r0=0,deltaR=0,relativeDelta=0,gaugeFactor=0,strain=0;
};

class MeasurementService {
 public:
  explicit MeasurementService(RF430Device&d):device_(d){}
  bool beginSingle(const MeasurementConfig&c); bool poll(const MeasurementConfig&c,MeasurementResult&r); void cancel();
  bool busy()const{return active_;} const MeasurementResult&last()const{return last_;}
  void setBaseline(double r0){last_.r0=r0;}
  void configurationChanged(){configured_=false;}
 private:
  bool recoverAndConfigure(const MeasurementConfig&c);
  RF430Device&device_; bool active_=false;bool configured_=false;uint32_t started_=0;MeasurementResult last_;
};

bool calculateMeasurement(const MeasurementConfig&c,uint16_t adc1,uint16_t adc2,uint16_t adc0,MeasurementResult&r);

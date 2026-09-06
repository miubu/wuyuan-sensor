#pragma once
#include "pn5180_transport.h"
#include "rf430_config.h"

class RF430Device {
 public:
  explicit RF430Device(PN5180Transport &transport):transport_(transport){}
  bool detect(); bool applyConfig(const MeasurementConfig &config); bool startSampling(const MeasurementConfig &config);
  bool stopSampling(); bool state(RF430::SamplingState &state,uint8_t &status);
  bool readSample(const MeasurementConfig &config,uint16_t &adc1,uint16_t &adc2,uint16_t &adc0,uint8_t rawBlock[8]);
  bool readBlock(uint8_t block,uint8_t out[8]); bool resetReader(){detected_=false;return transport_.recover();}
  bool detected()const{return detected_;} const uint8_t* uid()const{return uid_;} uint8_t blockSize()const{return blockSize_;} uint8_t blockCount()const{return blockCount_;}
 private:
  PN5180Transport &transport_; uint8_t uid_[8]={}; uint8_t blockSize_=0,blockCount_=0; bool detected_=false;
};

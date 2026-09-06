#include "rf430_device.h"
#include <cstring>

bool RF430Device::detect(){ detected_=transport_.inventory(uid_); if(!detected_)return false; if(!transport_.getSystemInfo(uid_,blockSize_,blockCount_)||blockSize_!=RF430::BLOCK_SIZE){detected_=false;return false;} return true; }
bool RF430Device::readBlock(uint8_t b,uint8_t o[8]){return detected_&&transport_.readBlock(uid_,b,o,8);}
bool RF430Device::applyConfig(const MeasurementConfig &c){String e;if(!validateMeasurementConfig(c,e)||!detected_)return false;uint8_t old[8],next[8],verify[8],control[8];if(!readBlock(RF430::BLOCK_ADC_CONFIG,old))return false;buildBlock2(c,old,next);if(!transport_.writeBlock(uid_,RF430::BLOCK_ADC_CONFIG,next,8)||!readBlock(RF430::BLOCK_ADC_CONFIG,verify)||memcmp(next,verify,4)!=0)return false;buildBlock0(c,false,control);return transport_.writeBlock(uid_,RF430::BLOCK_CONTROL,control,8);}
bool RF430Device::startSampling(const MeasurementConfig &c){if(!detected_)return false;uint8_t b[8];buildBlock0(c,false,b);if(!transport_.writeBlock(uid_,RF430::BLOCK_CONTROL,b,8))return false;delay(3);buildBlock0(c,true,b);return transport_.writeBlock(uid_,RF430::BLOCK_CONTROL,b,8);}
bool RF430Device::stopSampling(){uint8_t b[8];if(!readBlock(0,b))return false;b[0]&=~RF430::GENERAL_START;return transport_.writeBlock(uid_,0,b,8);}
bool RF430Device::state(RF430::SamplingState&s,uint8_t&status){uint8_t b[8];if(!readBlock(0,b))return false;status=b[1];s=RF430::SamplingState(status&RF430::STATUS_STATE_MASK);return true;}
bool RF430Device::readSample(const MeasurementConfig&c,uint16_t&a1,uint16_t&a2,uint16_t&a0,uint8_t raw[8]){if(!readBlock(9,raw))return false;uint8_t p=0;a1=a2=a0=0;auto take=[&](){uint16_t v=uint16_t(raw[p])|(uint16_t(raw[p+1])<<8);p+=2;return uint16_t(v&RF430::RAW_MAX);};if(c.adc1Enabled)a1=take();if(c.adc2Enabled)a2=take();if(c.adc0Enabled)a0=take();return true;}

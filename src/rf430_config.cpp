#include "rf430_config.h"
#include <cstring>

static bool rateBits(const AdcConfig &c, uint8_t &bits) {
  static const uint16_t cic[] = {32,64,128,256,512,1024,2048};
  static const uint16_t ma[] = {4096,8192,16384,32768};
  const uint16_t *rates = c.filter == RF430::AdcFilter::CIC ? cic : ma;
  const uint8_t count = c.filter == RF430::AdcFilter::CIC ? 7 : 4;
  for (uint8_t i=0;i<count;i++) if (rates[i] == c.rate) { bits=i; return true; }
  return false;
}

bool encodeAdcConfig(const AdcConfig &c, uint8_t &value) {
  uint8_t rate=0; if (!rateBits(c, rate)) return false;
  value = (uint8_t(c.gain)&3U) | ((uint8_t(c.filter)&1U)<<2) |
          ((rate&7U)<<3) | ((uint8_t(c.reference)&1U)<<6);
  return true;
}

bool decodeAdcConfig(uint8_t value, AdcConfig &c) {
  if (value & 0x80) return false;
  c.gain=RF430::AdcGain(value&3); c.filter=RF430::AdcFilter((value>>2)&1);
  c.reference=RF430::AdcReference((value>>6)&1); uint8_t b=(value>>3)&7;
  static const uint16_t cic[] = {32,64,128,256,512,1024,2048};
  static const uint16_t ma[] = {4096,8192,16384,32768};
  if (c.filter==RF430::AdcFilter::CIC) { if(b>6)return false; c.rate=cic[b]; }
  else { if(b>3)return false; c.rate=ma[b]; }
  return true;
}

bool validateMeasurementConfig(const MeasurementConfig &c, String &e) {
  uint8_t v;
  if ((c.adc1Enabled&&!encodeAdcConfig(c.adc1,v)) || (c.adc2Enabled&&!encodeAdcConfig(c.adc2,v)) || (c.adc0Enabled&&!encodeAdcConfig(c.adc0,v))) { e="invalid ADC filter/rate combination"; return false; }
  if (c.frequency>16) { e="frequency must be 0..16"; return false; }
  if (c.passes<1 || c.passes>2047) { e="passes must be 1..2047"; return false; }
  if (c.infinite && c.passes!=2) { e="infinite sampling requires passes=2"; return false; }
  if (c.averaging<1) { e="averaging must be 1..255"; return false; }
  if (c.referenceOhm<=0 || c.gaugeFactor<=0) { e="Rref and gauge factor must be positive"; return false; }
  int sensors=(c.adc1Role==RF430::ChannelRole::Sensor)+(c.adc2Role==RF430::ChannelRole::Sensor);
  int refs=(c.adc1Role==RF430::ChannelRole::Reference)+(c.adc2Role==RF430::ChannelRole::Reference);
  if (sensors!=1 || refs!=1 || c.adc1Role==c.adc2Role) { e="ADC1/ADC2 need one Sensor and one Reference"; return false; }
  return true;
}

void buildBlock0(const MeasurementConfig &c, bool start, uint8_t o[8]) {
  memset(o,0,8); if(start)o[0]=RF430::GENERAL_START;
  o[2]=(c.adc1Enabled?RF430::SENSOR_ADC1:0)|(c.adc2Enabled?RF430::SENSOR_ADC2:0)|(c.adc0Enabled?RF430::SENSOR_ADC0:0);
  o[3]=uint8_t(((c.passes>>8)&7)<<5)|(c.frequency&0x1F); o[4]=c.passes&0xFF; o[5]=c.averaging;
  o[6]=c.infinite?RF430::INTERRUPT_INFINITE:0; o[7]=c.resistiveBias?RF430::ERROR_USING_THERMISTOR:0;
}

void buildBlock2(const MeasurementConfig &c, const uint8_t current[8], uint8_t o[8]) {
  memcpy(o,current,8); uint8_t v; if(encodeAdcConfig(c.adc1,v))o[0]=v; if(encodeAdcConfig(c.adc2,v))o[1]=v; if(encodeAdcConfig(c.adc0,v))o[2]=v;
}

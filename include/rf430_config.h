#pragma once
#include <Arduino.h>
#include "rf430_registers.h"

struct AdcConfig {
  RF430::AdcGain gain = RF430::AdcGain::X1;
  RF430::AdcFilter filter = RF430::AdcFilter::CIC;
  uint16_t rate = 256;
  RF430::AdcReference reference = RF430::AdcReference::SVSS;
};

struct MeasurementConfig {
  AdcConfig adc1, adc2, adc0;
  bool adc1Enabled = true, adc2Enabled = true, adc0Enabled = false;
  RF430::ChannelRole adc1Role = RF430::ChannelRole::Sensor;
  RF430::ChannelRole adc2Role = RF430::ChannelRole::Reference;
  uint8_t frequency = 3;
  uint16_t passes = 1;
  uint8_t averaging = 1;
  bool infinite = false;
  bool resistiveBias = true;
  double referenceOhm = 200000.0;
  double r0Ohm = 0.0;
  double gaugeFactor = 2.0;
};

bool encodeAdcConfig(const AdcConfig &config, uint8_t &value);
bool decodeAdcConfig(uint8_t value, AdcConfig &config);
bool validateMeasurementConfig(const MeasurementConfig &config, String &error);
void buildBlock0(const MeasurementConfig &config, bool start, uint8_t out[8]);
void buildBlock2(const MeasurementConfig &config, const uint8_t current[8], uint8_t out[8]);

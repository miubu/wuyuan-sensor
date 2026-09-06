#pragma once
#include "rf430_config.h"
class SettingsStore { public: void begin(); void load(MeasurementConfig&); void save(const MeasurementConfig&); };

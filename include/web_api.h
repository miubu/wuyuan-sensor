#pragma once
#include <ESPAsyncWebServer.h>
#include "measurement.h"
#include "settings.h"

class WebApi {
 public:
  WebApi(RF430Device&,MeasurementService&,MeasurementConfig&,SettingsStore&);
  void begin(); void loop(); void publish(const MeasurementResult&);
 private:
  String statusJson();String configJson();String resultJson(const MeasurementResult&);String previewJson();
  bool updateConfig(const String&,String&);void addHistory(const MeasurementResult&);String csv();
  RF430Device&device_;MeasurementService&measurement_;MeasurementConfig&config_;SettingsStore&store_;
  AsyncWebServer server_{80};AsyncEventSource events_{"/events"};
  static constexpr size_t HISTORY=256;MeasurementResult history_[HISTORY];size_t head_=0,count_=0;bool continuous_=false;uint32_t nextSample_=0;
};

#include <Arduino.h>
#include <ArduinoJson.h>
#include "pn5180_transport.h"
#include "rf430_device.h"
#include "measurement.h"

constexpr uint8_t PN5180_NSS=3, PN5180_BUSY=5, PN5180_RST=4;
constexpr uint8_t PN5180_SCK=6, PN5180_MISO=7, PN5180_MOSI=10;

PN5180Transport transport(PN5180_NSS,PN5180_BUSY,PN5180_RST,PN5180_SCK,PN5180_MISO,PN5180_MOSI);
RF430Device rf430(transport);
MeasurementService measurements(rf430);
MeasurementConfig config;
bool continuous=true;
uint32_t nextDetect=0, nextSample=0, lastResultTimestamp=0;
uint8_t detectFailures=0;
uint32_t samplePeriodMs=1000;
uint8_t startFailures=0;

static String uidText(){String s;for(int i=7;i>=0;i--){if(rf430.uid()[i]<16)s+='0';s+=String(rf430.uid()[i],HEX);if(i)s+=':';}s.toUpperCase();return s;}
static void status(const char *message){JsonDocument d;d["type"]="status";d["message"]=message;d["rf430"]=rf430.detected();d["uid"]=rf430.detected()?uidText():"";d["block_size"]=rf430.blockSize();d["block_count"]=rf430.blockCount();Serial.print("JSON ");serializeJson(d,Serial);Serial.println();}
static void result(const MeasurementResult&r){JsonDocument d;d["type"]="measurement";d["timestamp_ms"]=r.timestampMs;d["valid"]=r.valid;d["error"]=r.error;d["adc1_raw"]=r.adc1Raw;d["adc2_raw"]=r.adc2Raw;d["adc0_raw"]=r.adc0Raw;d["sensor_ohm"]=r.sensorResistance;d["r0_ohm"]=config.r0Ohm;d["delta_r_ohm"]=r.deltaR;d["relative_change"]=r.relativeDelta;d["strain_ue"]=r.strain;Serial.print("JSON ");serializeJson(d,Serial);Serial.println();}
static void command(String line){line.trim();if(line=="start"){continuous=true;nextSample=0;status("continuous started");}else if(line=="stop"){continuous=false;measurements.cancel();status("stopped");}else if(line=="single"){continuous=false;if(!measurements.busy())measurements.beginSingle(config);status("single sample started");}else if(line=="baseline"){if(measurements.last().valid)config.r0Ohm=measurements.last().sensorResistance;status("baseline updated");}else if(line.startsWith("interval ")){uint32_t v=line.substring(9).toInt();if(v>=650&&v<=60000){samplePeriodMs=v;nextSample=0;}status("sample interval updated");}else if(line.startsWith("rref ")){double v=line.substring(5).toDouble();if(v>0)config.referenceOhm=v;status("reference updated");}else if(line.startsWith("gf ")){double v=line.substring(3).toDouble();if(v>0)config.gaugeFactor=v;status("gauge factor updated");}else status("unknown command");}

void setup(){Serial.begin(115200);delay(1000);Serial.println("RF430 USB Dashboard Firmware");Serial.println("Wi-Fi disabled; PN5180 measurement enabled.");uint8_t v;encodeAdcConfig(config.adc1,v);Serial.printf("ADC config regression 0x%02X %s\n",v,v==0x18?"PASS":"FAIL");if(!transport.begin())status("PN5180 initialization failed");else status("PN5180 ready");}
void loop(){if(Serial.available())command(Serial.readStringUntil('\n'));if(!rf430.detected()&&millis()>=nextDetect){if(rf430.detect()){detectFailures=0;status("RF430 detected");}else{status("waiting for RF430");if(++detectFailures>=3){status("cycling RF field");transport.recover();detectFailures=0;}}nextDetect=millis()+1000;}MeasurementResult r;if(measurements.poll(config,r)){result(r);lastResultTimestamp=r.timestampMs;nextSample=max(nextSample,millis()+250);if(!r.valid&&(r.error.indexOf("timeout")>=0||r.error.indexOf("read failed")>=0||r.error.indexOf("sampling error")>=0)){status("automatic restart after sampling fault");Serial.flush();delay(100);ESP.restart();}}if(continuous&&rf430.detected()&&!measurements.busy()&&millis()>=nextSample){if(measurements.beginSingle(config)){nextSample=millis()+samplePeriodMs;startFailures=0;}else{status("could not start sample");nextSample=millis()+1000;if(++startFailures>=3){status("automatic restart after start failures");Serial.flush();delay(100);ESP.restart();}}}delay(2);}

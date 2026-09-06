#include <Arduino.h>
#include <WiFi.h>
#include "pn5180_transport.h"
#include "rf430_device.h"
#include "measurement.h"
#include "settings.h"
#include "web_api.h"

constexpr uint8_t PN5180_NSS=3,PN5180_BUSY=5,PN5180_RST=4;
constexpr uint8_t PN5180_SCK=6,PN5180_MISO=7,PN5180_MOSI=10;
PN5180Transport transport(PN5180_NSS,PN5180_BUSY,PN5180_RST,PN5180_SCK,PN5180_MISO,PN5180_MOSI);
RF430Device rf430(transport); MeasurementService measurements(rf430); MeasurementConfig config; SettingsStore settings; WebApi web(rf430,measurements,config,settings);
uint32_t nextScan=0;

static void printUid(){Serial.print("UID = ");for(int i=7;i>=0;i--){if(rf430.uid()[i]<16)Serial.print('0');Serial.print(rf430.uid()[i],HEX);if(i)Serial.print(':');}Serial.println();}
void setup(){Serial.begin(115200);delay(1000);Serial.println("\nRF430 Web Measurement Tool");uint8_t v;encodeAdcConfig(config.adc1,v);Serial.printf("Encoder regression x1 CIC256 SVSS = 0x%02X (%s)\n",v,v==0x18?"PASS":"FAIL");AdcConfig x2;x2.gain=RF430::AdcGain::X2;encodeAdcConfig(x2,v);Serial.printf("Encoder regression x2 CIC256 SVSS = 0x%02X (%s)\n",v,v==0x19?"PASS":"FAIL");settings.begin();settings.load(config);bool pn=transport.begin();Serial.printf("PN5180 = %s\n",pn?"ready":"ERROR");WiFi.mode(WIFI_AP);WiFi.softAP("RF430-Tool");Serial.printf("Wi-Fi AP RF430-Tool: http://%s\n",WiFi.softAPIP().toString().c_str());web.begin();nextScan=millis();}
void loop(){web.loop();if(!rf430.detected()&&millis()>=nextScan){if(rf430.detect()){Serial.println("RF430 detected");printUid();Serial.printf("Block size = %u, Blocks = %u\n",rf430.blockSize(),rf430.blockCount());if(measurements.beginSingle(config))Serial.println("Hardware regression sample started");}else Serial.printf("Waiting for RF430 (error %d)\n",transport.lastError());nextScan=millis()+1000;}delay(2);}

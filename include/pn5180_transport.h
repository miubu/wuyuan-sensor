#pragma once
#include <Arduino.h>
#include <PN5180ISO15693.h>

class PN5180Transport {
 public:
  PN5180Transport(uint8_t nss,uint8_t busy,uint8_t rst,uint8_t sck,uint8_t miso,uint8_t mosi);
  bool begin(); bool recover(); bool inventory(uint8_t uid[8]);
  bool getSystemInfo(const uint8_t uid[8],uint8_t &blockSize,uint8_t &blockCount);
  bool readBlock(const uint8_t uid[8],uint8_t block,uint8_t *data,uint8_t size);
  bool writeBlock(const uint8_t uid[8],uint8_t block,const uint8_t *data,uint8_t size);
  int lastError() const { return error_; } bool ready() const { return ready_; }
 private:
  bool waitBusyLow(uint32_t timeoutMs);
  uint8_t nss_,busy_,rst_,sck_,miso_,mosi_; PN5180ISO15693 nfc_; int error_=0; bool ready_=false;
};

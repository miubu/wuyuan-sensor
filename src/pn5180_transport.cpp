#include "pn5180_transport.h"
#include <SPI.h>

PN5180Transport::PN5180Transport(uint8_t n,uint8_t b,uint8_t r,uint8_t s,uint8_t mi,uint8_t mo):nss_(n),busy_(b),rst_(r),sck_(s),miso_(mi),mosi_(mo),nfc_(n,b,r){}
bool PN5180Transport::waitBusyLow(uint32_t timeout){ uint32_t t=millis(); while(digitalRead(busy_)&&millis()-t<timeout)delay(1); return !digitalRead(busy_); }
bool PN5180Transport::begin(){ pinMode(nss_,OUTPUT);pinMode(busy_,INPUT);pinMode(rst_,OUTPUT);digitalWrite(nss_,HIGH);digitalWrite(rst_,HIGH);SPI.begin(sck_,miso_,mosi_,nss_);delay(20);return recover(); }
bool PN5180Transport::recover(){if(!waitBusyLow(200)){error_=-100;ready_=false;return false;}nfc_.reset();delay(1000);ready_=nfc_.setupRF();error_=ready_?0:-101;return ready_;}
bool PN5180Transport::inventory(uint8_t u[8]){ if(!ready_)return false; auto rc=nfc_.getInventory(u);error_=(int)rc;return rc==ISO15693_EC_OK; }
bool PN5180Transport::getSystemInfo(const uint8_t u[8],uint8_t &s,uint8_t &n){auto rc=nfc_.getSystemInfo(const_cast<uint8_t*>(u),&s,&n);error_=(int)rc;return rc==ISO15693_EC_OK;}
bool PN5180Transport::readBlock(const uint8_t u[8],uint8_t b,uint8_t*d,uint8_t s){auto rc=nfc_.readSingleBlock(const_cast<uint8_t*>(u),b,d,s);error_=(int)rc;return rc==ISO15693_EC_OK;}
bool PN5180Transport::writeBlock(const uint8_t u[8],uint8_t b,const uint8_t*d,uint8_t s){auto rc=nfc_.writeSingleBlock(const_cast<uint8_t*>(u),b,const_cast<uint8_t*>(d),s);error_=(int)rc;return rc==ISO15693_EC_OK;}

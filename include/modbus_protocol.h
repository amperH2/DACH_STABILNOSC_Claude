#pragma once
#include <stdint.h>
#include <stddef.h>
#include <array>
namespace astro_modbus {
constexpr uint16_t crc16(const uint8_t* data,size_t size) {
  uint16_t crc=0xFFFF;
  for(size_t i=0;i<size;++i){crc^=data[i];for(uint8_t b=0;b<8;++b)crc=(crc&1)?(crc>>1)^0xA001:crc>>1;}
  return crc;
}
constexpr std::array<uint8_t,15> pwmFrame(uint8_t addr,uint8_t channel,uint32_t freqCentiHz,uint16_t dutyCentiPct) {
  std::array<uint8_t,15> f{};
  if(addr==0||channel<1||channel>4||freqCentiHz<100||freqCentiHz>20000000||dutyCentiPct>10000)return f;
  uint16_t reg=(channel-1)*3;
  f={{addr,0x10,(uint8_t)(reg>>8),(uint8_t)reg,0,3,6,
      (uint8_t)(freqCentiHz>>24),(uint8_t)(freqCentiHz>>16),(uint8_t)(freqCentiHz>>8),(uint8_t)freqCentiHz,
      (uint8_t)(dutyCentiPct>>8),(uint8_t)dutyCentiPct,0,0}};
  uint16_t c=crc16(f.data(),13);f[13]=c&255;f[14]=c>>8;return f;
}
constexpr bool writeResponseValid(const uint8_t* req,size_t reqSize,const uint8_t* resp,size_t respSize) {
  if(reqSize<8||respSize!=8||resp[0]!=req[0]||(resp[1]&0x80))return false;
  if(req[1]!=5&&req[1]!=6&&req[1]!=0x10)return false;
  for(size_t i=0;i<6;++i)if(resp[i]!=req[i])return false;
  return crc16(resp,6)==(uint16_t)(resp[6]|(resp[7]<<8));
}
}

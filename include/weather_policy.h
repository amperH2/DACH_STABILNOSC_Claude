#pragma once
#include <stdint.h>
#include <stddef.h>
namespace astro_weather {
struct Policy {
 bool rain=true,wind=true,clouds=true,humidity=false,dew=false;
 uint8_t humidityLimit=90,dewTenths=30;
 uint16_t moistureHoldSec=60,cloudHoldSec=180,reopenSec=300;
 constexpr uint8_t mask() const {return (rain?1:0)|(wind?2:0)|(clouds?4:0)|(humidity?8:0)|(dew?16:0);}
 constexpr bool valid() const {return humidityLimit>=50 && humidityLimit<=100 && dewTenths<=100 && moistureHoldSec>=10 && moistureHoldSec<=1800 && cloudHoldSec>=30 && cloudHoldSec<=1800 && reopenSec>=60 && reopenSec<=3600;}
};
constexpr size_t BYTES=14;
constexpr uint16_t checksum(const uint8_t* b,size_t count) {
 uint16_t crc=0xFFFF;
 for(size_t i=0;i<count;++i){crc^=uint16_t(b[i])<<8;for(unsigned bit=0;bit<8;++bit)crc=(crc&0x8000)?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);}
 return crc;
}
constexpr void put16(uint8_t* b,uint16_t v){b[0]=uint8_t(v);b[1]=uint8_t(v>>8);}
constexpr uint16_t get16(const uint8_t* b){return uint16_t(b[0])|(uint16_t(b[1])<<8);}
constexpr void encode(const Policy& p,uint8_t* b){
 b[0]='W';b[1]='P';b[2]=1;b[3]=p.mask();b[4]=p.humidityLimit;b[5]=p.dewTenths;
 put16(b+6,p.moistureHoldSec);put16(b+8,p.cloudHoldSec);put16(b+10,p.reopenSec);put16(b+12,checksum(b,12));
}
constexpr bool decode(const uint8_t* b,size_t length,Policy& out){
 if(length!=BYTES || b[0]!='W'||b[1]!='P'||b[2]!=1||(b[3]&~31)||get16(b+12)!=checksum(b,12))return false;
 Policy p;p.rain=b[3]&1;p.wind=b[3]&2;p.clouds=b[3]&4;p.humidity=b[3]&8;p.dew=b[3]&16;
 p.humidityLimit=b[4];p.dewTenths=b[5];p.moistureHoldSec=get16(b+6);p.cloudHoldSec=get16(b+8);p.reopenSec=get16(b+10);
 if(!p.valid())return false;out=p;return true;
}
// Separate timing flag permits millis()==0 and unsigned wraparound.
struct Stable {
 bool timing=false;uint32_t since=0;
 constexpr bool update(bool good,uint32_t now,uint32_t waitMs){if(!good){timing=false;return false;}if(!timing){timing=true;since=now;}return uint32_t(now-since)>=waitMs;}
};
struct Moisture {
 Stable humidTimer,dewTimer;bool humidLatched=false,dewLatched=false;
 constexpr void update(const Policy& p,bool valid,float humidity,float margin,uint32_t now){
  if(!p.humidity){humidLatched=false;humidTimer.timing=false;}
  else if(!valid){humidTimer.timing=false;} // caller treats missing selected data as UNSAFE
  else if(humidity>=p.humidityLimit){if(humidTimer.update(true,now,uint32_t(p.moistureHoldSec)*1000))humidLatched=true;}
  else {humidTimer.timing=false;if(humidity<=float(p.humidityLimit)-3)humidLatched=false;}
  if(!p.dew){dewLatched=false;dewTimer.timing=false;}
  else if(!valid){dewTimer.timing=false;}
  else if(margin<=float(p.dewTenths)/10){if(dewTimer.update(true,now,uint32_t(p.moistureHoldSec)*1000))dewLatched=true;}
  else {dewTimer.timing=false;if(margin>=float(p.dewTenths)/10+1)dewLatched=false;}
 }
};
constexpr bool selectedReady(const Policy& p,bool wind,bool sky,bool moisture){return (!p.wind||wind)&&(!p.clouds||sky)&&(!(p.humidity||p.dew)||moisture);}
constexpr bool hardBlock(const Policy& p,bool rain,bool wind,bool dense,bool moisture){return (p.rain&&rain)||(p.wind&&wind)||(p.clouds&&dense)||moisture;}
}

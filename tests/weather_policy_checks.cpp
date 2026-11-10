#include "weather_policy.h"
using namespace astro_weather;
constexpr bool everyCombination(){
 for(unsigned mask=0;mask<32;++mask){
  Policy p;p.rain=mask&1;p.wind=mask&2;p.clouds=mask&4;p.humidity=mask&8;p.dew=mask&16;
  uint8_t bytes[BYTES]={};encode(p,bytes);Policy restored;
  if(!decode(bytes,BYTES,restored)||restored.mask()!=mask)return false;
  for(unsigned bad=0;bad<8;++bad){
   bool wind=!(bad&1),sky=!(bad&2),thermo=!(bad&4);
   bool expected=(!p.wind||wind)&&(!p.clouds||sky)&&(!(p.humidity||p.dew)||thermo);
   if(selectedReady(p,wind,sky,thermo)!=expected)return false;
  }
  if(hardBlock(p,true,false,false,false)!=p.rain)return false;
  if(hardBlock(p,false,true,false,false)!=p.wind)return false;
  if(hardBlock(p,false,false,true,false)!=p.clouds)return false;
 }
 return true;
}
constexpr bool corruption(){Policy p;uint8_t bytes[BYTES]={};encode(p,bytes);
 for(unsigned i=0;i<BYTES;++i){bytes[i]^=1;Policy out;if(decode(bytes,BYTES,out))return false;bytes[i]^=1;}
 Policy out;return !decode(bytes,BYTES-1,out);
}
constexpr bool moisture(){
 Policy p;p.humidity=p.dew=true;Moisture m;
 m.update(p,true,95,2,0);m.update(p,true,95,2,59999);if(m.humidLatched||m.dewLatched)return false;
 m.update(p,true,95,2,60000);if(!m.humidLatched||!m.dewLatched)return false;
 m.update(p,true,89,3.5,61000);if(!m.humidLatched||!m.dewLatched)return false;
 m.update(p,true,87,4,62000);if(m.humidLatched||m.dewLatched)return false;
 m.update(p,true,95,2,63000);m.update(p,false,0,0,70000);m.update(p,true,95,2,80000);
 m.update(p,true,95,2,139999);if(m.humidLatched||m.dewLatched)return false;
 m.update(p,true,95,2,140000);if(!m.humidLatched||!m.dewLatched)return false;
 p.humidity=p.dew=false;m.update(p,false,0,0,140001);return !m.humidLatched&&!m.dewLatched;
}
constexpr bool reopen(){Stable s;if(s.update(true,0,300000))return false;if(s.update(true,299999,300000))return false;
 if(!s.update(true,300000,300000))return false;if(s.update(false,300001,300000))return false;
 if(s.update(true,400000,300000))return false;if(!s.update(true,700000,300000))return false;
 Stable wrap;if(wrap.update(true,0xFFFFFFF0u,60))return false;return wrap.update(true,44,60);
}
static_assert(everyCombination(),"All 32 policies independently gate selected measurements and roof weather vetoes");
static_assert(corruption(),"Corruption or truncation of persisted choices must be rejected");
static_assert(moisture(),"Moisture requires sustained data, hysteresis and resets confirmation on missing data");
static_assert(reopen(),"Stable reopening delay survives millis zero/wrap and restarts after bad input");
static_assert(Policy{}.mask()==7,"Default retains rain/wind/cloud protection; humidity and dew remain advisory");

#pragma once
#include "local_do_policy.h"
// Local TCA outputs only; never shares the 8RO roof relay backend.
static constexpr uint8_t localDoEnabledMask=DO1_ENABLED|(DO2_ENABLED<<1)|(DO3_ENABLED<<2)|(DO4_ENABLED<<3)|(DO5_ENABLED<<4)|(DO6_ENABLED<<5)|(DO7_ENABLED<<6);
static const char* const localDoNames[7]={DO1_NAME,DO2_NAME,DO3_NAME,DO4_NAME,DO5_NAME,DO6_NAME,DO7_NAME};
static bool localDoState[7]={};
static bool localDoSupported(){return !OBS_BOARD_8RO && LOCAL_DO_ENABLED;}
static bool localDoAllowed(unsigned id){return astro_do::allowed(OBS_BOARD_8RO,LOCAL_DO_ENABLED,id,localDoEnabledMask);}
static bool localDoReadback(){
  if(!localDoSupported() || !tcaOK)return false;
  uint8_t latch=0;
  if(!tca.readOutput(latch)){tcaOK=false;return false;}
  for(unsigned i=0;i<7;++i)localDoState[i]=(latch & (1U<<i))!=0;
  return true; // output latch, not measurement of external load
}
static bool localDoWrite(unsigned id,bool on){
  if(!localDoAllowed(id) || !tcaOK)return false;
  if(!tca.digitalWrite(id-1,on?HIGH:LOW)){tcaOK=false;return false;}
  // Reading all bits confirms the requested bit and observes DO8 without changing it.
  return localDoReadback() && localDoState[id-1]==on;
}

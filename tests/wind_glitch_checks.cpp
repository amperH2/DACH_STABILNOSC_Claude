#include "wind_monitor.h"
constexpr WindCalibration cal(){WindCalibration c;c.verified=1;return c;}
constexpr WindValidity ready(uint32_t t=1000){WindValidity v;v.adcCalibrated=true;v.present=true;v.voltage=.4f;v.goodCount=64;v.sampledAt=t;return v;}
constexpr bool graceTests(){
 auto c=cal();auto v=ready();
 if(windAcceptSample(c,v,0,true,2000))return false;
 if(!v.holding||v.sampledAt!=1000||v.goodCount!=64)return false;
 if(!windUsable(c,v,3500)||windUsable(c,v,3501))return false;
 // One rejected frame at 1 Hz must not become UNSAFE before the next valid frame.
 auto recovered=v;
 if(!windAcceptSample(c,recovered,.4f,false,3000)||!windUsable(c,recovered,3000)||recovered.holding)return false;
 if(windAcceptSample(c,v,0,true,3000)||v.holding||v.goodCount!=0)return false;
 if(windUsable(c,v,3000))return false;
 if(!windAcceptSample(c,v,.4f,false,4000)||v.goodCount!=1||windUsable(c,v,4000))return false;
 for(unsigned i=1;i<64;++i)windAcceptSample(c,v,.4f,false,4000+i*1000);
 if(!windUsable(c,v,v.sampledAt))return false;
 // No grace before startup completes, after stale data, or without verification.
 v=ready();v.goodCount=63;windAcceptSample(c,v,0,true,2000);if(v.holding)return false;
 v=ready();windAcceptSample(c,v,0,true,7000);if(v.holding)return false;
 v=ready();c.verified=0;windAcceptSample(c,v,0,true,2000);if(v.holding)return false;
 c=cal();v=ready(0xffffff00u);windAcceptSample(c,v,0,true,0x100u);
 if(!v.holding||!windUsable(c,v,0x100u)||windUsable(c,v,0x1000u))return false;
 // Valid high wind is never delayed by the electrical-noise grace.
 v=ready();if(!windAcceptSample(c,v,1.9f,false,2000)||v.holding||v.voltage!=1.9f)return false;
 return true;
}
constexpr bool frameTests(){
 uint16_t a[16]={450,450,450,450,450,450,450,450,450,450,450,450,450,450,450,450};
 a[0]=0;a[15]=4095;auto f=windAdcFrame(a);
 if(!f.usable()||f.rails!=2)return false;
 for(unsigned i=2;i<14;++i)if(f.sorted[i]!=450)return false;
 a[1]=0;if(windAdcFrame(a).usable())return false;
 for(unsigned i=0;i<16;++i)a[i]=0;if(windAdcFrame(a).usable())return false;
 for(unsigned i=0;i<16;++i)a[i]=3000;
 f=windAdcFrame(a);if(!f.usable()||f.sorted[2]!=3000||f.sorted[13]!=3000)return false;
 return true;
}
static_assert(graceTests(),"Bounded grace, no freshness extension, startup, stale, calibration, recovery, wrap, high wind");
static_assert(frameTests(),"Isolated ADC rails filtered; repeated clipping and unplugged sensor rejected; sustained values retained");

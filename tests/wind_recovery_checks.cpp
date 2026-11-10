#include "wind_monitor.h"
constexpr bool recovering(){
 WindCalibration c;c.verified=1;WindValidity v;v.adcCalibrated=true;
 for(unsigned n=1;n<=64;++n)windAcceptSample(c,v,.4f,false,n*1000);
 if(!windUsable(c,v,64000))return false;
 windAcceptSample(c,v,.34f,false,65000);windAcceptSample(c,v,.34f,false,66000);
 if(windStatus(c,v,66000)[0]!='O'||windUsable(c,v,66000))return false;
 for(unsigned n=1;n<64;++n){windAcceptSample(c,v,.4f,false,66000+n*1000);if(windUsable(c,v,66000+n*1000)||windStatus(c,v,66000+n*1000)[0]!='O')return false;}
 windAcceptSample(c,v,.4f,false,130000);if(!windUsable(c,v,130000))return false;
 windAcceptSample(c,v,.34f,false,131000);windAcceptSample(c,v,.34f,false,132000);
 for(unsigned n=1;n<63;++n)windAcceptSample(c,v,.4f,false,132000+n*1000);
 windAcceptSample(c,v,.34f,false,195000);return !windUsable(c,v,195000)&&v.goodCount==0;
}
static_assert(recovering(),"Fault remains OUT_OF_RANGE through recovery; 64 valid samples required; no acceptance of low voltage");

#include "wind_monitor.h"
constexpr WindCalibration verifiedCalibration() {
  WindCalibration c; c.verified=1;c.model[0]='T';return c;
}
constexpr WindValidity sample(float volts=0.4f) {
  WindValidity v;v.present=true;v.adcCalibrated=true;v.voltage=volts;v.goodCount=64;v.sampledAt=1000;return v;
}
constexpr bool invalidFields() {
  auto c=verifiedCalibration();c.top=c.zero;if(windCalibrationValid(c))return false;
  c=verifiedCalibration();c.gain=NAN;if(windCalibrationValid(c))return false;
  c=verifiedCalibration();c.offset=INFINITY;if(windCalibrationValid(c))return false;
  c=verifiedCalibration();c.model[0]=0;if(windCalibrationValid(c))return false;
  c=verifiedCalibration();c.version=2;if(windCalibrationValid(c))return false;
  c=verifiedCalibration();c.ratio=0;if(windCalibrationValid(c))return false;
  c=verifiedCalibration();c.maxMS=0;if(windCalibrationValid(c))return false;
  return true;
}
constexpr bool invalidStates() {
  const auto c=verifiedCalibration();auto v=sample();
  v.present=false;if(windUsable(c,v,1000))return false;
  v=sample();v.adcCalibrated=false;if(windUsable(c,v,1000))return false;
  v=sample();v.saturated=true;if(windUsable(c,v,1000))return false;
  v=sample();v.goodCount=63;if(windUsable(c,v,1000))return false;
  v=sample(NAN);if(windUsable(c,v,1000))return false;
  v=sample(INFINITY);if(windUsable(c,v,1000))return false;
  v=sample(0.2f);if(windUsable(c,v,1000))return false;
  v=sample(2.2f);if(windUsable(c,v,1000))return false;
  return true;
}
constexpr bool conversionChecks() {
 auto c=verifiedCalibration();
 const float cg=windConvert(.645f,c);if(cg<4.961f||cg>4.962f)return false;
 if(windConvert(.396f,c)!=0)return false;
 if(windConvert(.4f,c)!=0 || windConvert(0,c)!=0 || windConvert(2,c)!=32.4f || windConvert(3,c)!=32.4f)return false;
 const float speed=windConvert(.474f,c);if(speed<1.498f||speed>1.499f)return false;
 c.zero=.06f;const float old=windConvert(.4f,c);if(old<5.67f||old>5.69f)return false;
 return true;
}
static_assert(invalidFields(),"Reject malformed/nonfinite calibration");
static_assert(invalidStates(),"Reject missing, stale, saturated, invalid, warming-up samples");
static_assert(conversionChecks(),"Endpoint, clamp and offset regression checks");
static_assert(!windUsable(WindCalibration{},sample(),1000),"CG-FS defaults never auto-verify supply or variant");
static_assert(windUsable(verifiedCalibration(),sample(),1000),"Verified sample usable");
static_assert(windUsable(verifiedCalibration(),sample(),6000),"Freshness boundary inclusive");
static_assert(!windUsable(verifiedCalibration(),sample(),6001),"Stale boundary rejected");
constexpr bool wrapCheck(){auto v=sample();v.sampledAt=0xfffffff0u;return windUsable(verifiedCalibration(),v,0x20u);}
static_assert(wrapCheck(),"millis wraparound");

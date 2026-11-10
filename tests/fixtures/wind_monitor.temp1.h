#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include <float.h>

// CG-FS voltage variant: 7-24 V supply, 0.4-2.0 V = 0-32.4 m/s.
// Confirm the actual variant and supply before verification; valid NVS is retained.
struct WindCalibration {
  uint32_t version=1;
  float gain=1.0f, offset=0.0f, ratio=1.0f;
  float zero=0.4f, top=2.0f, maxMS=32.4f;
  uint32_t verified=0;
  char model[64]="CG-FS 0.4-2.0V";
  uint32_t checksum=0;
};
inline uint32_t windChecksum(const WindCalibration& c) {
  uint32_t h=2166136261u; const uint8_t* b=reinterpret_cast<const uint8_t*>(&c);
  for(size_t i=0;i<offsetof(WindCalibration,checksum);++i) h=(h^b[i])*16777619u;
  return h;
}
constexpr bool windCalibrationValid(const WindCalibration& c) {
  bool terminated=false; for(size_t i=0;i<sizeof(c.model);++i) if(c.model[i]==0) terminated=true;
  return c.version==1 && c.verified<=1 && terminated &&
    (!c.verified || c.model[0]) && isfinite(c.gain) && c.gain>=0.8f && c.gain<=1.2f &&
    isfinite(c.offset) && fabsf(c.offset)<=0.1f && isfinite(c.ratio) && c.ratio>=1 && c.ratio<=10 &&
    isfinite(c.zero) && isfinite(c.top) && isfinite(c.maxMS) && c.zero>=0 &&
    c.top-c.zero>=0.1f && c.top<=3.0f && c.top/c.ratio<=3.0f && c.maxMS>=5 && c.maxMS<=100;
}
inline bool windRecordValid(const WindCalibration& c) {
  return windCalibrationValid(c) && c.checksum==windChecksum(c);
}
constexpr float windConvert(float volts,const WindCalibration& c) {
  if(!isfinite(volts)||!windCalibrationValid(c)) return NAN;
  const float speed=(volts-c.zero)/(c.top-c.zero)*c.maxMS;
  return speed<0?0:(speed>c.maxMS?c.maxMS:speed);
}
constexpr uint32_t WIND_SAMPLE_MAX_AGE_MS=5000;
// One rejected 1 Hz frame may use the previous verified sample for at most
// 2.5 seconds since the last good sample (one missed 1 Hz frame plus jitter).
// A second rejected frame immediately cancels grace. Never permits startup,
// stale data, or an unverified sensor.
constexpr uint32_t WIND_GLITCH_GRACE_MS=2500;
struct WindValidity {
  bool present=false, adcCalibrated=false, saturated=false;
  bool holding=false;
  uint8_t rejectedRun=0;
  uint32_t sampledAt=0, goodCount=0;
  float voltage=NAN;
};
constexpr const char* windStatus(const WindCalibration& c,const WindValidity& v,uint32_t now) {
  if(!windCalibrationValid(c)||!c.verified) return "UNVERIFIED";
  if(!v.adcCalibrated) return "ADC_UNCALIBRATED";
  if(!v.present) return "STARTING";
  if(uint32_t(now-v.sampledAt)>WIND_SAMPLE_MAX_AGE_MS) return "STALE";
  if(v.holding && uint32_t(now-v.sampledAt)>WIND_GLITCH_GRACE_MS) return "OUT_OF_RANGE";
  if(!isfinite(v.voltage)||v.saturated||v.voltage<0.03f||v.voltage>3.0f*c.ratio||
     v.voltage<c.zero-0.05f||v.voltage>c.top+0.05f) return "OUT_OF_RANGE";
  if(v.goodCount<64) return "STARTING";
  return "OK";
}
constexpr bool windUsable(const WindCalibration& c,const WindValidity& v,uint32_t now) {
  const char* state=windStatus(c,v,now); return state[0]=='O' && state[1]=='K';
}

constexpr bool windAcceptSample(const WindCalibration& c,WindValidity& v,
                                float volts,bool saturated,uint32_t now) {
  const bool gap=v.present && uint32_t(now-v.sampledAt)>WIND_SAMPLE_MAX_AGE_MS;
  const bool valid=isfinite(volts)&&!saturated&&volts>=0.03f&&volts<=3.0f*c.ratio&&
                   volts>=c.zero-0.05f&&volts<=c.top+0.05f;
  v.rejectedRun=valid ? 0 : (v.rejectedRun<255?v.rejectedRun+1:255);
  if(!valid && v.rejectedRun==1 && !gap && v.present && v.adcCalibrated && v.goodCount>=64 &&
     windCalibrationValid(c)&&c.verified &&
     uint32_t(now-v.sampledAt)<=WIND_GLITCH_GRACE_MS) {
    v.holding=true;return false; // Do not refresh age or insert a made-up gust sample.
  }
  if(gap)v.goodCount=0;
  v.holding=false;v.present=true;v.sampledAt=now;v.voltage=volts;v.saturated=saturated;
  v.goodCount=valid ? (v.goodCount<64?v.goodCount+1:64) : 0;
  return valid;
}

struct WindAdcFrame {
  uint16_t sorted[16]={};
  unsigned rails=0;
  constexpr bool usable() const {return rails<=2;}
};
constexpr WindAdcFrame windAdcFrame(const uint16_t (&raw)[16]) {
  WindAdcFrame f;
  for(unsigned i=0;i<16;++i) {
    if(raw[i]==0||raw[i]>=4090)++f.rails;
    unsigned pos=i;
    while(pos && f.sorted[pos-1]>raw[i]){f.sorted[pos]=f.sorted[pos-1];--pos;}
    f.sorted[pos]=raw[i];
  }
  return f;
}

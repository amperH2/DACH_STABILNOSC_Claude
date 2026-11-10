#pragma once
#include <stdint.h>
#include <stddef.h>
namespace astro_notify {
constexpr uint32_t kHealthAgeMs=3000,kDedupMs=300000,kJobTtlMs=600000;
constexpr bool due(uint32_t now,uint32_t at){return int32_t(now-at)>=0;}
constexpr bool fresh(uint32_t now,uint32_t stamp){return uint32_t(now-stamp)<=kHealthAgeMs;}
constexpr bool retryable(int code){return code<=0 || code>=500;}
constexpr bool emergencyValid(int retry,int expire){return retry>=30 && expire>=retry && expire<=10800;}
constexpr bool validPhone(const char* n){
 if(!n||n[0]!='+')return false;size_t count=0;
 for(size_t i=1;n[i];++i){if(n[i]<'0'||n[i]>'9')return false;++count;}
 return count>=6 && count<=15;
}
struct Health {
 uint32_t loopAt=0,sensorsAt=0,roofAt=0,generation=0;
 bool completed=false,hardwareOK=false;
 constexpr bool usable(uint32_t now)const{
  return completed && hardwareOK && fresh(now,loopAt)&&fresh(now,sensorsAt)&&fresh(now,roofAt);
 }
};
constexpr uint32_t retryDelay(unsigned attempt){return attempt==1?30000:attempt==2?60000:120000;}
}

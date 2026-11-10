#include "local_do_policy.h"
constexpr bool exhaustive(){
  for(unsigned spare=0;spare<2;++spare)for(unsigned enabled=0;enabled<2;++enabled)
    for(unsigned mask=0;mask<128;++mask)for(unsigned id=0;id<10;++id){
      const bool actual=astro_do::allowed(spare,enabled,id,mask);
      if(spare || !enabled || id<1 || id>7){if(actual)return false;}
      else if(actual!=((mask>>(id-1))&1U))return false;
    }
  return true;
}
static_assert(exhaustive(),"5120 DO ownership/enable/range combinations");

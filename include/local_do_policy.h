#pragma once
#include <stdint.h>
namespace astro_do {
constexpr bool allowed(bool spare,bool enabled,unsigned id,uint8_t mask){
  return !spare && enabled && id>=1 && id<=7 && (mask & (1U<<(id-1)));
}
}

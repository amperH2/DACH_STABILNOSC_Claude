#include "park_safety.h"
constexpr bool checks() {
    astro_park::StablePark p;
    if(p.ready(9000,5000)) return false;
    p.sample(true,false,100); if(p.ready(9000,5000)) return false;
    p.sample(false,true,200); if(p.ready(9000,5000)) return false;
    p.sample(true,true,300);
    if(p.ready(5299,5000) || !p.ready(5300,5000)) return false;
    p.sample(false,true,5301); if(p.ready(5301,5000)) return false;
    p.sample(true,true,5302);
    if(p.ready(10301,5000) || !p.ready(10302,5000)) return false;
    p.sample(true,false,10303); if(p.ready(10303,5000)) return false;
    astro_park::StablePark rollover;
    rollover.sample(true,true,0xfffffff0u);
    if(rollover.ready(0x00001377u,5000)) return false;
    if(!rollover.ready(0x00001378u,5000)) return false;
    return true;
}
static_assert(checks(),"RA/DEC, settling, loss, recovery and millis rollover");

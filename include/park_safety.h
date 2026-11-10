#pragma once
#include <stdint.h>
namespace astro_park {
struct StablePark {
    bool active = false;
    uint32_t since = 0;
    constexpr void sample(bool ra, bool dec, uint32_t now) {
        if (!ra || !dec) { active = false; return; }
        if (!active) { active = true; since = now; }
    }
    constexpr bool ready(uint32_t now, uint32_t settle) const {
        return active && uint32_t(now - since) >= settle;
    }
};
}

#pragma once
#include <stdint.h>
#include <math.h>

enum class RoofState { UNKNOWN, OPEN, CLOSED, OPENING, CLOSING, STOPPED, ERROR };
namespace astro_roof {
constexpr bool closeForWeather(bool manual, bool hardWeather, bool softWeather) {
    return hardWeather || (!manual && softWeather);
}
// Enable only with measured/calibrated values supplied explicitly at build time.
struct DenseCloud {
#if defined(DENSE_CLOUD_TRIP_C) && defined(DENSE_CLOUD_RESET_C) && defined(DENSE_CLOUD_SUSTAIN_MS)
    static constexpr bool enabled=true;
    static_assert(DENSE_CLOUD_RESET_C>DENSE_CLOUD_TRIP_C && DENSE_CLOUD_SUSTAIN_MS>0,
        "Dense cloud calibration requires hysteresis and a positive sustain time");
#else
    static constexpr bool enabled=false;
#endif
    bool latched=false, timing=false;
    uint32_t since=0;
    constexpr void update(bool valid,float normalized,uint32_t now) {
#if defined(DENSE_CLOUD_TRIP_C) && defined(DENSE_CLOUD_RESET_C) && defined(DENSE_CLOUD_SUSTAIN_MS)
        if (!valid) { timing=false; return; } // data loss never clears protection
        if (normalized<DENSE_CLOUD_TRIP_C) {
            if(!timing) { timing=true; since=now; }
            if(uint32_t(now-since)>=DENSE_CLOUD_SUSTAIN_MS) latched=true;
        } else {
            timing=false;
            if(normalized>=DENSE_CLOUD_RESET_C) latched=false;
        }
#endif
    }
};
constexpr bool moving(RoofState s) { return s==RoofState::OPENING || s==RoofState::CLOSING; }
// Starting limit must not erase an accepted move. Conflicting limits are
// handled separately by the existing debounce/fault path.
constexpr RoofState fromLimits(RoofState s, bool closed, bool opened) {
    if (s==RoofState::ERROR || (closed && opened)) return s;
    if (s==RoofState::OPENING) return opened ? RoofState::OPEN : s;
    if (s==RoofState::CLOSING) return closed ? RoofState::CLOSED : s;
    return closed ? RoofState::CLOSED : opened ? RoofState::OPEN :
        s==RoofState::UNKNOWN ? s : RoofState::STOPPED;
}
constexpr bool due(uint32_t now, uint32_t deadline) { return int32_t(now-deadline)>=0; }
constexpr bool weatherAllowsOpen(bool manual, bool safe, bool nina) { return manual || (safe && nina); }
// Physical interlocks (E-STOP/collision/limits/relays) remain outside this policy.
constexpr bool closeParkAllows(bool clearanceConfirmed,bool parkReady) {
    return clearanceConfirmed || parkReady;
}
constexpr bool parkRequiredDuringMotion(RoofState s,bool clearanceConfirmed) {
    return s==RoofState::OPENING || (s==RoofState::CLOSING && !clearanceConfirmed);
}
constexpr bool parkAllows(bool manual, bool mergedPark, bool stableHardwarePark) {
    return stableHardwarePark && (manual || mergedPark);
}
constexpr float cloudNormalize(float raw, float expected, float clear) {
    const float gap = expected>raw ? expected-raw : 0;
    const float normalized = clear+4-gap;
    // A model may be conservative, but can never promote raw overcast to SAFE.
    return normalized<raw ? normalized : raw;
}
constexpr uint32_t MLX_MAX_AGE_MS=3000;
struct MlxSample {
    float sky=NAN, ambient=NAN;
    uint32_t at=0;
    uint8_t bad=0;
    bool present=false;
    constexpr bool update(float object, float enclosure, uint32_t now) {
        // Raw limits from the MLX90614 datasheet, before user offsets.
        const bool good=isfinite(object) && isfinite(enclosure) &&
            object>=-70 && object<=380 && enclosure>=-40 && enclosure<=125;
        if (good) { sky=object; ambient=enclosure; at=now; bad=0; present=true; }
        else if (bad<3) ++bad;
        return good;
    }
    constexpr bool usable(uint32_t now) const {
        return present && bad<3 && uint32_t(now-at)<=MLX_MAX_AGE_MS;
    }
};
constexpr int selectMlx(int active, const MlxSample& first, const MlxSample& second, uint32_t now) {
    const auto& current=active==0 ? first : second;
    const auto& backup=active==0 ? second : first;
    if (current.usable(now) && current.bad==0) return active;
    if (backup.usable(now) && backup.bad==0) return 1-active;
    if (current.usable(now)) return active; // bounded tolerance of transient I2C error
    if (backup.usable(now)) return 1-active;
    return -1;
}
}

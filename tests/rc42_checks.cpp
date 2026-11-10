// Synthetic calibration values test the mechanism, not real-world thresholds.
#define DENSE_CLOUD_TRIP_C 2
#define DENSE_CLOUD_RESET_C 6
#define DENSE_CLOUD_SUSTAIN_MS 100
#include "roof_control.h"
#include "http_request.h"
using namespace astro_roof;
constexpr bool weatherMatrix() {
    for(bool manual:{false,true}) for(bool hard:{false,true}) for(bool soft:{false,true}) {
        if(closeForWeather(manual,hard,soft)!=(hard || (!manual && soft))) return false;
    }
    return true;
}
static_assert(weatherMatrix(),"Rain/wind close in MANUAL; ordinary cloud/MLX loss may be bypassed");
constexpr bool denseTransitions() {
    DenseCloud d;
    d.update(true,1,0); d.update(true,1,99); if(d.latched) return false;
    d.update(true,1,100); if(!d.latched) return false;
    d.update(false,99,200); if(!d.latched) return false;
    d.update(true,5,201); if(!d.latched) return false;
    d.update(true,6,202); if(d.latched) return false;
    d.update(true,1,0xfffffff0); d.update(true,1,0x54); return d.latched;
}
static_assert(denseTransitions(),"Dense cloud sustain, invalid data, hysteresis and rollover");
constexpr bool tokenParsing() {
    astro_http::Request r; r.reset(0);
    for(const char c: "POST /cmd HTTP/1.1\r\nX-Command-Token: secret\r\nContent-Length: 7\r\n\r\ndo=stop")
        if(c) r.feed(c,1);
    return r.complete() && astro_http::Request::equal(r.csrf,"secret") &&
        astro_http::Request::equal(r.body,"do=stop");
}
static_assert(tokenParsing(),"POST STOP body and command header survive bounded parser");
constexpr bool duplicateToken() {
    astro_http::Request r; r.reset(0);
    for(const char c: "POST /cmd HTTP/1.1\r\nX-Command-Token: a\r\nX-Command-Token: b\r\n\r\n")
        if(c) r.feed(c,1);
    return r.failed();
}
static_assert(duplicateToken(),"Ambiguous token headers must fail closed");

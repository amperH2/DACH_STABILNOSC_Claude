#include "roof_control.h"
#ifndef LINE_MAX
#define LINE_MAX 2048
#endif
#include "http_request.h"
#include "park_safety.h"
using namespace astro_roof;
constexpr bool motionSequence() {
    auto s=RoofState::OPENING;
    for(int i=0;i<5;++i) { s=fromLimits(s,true,false); if(s!=RoofState::OPENING) return false; }
    s=fromLimits(s,false,false); if(s!=RoofState::OPENING) return false;
    s=fromLimits(s,false,true); if(s!=RoofState::OPEN) return false;
    s=RoofState::CLOSING;
    for(int i=0;i<5;++i) { s=fromLimits(s,false,true); if(s!=RoofState::CLOSING) return false; }
    s=fromLimits(s,false,false); if(s!=RoofState::CLOSING) return false;
    return fromLimits(s,true,false)==RoofState::CLOSED;
}
static_assert(motionSequence(),"Starting contacts and travel preserve both directions until destination");
static_assert(fromLimits(RoofState::ERROR,true,false)==RoofState::ERROR,"Fault never auto-clears from limit");
static_assert(fromLimits(RoofState::CLOSING,true,true)==RoofState::CLOSING,"Conflict is handled by fault debounce, not a false arrival");
static_assert(fromLimits(RoofState::STOPPED,false,false)==RoofState::STOPPED,"Mid-travel STOP retained");
static_assert(fromLimits(RoofState::UNKNOWN,false,false)==RoofState::UNKNOWN,"Unknown position is not movement");
static_assert(due(0,0),"Deadline zero is a real deadline; separate pulseActive flag");
static_assert(!due(0xfffffff0u,0x00000020u) && due(0x20u,0x20u) && due(0x21u,0x20u),"Pulse deadline wraps safely");
constexpr bool manualMatrix() {
    for(int manual=0;manual<2;++manual) for(int safe=0;safe<2;++safe) for(int nina=0;nina<2;++nina)
        if(weatherAllowsOpen(manual,safe,nina)!=(manual || (safe && nina))) return false;
    return true;
}
static_assert(manualMatrix(),"Manual only overrides weather and NINA policy");
static_assert(parkAllows(true,false,true) && !parkAllows(false,false,true),"Manual ignores network atpark disagreement, AUTO preserves it");
static_assert(!parkAllows(true,true,false) && !parkAllows(true,false,false),"Manual never bypasses hardware RA/DEC settling");
constexpr bool mlxChecks() {
    MlxSample first,second;
    if(selectMlx(0,first,second,0)!=-1) return false;
    first.update(-15,8,0); second.update(-14,8,0);
    if(!first.usable(3000) || first.usable(3001)) return false;
    // Partial failure is a failed pair and never refreshes its time.
    first.update(NAN,8,1000); if(first.bad!=1 || first.at!=0) return false;
    second.update(-14,8,1000);
    if(selectMlx(0,first,second,1000)!=1) return false;
    if(selectMlx(1,first,second,1000)!=1) return false; // no gratuitous return
    first.update(-15,NAN,2000); first.update(NAN,NAN,3000);
    if(first.usable(3000)) return false;
    second.update(INFINITY,8,2000); second.update(-14,INFINITY,3000); second.update(NAN,8,4000);
    if(selectMlx(1,first,second,4000)!=-1) return false;
    first.update(-15,8,4001); if(selectMlx(1,first,second,4001)!=0) return false;
    first.update(-71,8,4002); if(first.bad!=1 || first.at!=4001) return false;
    first.update(-15,126,4003); if(first.bad!=2) return false;
    MlxSample wrap; wrap.update(-20,8,0xfffffff0u);
    return wrap.usable(0x20u) && !wrap.usable(0x1000u);
}
static_assert(mlxChecks(),"Pair validity, partial failures, age, both failover directions, recovery, bounds and wrap");
constexpr bool cloudChecks() {
    for(int raw=-5;raw<=60;++raw) for(int expected=-5;expected<=60;++expected) {
        const float corrected=cloudNormalize(raw,expected,16);
        if(corrected>raw) return false;
        if(raw<3 && corrected>=3) return false;
    }
    return cloudNormalize(2,2,16)==2 && cloudNormalize(20,20,16)==20;
}
static_assert(cloudChecks(),"Neither bad learning nor normalization can promote raw critical cloud to SAFE");
constexpr void feed(astro_http::Request& r,const char* text,uint32_t now=100) {
    for(size_t i=0;text[i];++i) r.feed(text[i],now);
}
constexpr bool httpChecks() {
    astro_http::Request r; r.reset(100);
    feed(r,"GET /api/v1/safetymonitor/0/issafe HTTP/1.1\r\nHost: esp\r\n\r\n");
    if(!r.complete() || r.failed()) return false;
    r.reset(100); feed(r,"PUT /api/v1/dome/0/closeshutter HTTP/1.1\r\nContent-Length: 3\r\nAuthorization: Basic test\r\n\r\na=");
    if(r.complete() || r.failed() || r.bodyLength!=2) return false;
    feed(r,"1"); if(!r.complete() || !astro_http::Request::equal(r.body,"a=1")) return false;
    r.reset(100); feed(r,"POST /api/wind/calibration HTTP/1.1\r\nContent-Length: 2048\r\n\r\n");
    if(!r.failed()) return false;
    r.reset(100); feed(r,"GET / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\n");
    if(!r.failed()) return false;
    r.reset(100); feed(r,"GET / HTTP/1.1\r\nContent-Length: -1\r\n\r\n"); if(!r.failed()) return false;
    r.reset(100); feed(r,"GET / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"); if(!r.failed()) return false;
    r.reset(100); feed(r,"PUT / HTTP/1.1\r\nExpect: 100-continue\r\ncontent-length: 1\r\n\r\n");
    if(r.complete() || r.failed() || !r.expectContinue || r.phase!=2) return false;
    feed(r,"1"); if(!r.complete()) return false;
    r.reset(100); feed(r,"PUT / HTTP/1.1\r\nExpect: 100-Continue \t\r\nContent-Length: 1\r\n\r\n1");
    if(!r.complete() || !r.expectContinue) return false;
    r.reset(0xfffffff0u); if(r.expired(0x20u) || !r.expired(0x800u)) return false;
    r.reset(100); r.lastByte=1900; if(!r.expired(2100)) return false; // total age despite drip feed
    r.reset(100); if(!r.expired(1100)) return false; // idle bound
    r.reset(100); for(size_t i=0;i<astro_http::kLineMax+1;++i) r.feed('x',100);
    if(!r.failed()) return false;
    // A stalled socket's state has no effect on a second complete STOP request.
    r.reset(100); feed(r,"GET / HTTP/1.1\r\nHost:");
    astro_http::Request stop; stop.reset(100);
    feed(stop,"GET /cmd?do=stop&ajax=1 HTTP/1.1\r\n\r\n");
    return !r.complete() && stop.complete();
}
static_assert(httpChecks(),"Incremental Alpaca/UI, partial body, strict bounds, malformed input, timeouts, rollover and concurrent STOP");
static_assert(astro_http::kLineMax==1024,"Parser must not collide with system LINE_MAX macro");

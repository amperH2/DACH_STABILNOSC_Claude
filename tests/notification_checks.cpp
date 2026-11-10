#include "notification_policy.h"
using namespace astro_notify;
constexpr bool healthChecks(){
 Health h;h.completed=true;h.hardwareOK=true;h.loopAt=h.sensorsAt=h.roofAt=1000;
 if(!h.usable(1000)||!h.usable(4000)||h.usable(4001))return false;
 h.hardwareOK=false;if(h.usable(1000))return false;h.hardwareOK=true;
 h.sensorsAt=1;if(h.usable(4000))return false;
 h.sensorsAt=1000;h.roofAt=1;if(h.usable(4000))return false;
 h.loopAt=h.roofAt=h.sensorsAt=0xfffffff0u;return h.usable(0x20u);
}
static_assert(healthChecks(),"Heartbeat requires fresh completed loop, sensors and roof service, including wraparound");
static_assert(!Health{}.usable(0),"No heartbeat before completed control iteration");
static_assert(retryable(-1)&&retryable(503)&&!retryable(401)&&!retryable(429)&&!retryable(200),"Bounded retries only for transient errors");
static_assert(emergencyValid(60,1800)&&!emergencyValid(29,1800)&&!emergencyValid(60,10801),"Pushover emergency bounds");
static_assert(retryDelay(1)>=5000&&retryDelay(2)>=5000,"No rapid provider retries");
static_assert(validPhone("+48123456789")&&!validPhone("+48XXXXXXXXX")&&!validPhone("48123456789")&&!validPhone("+123"),"Reject placeholder/invalid SMS numbers");

"""Compile-time scenarios execute extracted production roofClose, without hardware."""
from pathlib import Path
import sys,subprocess,tempfile
from check_wind import block
p=Path(__file__).resolve().parents[1];source=(p/'src/main.cpp').read_text(encoding='utf-8')
prefix=r"""
#include "roof_control.h"
#include "operator_policy.h"
#define ROOF_BLOCK_REDUNDANT_COMMANDS 1
#define ROOF_INTERLOCK_RELAYS 1
#define USE_OTA 0
#define USE_WIFI 1
#define COLLISION_SENSOR_ENABLED 0
#define COLLISION_POWER_RELAY_ENABLED 0
#define F(x) x
struct Text {
 const char* data="";
 constexpr Text()=default;constexpr Text(const char* p):data(p){}
 constexpr Text& operator=(const char* p){data=p;return *this;}
};
constexpr Text operator+(const char*,Text){return Text{};}
struct Harness {
 enum class RoofFaultKind { COMMUNICATION };
 bool closed=false,opened=true,modbusRelayOK=true,g_mountEstop=false,rawEstop=false,collision=true;
 bool clearance=true,park=false,pulseActive[3]={},pulseAck=true;
 static constexpr int RELAY_ROOF_OPEN=0,RELAY_ROOF_CLOSE=1,RELAY_ROOF_STOP=2,NOTIFY_CRITICAL=2;
 RoofState roofState=RoofState::OPEN;
 Text g_lastRoofBlockReason,g_roofMotionFault;
 unsigned g_stopEpoch=0,roofMoveStartMs=0;bool closePulsed=false;
 struct SerialStub { constexpr void println(const char*) {} } Serial;
 constexpr bool limitClosedActive(){return closed;}
 constexpr bool limitOpenActive(){return opened;}
 constexpr bool mountLimitRawTripped(){return rawEstop;}
 constexpr bool mountProtectionBlocksClose(){return astro_policy::mountBlocksClose(clearance,false,g_mountEstop||rawEstop);}
 constexpr bool clearanceAllowsClose(){return astro_roof::closeParkAllows(clearance,park);}
 constexpr bool collisionClearForRoofMove(){return collision;}
 constexpr bool blockRoofCommand(Text){return false;}
 constexpr void notifyEvent(int,const char*,const char*){}
 constexpr void updateRoofStateFromLimits(){}
 constexpr void illumForceOff(const char*){}
 constexpr void collisionPowerOn(){}
 constexpr void roofRelaysOff(){}
 constexpr void eventLogEx(const char*,const char*,const char*){}
 constexpr bool relayPulse(int){closePulsed=true;return pulseAck;}
 constexpr void roofFaultStop(Text,RoofFaultKind){roofState=RoofState::ERROR;}
 constexpr unsigned millis(){return 100;}
 constexpr void roofMotionWatchStart(){}
"""
suffix=r"""
};
constexpr bool unparkedOfflinePc(){Harness h;return h.roofClose()&&h.closePulsed&&h.roofState==RoofState::CLOSING;}
constexpr bool geometryOff(){Harness h;h.clearance=false;return !h.roofClose()&&!h.closePulsed;}
constexpr bool parkedFallback(){Harness h;h.clearance=false;h.park=true;return h.roofClose()&&h.closePulsed;}
constexpr bool physicalGates(){
 for(int i=0;i<9;++i){Harness h;
  switch(i){case 0:h.g_mountEstop=true;h.clearance=false;h.park=true;break;case 1:h.rawEstop=true;h.clearance=false;h.park=true;break;
   case 2:h.collision=false;break;case 3:h.closed=true;break;
   case 4:h.modbusRelayOK=false;break;case 5:h.roofState=RoofState::ERROR;break;
   case 6:h.roofState=RoofState::OPENING;break;case 7:h.pulseActive[2]=true;break;
   case 8:h.pulseActive[0]=true;break;}
  if(h.roofClose()||h.closePulsed)return false;
 }return true;
}
constexpr bool alreadyClosed(){Harness h;h.closed=true;h.opened=false;h.roofState=RoofState::CLOSED;return h.roofClose()&&!h.closePulsed;}
static_assert(unparkedOfflinePc(),"Confirmed clearance allows CLOSE without park/PC");
static_assert(geometryOff(),"No confirmed clearance still requires park");
static_assert(parkedFallback(),"Legacy parked installation remains usable");
constexpr bool mountTripWithClearance(){for(int i=0;i<2;++i){Harness h;h.g_mountEstop=(i==0);h.rawEstop=(i==1);if(!h.roofClose()||!h.closePulsed)return false;}return true;}
static_assert(mountTripWithClearance(),"Mount trip cannot defeat confirmed clearance weather CLOSE");
static_assert(physicalGates(),"Clearance does not bypass physical/relay/STOP gates");
static_assert(alreadyClosed(),"CLOSED is idempotent, never a second impulse");
static_assert(astro_roof::parkRequiredDuringMotion(RoofState::OPENING,true),"OPEN retains park");
static_assert(!astro_roof::parkRequiredDuringMotion(RoofState::CLOSING,true),"CLOSE continues without park");
static_assert(astro_roof::parkRequiredDuringMotion(RoofState::CLOSING,false),"Unconfirmed geometry retains park");
"""
with tempfile.TemporaryDirectory() as d:
 f=Path(d)/'close.cpp';f.write_text(prefix+block(source,'roofClose').replace('static bool','constexpr bool',1)+suffix)
 subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only','-I'+str(p/'include'),str(f)],check=True)
close=block(source,'roofClose');opening=block(source,'roofOpen');tick=block(source,'roofTick')
assert close.count('!clearanceAllowsClose()')==3
assert '!parkGuardAllowsClose()' in opening and 'clearanceAllowsClose' not in opening
assert 'parkRequiredDuringMotion(roofState,CLOSE_CLEARANCE_CONFIRMED)' in tick
alerts=block(source,'weatherResponseAlertsTick')
assert '#if USE_TELEGRAM' not in alerts and 'NOTIFY_CRITICAL' in alerts
transport=block(source,'weatherResponseNotify')
assert transport.index('notifyEvent(')<transport.index('#if USE_TELEGRAM')
assert 'telegramSend(String(title)+": "+message,true)' in transport
assert 'hardMask & ~announcedHard' in alerts
assert '300000UL' in alerts and 'limitClosedActive() && !limitOpenActive()' in alerts
sensors=block(source,'readSensorsAndSafety')
assert sensors.index('if (roofClose())')<sensors.index('weatherResponseAlertsTick();')
assert 'if (!ninaIsActive()' not in close and 'PARK_ALPACA_HOST' not in close
print('RC4.3 PASS: extracted CLOSE scenarios, unparked/offline PC, geometry fallback, physical vetoes, OPEN park, motion monitor and notification ordering')

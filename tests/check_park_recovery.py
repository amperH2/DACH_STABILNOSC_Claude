from pathlib import Path
import sys,subprocess
from check_wind import block
p=Path(__file__).resolve().parents[1];s=(p/'src/main.cpp').read_text(encoding='utf-8')
cpp=r'''
#include "roof_control.h"
#include "park_safety.h"
#define LOW 0
#define PARK_SENSOR_PIN 1
#define PARK_SENSOR_DEC_PIN 2
#define PARK_SETTLE_MS 5000u
struct Text{constexpr Text& operator=(const char*){return *this;}};
struct Harness {
 enum class RoofFaultKind { NONE,COMMUNICATION,PHYSICAL,STOP_UNCONFIRMED,PARK_LOST };
 RoofState roofState=RoofState::ERROR;
 RoofFaultKind g_roofFaultKind=RoofFaultKind::PARK_LOST;
 astro_park::StablePark g_stablePark{true,0};
 uint32_t now=6000,g_stopEpoch=1;
 bool ra=true,dec=true,closed=false,opened=false,clearance=true,mount=false,collision=true;
 bool modbusRelayOK=true,poll=true,loseRaDuringPoll=false,tripDuringPoll=false,stopDuringPoll=false,newFaultDuringPoll=false,stateDuringPoll=false;
 bool g_stopAttempted=true,g_stopCommandConfirmed=true,g_stopReleaseConfirmed=true;
 bool pulseActive[3]={},modbusRelayActualState[3]={},g_pendingAutoClose=true;
 Text g_roofMotionFault;
 static constexpr unsigned RELAY_ROOF_OPEN=0,RELAY_ROOF_CLOSE=1,RELAY_ROOF_STOP=2;
 constexpr uint32_t millis(){return now;}
 constexpr int digitalRead(int pin){return (pin==1?ra:dec)?0:1;}
 constexpr void sampleHardwarePark(){g_stablePark.sample(ra,dec,now);}
 constexpr bool limitClosedActive(){return closed;}
 constexpr bool limitOpenActive(){return opened;}
 constexpr bool clearanceAllowsClose(){return clearance;}
 constexpr bool mountProtectionBlocksClose(){return mount;}
 constexpr bool collisionClearForRoofMove(){return collision;}
 constexpr bool modbusRelayPollNow(){if(stopDuringPoll)++g_stopEpoch;if(newFaultDuringPoll)g_roofFaultKind=RoofFaultKind::PHYSICAL;if(stateDuringPoll)roofState=RoofState::STOPPED;if(loseRaDuringPoll)ra=false;if(tripDuringPoll)mount=true;return poll;}
 constexpr bool blockRoofCommand(const char*){return false;}
 template<class... T>constexpr void eventLogEx(T...){}
'''+block(s,'parkLossRecoveryReady').replace('static bool','constexpr bool',1)+'\n'+block(s,'acknowledgeParkLossFault').replace('static bool','constexpr bool',1)+r'''
};
#if PARK_GUARD_ENABLED
constexpr bool positions(){for(int i=0;i<3;++i){Harness h;h.closed=i==0;h.opened=i==1;if(!h.acknowledgeParkLossFault()||h.g_pendingAutoClose||h.g_roofFaultKind!=Harness::RoofFaultKind::NONE)return false;if(h.roofState!=(i==0?RoofState::CLOSED:i==1?RoofState::OPEN:RoofState::STOPPED))return false;}return true;}
constexpr bool vetoes(){for(int i=0;i<25;++i){Harness h;switch(i){case 0:h.ra=false;break;case 1:h.dec=false;break;case 2:h.g_stablePark.since=h.now-4999;break;case 3:h.clearance=false;break;case 4:h.closed=h.opened=true;break;case 5:h.mount=true;break;case 6:h.collision=false;break;case 7:h.modbusRelayOK=false;break;case 8:h.g_stopAttempted=false;break;case 9:h.g_stopCommandConfirmed=false;break;case 10:h.g_stopReleaseConfirmed=false;break;case 11:h.pulseActive[0]=true;break;case 12:h.pulseActive[1]=true;break;case 13:h.pulseActive[2]=true;break;case 14:h.poll=false;break;case 15:h.modbusRelayActualState[0]=true;break;case 16:h.modbusRelayActualState[1]=true;break;case 17:h.modbusRelayActualState[2]=true;break;case 18:h.loseRaDuringPoll=true;break;case 19:h.tripDuringPoll=true;break;case 20:h.g_roofFaultKind=Harness::RoofFaultKind::PHYSICAL;break;case 21:h.g_roofFaultKind=Harness::RoofFaultKind::STOP_UNCONFIRMED;break;case 22:h.stopDuringPoll=true;break;case 23:h.newFaultDuringPoll=true;break;case 24:h.stateDuringPoll=true;break;}if(h.acknowledgeParkLossFault()||h.roofState!=(i==24?RoofState::STOPPED:RoofState::ERROR))return false;}return true;}
constexpr bool bounce(){Harness h;h.ra=false;if(h.acknowledgeParkLossFault())return false;h.ra=true;if(h.acknowledgeParkLossFault())return false;h.now+=4999;if(h.acknowledgeParkLossFault())return false;h.now+=1;return h.acknowledgeParkLossFault();}
constexpr bool wrap(){Harness h;h.g_stablePark.since=0xfffffff0;h.now=0xfffffff0u+5000u;return h.acknowledgeParkLossFault();}
static_assert(positions()&&vetoes()&&bounce()&&wrap(),"PARK recovery: positions, 25 vetoes, full settle after bounce, timer wrap");
#else
constexpr bool disabled(){Harness h;return !h.acknowledgeParkLossFault()&&h.roofState==RoofState::ERROR;}
static_assert(disabled(),"PARK recovery disabled without configured PARK inputs");
#endif
'''
out=p/'.pio/offline';out.mkdir(parents=True,exist_ok=True);f=out/'park_recovery.cpp';f.write_text(cpp,encoding='utf-8')
for enabled in [0,1]:subprocess.run([sys.argv[1],'-std=gnu++17',f'-DPARK_GUARD_ENABLED={enabled}','-fsyntax-only','-I'+str(p/'include'),str(f)],check=True)
assert 'if(cmdOk) cmdOk=g_roofFaultKind==RoofFaultKind::PARK_LOST ? acknowledgeParkLossFault() : acknowledgeCommunicationFault();' in s
assert 'RoofFaultKind::PARK_LOST);' in block(s,'roofTick')
assert 'acknowledgeParkLossFault' not in block(s,'readSensorsAndSafety')
assert 'roofClose(' not in block(s,'acknowledgeParkLossFault')
print('PASS production PARK acknowledgment: full debounce, 25 vetoes, post-Modbus recheck, no motion/automatic invocation, disabled inputs and timer wrap')

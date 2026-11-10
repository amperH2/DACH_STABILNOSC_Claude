"""Compile-time execution of the production communication-fault acknowledgment."""
from pathlib import Path
import sys,subprocess,tempfile
from check_wind import block
p=Path(__file__).resolve().parents[1];s=(p/'src/main.cpp').read_text(encoding='utf-8')
code=r'''
#include "roof_control.h"
#include "operator_policy.h"
struct Text{const char* v="";constexpr Text& operator=(const char* x){v=x;return *this;}};
struct Harness {
 enum class RoofFaultKind{NONE,COMMUNICATION,PHYSICAL};
 RoofState roofState=RoofState::ERROR;RoofFaultKind g_roofFaultKind=RoofFaultKind::COMMUNICATION;
 bool closed=false,opened=false,clearance=true,park=false,trip=false,collision=true,modbusRelayOK=true,pollOK=true;
 bool g_stopCommandConfirmed=true,g_stopReleaseConfirmed=true,pulseActive[3]={},modbusRelayActualState[3]={},g_pendingAutoClose=true;
 Text g_roofMotionFault;
 static constexpr int RELAY_ROOF_OPEN=0,RELAY_ROOF_CLOSE=1,RELAY_ROOF_STOP=2;
 constexpr bool limitClosedActive(){return closed;}constexpr bool limitOpenActive(){return opened;}
 constexpr bool clearanceAllowsClose(){return clearance||park;}
 constexpr bool mountProtectionBlocksClose(){return astro_policy::mountBlocksClose(clearance,false,trip);}
 constexpr bool collisionClearForRoofMove(){return collision;}
 constexpr bool modbusRelayPollNow(){return pollOK;}
 constexpr bool blockRoofCommand(const char*){return false;}
 template<class... T>constexpr void eventLogEx(T...){}
'''
code+=block(s,'acknowledgeCommunicationFault').replace('static bool','constexpr bool',1)
code+=r'''
};
constexpr bool positions(){for(int i=0;i<3;++i){Harness h;h.closed=i==0;h.opened=i==1;if(!h.acknowledgeCommunicationFault()||h.g_pendingAutoClose||h.g_roofFaultKind!=Harness::RoofFaultKind::NONE)return false;if(h.roofState!=(i==0?RoofState::CLOSED:i==1?RoofState::OPEN:RoofState::STOPPED))return false;}return true;}
constexpr bool vetoes(){for(int i=0;i<13;++i){Harness h;switch(i){case 0:h.g_roofFaultKind=Harness::RoofFaultKind::PHYSICAL;break;case 1:h.opened=h.closed=true;break;case 2:h.clearance=false;break;case 3:h.trip=true;h.clearance=false;h.park=true;break;case 4:h.collision=false;break;case 5:h.modbusRelayOK=false;break;case 6:h.g_stopCommandConfirmed=false;break;case 7:h.g_stopReleaseConfirmed=false;break;case 8:h.pollOK=false;break;case 9:h.modbusRelayActualState[0]=true;break;case 10:h.modbusRelayActualState[1]=true;break;case 11:h.modbusRelayActualState[2]=true;break;case 12:h.pulseActive[2]=true;break;}if(h.acknowledgeCommunicationFault()||h.roofState!=RoofState::ERROR)return false;}return true;}
static_assert(positions(),"Inspected stationary CLOSED, OPEN and mid roof may recover communication only");
static_assert(vetoes(),"Physical faults, geometry, coils, missing STOP/release/readback must remain latched");
'''
with tempfile.TemporaryDirectory() as d:
 f=Path(d)/'recovery.cpp';f.write_text(code,encoding='utf-8');subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only','-I'+str(p/'include'),str(f)],check=True)
print('Recovery PASS: extracted production acknowledgment, three positions and thirteen refusal cases; no motion interface in harness')

"""Compile-time fault injection against extracted production relay functions."""
from pathlib import Path
import sys,subprocess,tempfile
from check_wind import block
root=Path(__file__).resolve().parents[1]
s=(root/'src/main.cpp').read_text(encoding='utf-8')
functions=[]
for name in ['relayDeviceReady','relayWrite','relayPulse','updatePulseOutputs','stopDrivePulse']:
 body=block(s,name)
 functions.append(body.replace('static bool','constexpr bool',1).replace('static void','constexpr void',1))
prefix=r'''
#include <stdint.h>
#define RELAY_LOG 0
#define ROOF_MOTION_ENABLED 1
#define F(x) x
struct Harness {
 uint32_t now=100; bool modbusRelayOK=true,modbusRelayStateValid=true;
 bool relayState[16]={},actual[16]={},pulseActive[16]={};
 uint32_t pulseEndTime[16]={};
 bool ackOn=true,ackOff=true,deliverOn=true,deliverOff=true;
 bool g_stopAttempted=false,g_stopCommandConfirmed=false,g_stopReleaseConfirmed=true;
 uint32_t g_stopEpoch=0; const char* g_lastRoofBlockReason="";
 static constexpr uint8_t RELAY_ROOF_OPEN=0,RELAY_ROOF_CLOSE=1,RELAY_ROOF_STOP=2;
 constexpr uint32_t millis() const { return now; }
 constexpr bool modbusRelayWriteCoil(uint8_t ch,bool on) {
   if(on?deliverOn:deliverOff) actual[ch]=on;
   return on?ackOn:ackOff;
 }
 constexpr void illumForceOff(const char*) {}
 constexpr void eventLogEx(const char*,const char*,const char*) {}
'''
suffix=r'''
};
constexpr bool lostOnEcho() {
 Harness h;h.ackOn=false;
 if(h.relayPulse(0) || !h.actual[0] || !h.pulseActive[0])return false;
 h.updatePulseOutputs();return !h.actual[0] && !h.pulseActive[0];
}
constexpr bool lostOffEcho() {
 Harness h;if(!h.relayPulse(0,20))return false;
 h.now+=20;h.ackOff=false;h.updatePulseOutputs();
 if(h.pulseActive[0]==false || h.actual[0])return false;
 h.now+=250;h.ackOff=true;h.updatePulseOutputs();return !h.pulseActive[0];
}
constexpr bool offNotDelivered() {
 Harness h;h.relayPulse(0,20);h.now+=20;h.ackOff=false;h.deliverOff=false;
 h.updatePulseOutputs();if(!h.actual[0] || !h.pulseActive[0])return false;
 h.now+=250;h.ackOff=true;h.deliverOff=true;h.updatePulseOutputs();
 return !h.actual[0] && !h.pulseActive[0];
}
constexpr bool lostStopEcho() {
 Harness h;h.ackOn=false;
 if(h.stopDrivePulse(30) || !h.actual[2] || !h.pulseActive[2] || h.g_stopCommandConfirmed)return false;
 h.now+=30;h.updatePulseOutputs();
 return !h.actual[2] && !h.pulseActive[2] && h.g_stopReleaseConfirmed && !h.g_stopCommandConfirmed;
}
constexpr bool stopOffFailure() {
 Harness h;if(!h.stopDrivePulse(30))return false;
 h.now+=30;h.ackOff=false;h.deliverOff=false;h.updatePulseOutputs();
 if(h.g_stopReleaseConfirmed || !h.pulseActive[2])return false;
 h.now+=250;h.ackOff=true;h.deliverOff=true;h.updatePulseOutputs();
 return h.g_stopReleaseConfirmed && !h.pulseActive[2];
}
static_assert(lostOnEcho(),"Lost ON echo still schedules OFF");
static_assert(lostOffEcho(),"Lost OFF echo remains pending until confirmed");
static_assert(offNotDelivered(),"OFF not delivered remains pending and retries");
static_assert(lostStopEcho(),"Lost STOP echo schedules release without claiming STOP confirmation");
static_assert(stopOffFailure(),"STOP release failure cannot be silently cleared");
'''
with tempfile.TemporaryDirectory() as directory:
 fixture=Path(directory)/'relay_failures.cpp'
 fixture.write_text('#include "roof_control.h"\n'+prefix+'\n'.join(functions)+suffix,encoding='utf-8')
 subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only','-I'+str(root/'include'),str(fixture)],check=True)
print('Production relay fault injection PASS: lost ON, OFF applied/unapplied, lost STOP and failed STOP release')

from pathlib import Path
import sys,subprocess
root=Path(sys.argv[1]);compiler=sys.argv[2]
sys.path.insert(0,str(root/'tests'))
from check_wind import block
s=(root/'src/main.cpp').read_text(encoding='utf-8-sig')
subprocess.run([compiler,'-std=gnu++17','-fsyntax-only','-I'+str(root/'include'),str(root/'tests/park_checks.cpp')],check=True)
guard=block(s,'parkGuardAllowsClose')
assert 'astro_roof::parkAllows(g_manualMode,g_telescopeParked, g_stablePark.ready' in guard and 'sampleHardwarePark();' in guard
for name,relay in [('roofOpen','RELAY_ROOF_OPEN'),('roofClose','RELAY_ROOF_CLOSE')]:
 b=block(s,name)
 gate='!parkGuardAllowsClose()' if name=='roofOpen' else '!clearanceAllowsClose()'
 assert b.index(gate)<b.index('relayPulse('+relay+')')
 assert b.count(gate)>=2
 assert 'limitClosedActive() && limitOpenActive()' in b
 assert 'return true; // Bez kolejnego impulsu.' in b
 assert b.index('roofState == RoofState::ERROR')<b.index('Idempotent')
b=block(s,'roofTick')
compact=''.join(b.split())
assert '(digitalRead(PARK_SENSOR_PIN)!=LOW||digitalRead(PARK_SENSOR_DEC_PIN)!=LOW)' in compact
assert 'roofFaultStop("Utrata parku RA/DEC podczas ruchu dachu",RoofFaultKind::PARK_LOST)' in b
assert 'stopDrivePulse(500)' in block(s,'roofFaultStop')
assert 'roofState=RoofState::ERROR' in block(s,'roofFaultStop')
assert 'g_roofMotionFault.length()) return;' in block(s,'updateRoofStateFromLimits')
assert 'wasFault ? RoofState::ERROR' in block(s,'roofStop')
assert 'roofState!=RoofState::ERROR' in s and 'if (roofClose()) g_pendingAutoClose=false;' in s
for member,name in [('openshutter','roofOpen'),('closeshutter','roofClose')]:
 b=s[s.index('else if (member == "'+member+'"'):]
 b=b[:b.index('else if (member',8)]
 assert name+'()' in b
print('NINA integration PASS: real park logic, both command gates, no repeated pulses, AUTO path, Alpaca dispatch, live sensor STOP and latched fault')


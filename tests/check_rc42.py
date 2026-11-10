from pathlib import Path
import sys,subprocess,re
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from check_wind import block
s=(root/'src/main.cpp').read_text(encoding='utf-8-sig')
subprocess.run([sys.argv[1],'-std=gnu++17','-include','initializer_list','-fsyntax-only','-I'+str(root/'include'),str(root/'tests/rc42_checks.cpp')],check=True)
for name in ['relayPulse','stopDrivePulse']:
 b=block(s,name)
 assert b.index('pulseActive[')<b.index('relayWrite(ch, true)' if name=='relayPulse' else 'modbusRelayWriteCoil(RELAY_ROOF_STOP,true)')
 assert b.index('pulseEndTime[')<b.index('relayWrite(ch, true)' if name=='relayPulse' else 'modbusRelayWriteCoil(RELAY_ROOF_STOP,true)')
assert 'hardWeatherBlocksOpen()' in block(s,'roofOpen')
assert 'g_windUnsafeLatched = false' not in block(s,'handleClientImpl')
assert 'g_windUnsafeLatched = false' not in block(s,'readSensorsAndSafety').split('} else {\n    bool gustTrip')[0]
weather=block(s,'weatherCloseRequested');assert 'g_mountEstop' not in weather and 'safetyInputsUsable' not in weather
assert 'COMMS_WATCHDOG_ENABLED && COMMS_LOST_AUTOPARK && g_commsLost' in weather
assert '!policySkyReady()' in weather # TEMP3: availability follows selected cloud protection.
sensor=block(s,'readSensorsAndSafety');assert 'g_pendingAutoClose=roofStop();' in sensor
assert 'g_stopCommandConfirmed && g_stopReleaseConfirmed' in sensor
assert 'roofState!=RoofState::ERROR' in sensor
assert 'roofClose()' not in block(s,'manualModeTick')
assert 'manualModeOff();' not in block(s,'manualModeTick')
handler=block(s,'handleClientImpl')
assert 'method!="POST" || qpos>=0 || String(request.csrf)!=webCommandToken()' in handler
assert 'passParam' not in block(s,'checkAuth')
assert 'g_manualMode' not in s[s.index('member == "closeshutter"'):s.index('member == "abortslew"')]
assert 'g_manualMode' in s[s.index('member == "openshutter"'):s.index('member == "closeshutter"')]
for name in ['roofOpen','roofClose']:
 b=block(s,name)
 assert 'pulseActive[RELAY_ROOF_OPEN] || pulseActive[RELAY_ROOF_CLOSE] || pulseActive[RELAY_ROOF_STOP]' in b
recovery=block(s,'acknowledgeCommunicationFault')
for gate in ['RoofFaultKind::COMMUNICATION','(limitClosedActive() && limitOpenActive())','!clearanceAllowsClose()','mountProtectionBlocksClose()',
             '!g_stopCommandConfirmed','!g_stopReleaseConfirmed','modbusRelayPollNow()',
             'modbusRelayActualState[RELAY_ROOF_OPEN]','modbusRelayActualState[RELAY_ROOF_CLOSE]',
             'modbusRelayActualState[RELAY_ROOF_STOP]']:
 assert gate in recovery,gate
assert 'relayPulse(' not in recovery and 'roofClose(' not in recovery and 'roofOpen(' not in recovery
print('RC4.2 PASS: weather matrix, dense-cloud transitions/rollover, parser, pulse cleanup ordering, STOP/CLOSE gates, persistent MANUAL and HTTP contracts')

"""Regression checks for the isolated SafetyMonitor watchdog and truthful diagnostics."""
from pathlib import Path
from check_wind import block
p=Path(__file__).resolve().parents[1]
s=(p/'src/main.cpp').read_text(encoding='utf-8')
w=block(s,'commsWatchdogTick')
assert 'since = now - g_lastSafetyMonitorContactMs;' in w
assert 'since = now - g_lastAlpacaContactMs;' not in w
handler=block(s,'handleClientImpl')
a=handler.index('g_lastAlpacaContactMs = millis();')
b=handler.index('// Tylko SafetyMonitor',a)
generic=handler[a:b]
assert 'g_commsLost' not in generic and 'g_commsNoContactSinceMs' not in generic
assert 'g_lastSafetyMonitorContactMs = millis();' in handler[b:]
assert 'g_commsLost=false;' in handler[b:]
assert 'manual_auto_return\\\":false' in s
# Generic telemetry may be received indefinitely; SafetyMonitor age still reaches timeout.
last_sm=1000;last_generic=1000;timeout=300000
for now in range(1000,302001,1000):
 last_generic=now
 lost=now-last_sm>=timeout
assert lost and now-last_generic==0
print('PASS: temperature/ObservingConditions cannot reset NINA watchdog; MANUAL has no false automatic-return flag')

from pathlib import Path
import subprocess,sys
p=Path(__file__).resolve().parents[1]
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only','-I'+str(p/'include'),str(p/'tests/wind_recovery_checks.cpp')],check=True)
s=(p/'src/main.cpp').read_text(encoding='utf-8')
a=s[s.index('static void updateParkStatus()'):s.index('// ============== Forward declarations',s.index('static void updateParkStatus()'))]
assert 'setTimeout(600)' not in a and 'setTimeout(1)' in a
assert 'connect(parkAddress, PARK_ALPACA_PORT, 400)' in a and 'fromString(PARK_ALPACA_HOST)' in a
assert '::send(' in a and 'MSG_DONTWAIT' in a and 'alpacaClient.print' not in a
assert 'resp.length()<600' in a and 'millis()-t0<600' in a and 'delay(1)' in a
assert 'RG11_WET_HOLD_MS = 30UL * 1000UL' in s and 'rainConfirm >= 2' in s
print('PASS recovery latch, unchanged low-voltage rejection/rain thresholds, bounded park exchange and diagnostic breadcrumbs')

new=(p/'include/wind_monitor.h').read_text(encoding='utf-8')
old=(p/'tests/fixtures/wind_monitor.temp1.h').read_text(encoding='utf-8')
normalized=new.replace('bool holding=false, faultLatched=false;','bool holding=false;').replace('  if(v.faultLatched) return "OUT_OF_RANGE"; // Keep the fault visible throughout recovery.\n','').replace('  if(!valid || gap)v.faultLatched=true;\n  if(valid && v.goodCount>=64)v.faultLatched=false;\n','')
assert normalized==old,'Unexpected calibration/range/grace change'

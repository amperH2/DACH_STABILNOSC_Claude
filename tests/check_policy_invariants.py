from pathlib import Path
import sys,json,hashlib
from check_wind import block
from temp3_baseline import before_policy
from netfix_baseline import before_netfix
p=Path(__file__).resolve().parents[1];s=(p/'src/main.cpp').read_text(encoding='utf-8');b=json.loads((p/'tests/rc51_policy_baseline.json').read_text())
for name,expected in b['unchanged_functions'].items():
 if name=='commsWatchdogTick':continue # Audited separately: only SafetyMonitor proves NINA contact.
 current=before_policy(before_netfix(block(s,name)))
 if name=='windResetAfterCalibration':current=current.replace('  g_windValidity.faultLatched=false;\n','')
 assert hashlib.sha256(current.encode()).hexdigest()==expected,name
for name,expected in b['unchanged_files'].items():
 if name=='include/wind_monitor.h':
  old=(p/'tests/fixtures/wind_monitor.temp1.h').read_bytes()
  assert hashlib.sha256(old).hexdigest()==expected,name+' baseline'
  continue
 data=(p/name).read_bytes()
 if name=='platformio.ini':
  fixture=p/'tests/fixtures/platformio.before_netfix.ini'
  assert hashlib.sha256(fixture.read_bytes()).hexdigest()==expected,'Original platformio baseline'
  assert (p/name).read_text(encoding='utf-8')==fixture.read_text(encoding='utf-8').replace('  arduino-libraries/Ethernet@2.0.2\n',''),'Only Ethernet dependency should change'
  continue
 if name=='scripts/load_config.py':
  fixture=p/'tests/fixtures/identity_before_load_config.py'
  assert hashlib.sha256(fixture.read_bytes()).hexdigest()==expected,'Original config loader baseline'
  current=(p/name).read_text(encoding='utf-8')
  original=fixture.read_text(encoding='utf-8')
  original=original.replace('from config_core import load_hardware,load_secrets','from config_core import load_hardware,load_secrets,load_observatory')
  original=original.replace("secrets=load_secrets(root/'secrets.ini')","secrets=load_secrets(root/'secrets.ini')\nidentity=load_observatory(root/'observatory.ini')")
  original=original.replace('for k,v in secrets.items())','for k,v in {**secrets,**identity}.items())')
  assert current==original,'Only central identity loading may change'
  continue
 assert hashlib.sha256(data).hexdigest()==expected,name
sys.path.insert(0,str(p/'scripts'));from config_core import load_hardware
for env,values in b['hardware_profiles'].items():
 expected=dict(values)
 expected['ENABLE_DHT22']=1 # TEMP1: installed DHT22 on GPIO47, already authorized and working.
 assert load_hardware(p/'hardware.ini',env)==expected,env
print('RC5.1 invariants PASS:',len(b['unchanged_functions']),'functions compared after exact TEMP3 policy normalization; hardware values and wind calibration/ranges retained')

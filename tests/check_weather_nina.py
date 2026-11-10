from pathlib import Path
import sys,importlib.util,copy
p=Path(__file__).resolve().parents[1]
engine=p/'tests/fixtures/nina';sys.path.insert(0,str(engine))
def module(name):
 spec=importlib.util.spec_from_file_location('temp3_'+name,engine/(name+'.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
gate=module('capture_ready');clear=module('wait_clear')
policy=dict(version=1,rain=True,wind=True,clouds=True,humidity=False,dew=False,config_fault=False,thermo_valid=True,open_ready=True,reopen_sec=300,cloud_hold_sec=180,moisture_hold_sec=60,humidity_limit=90,dew_tenths=30)
st=dict(fw='ASTRO-ROOF SAFE RC5.2 TEMP3',safe=True,manual_mode=False,roof=dict(state='OPEN',limit_open=True,limit_closed=False,motion_fault=''),safety_ext=dict(mount_estop=False,comms_lost=False),sensors=dict(rain=False,wind_valid=True,mlx_active=1,mlx1_valid=True,mlx1_age_ms=1,cloud_delta=20),limits=dict(cloud_clear=16),diag=dict(uptime_s=1),weather_policy=policy)
assert gate.require_capture_ready(st)
for selected in (False,True):
 for key,sensor in [('wind','wind_valid'),('rain','rain')]:
  x=copy.deepcopy(st);x['weather_policy'][key]=selected;x['sensors'][sensor]=False if key=='wind' else True
  try:gate.require_capture_ready(x);ok=True
  except gate.Blocked:ok=False
  assert ok is (not selected),(selected,key)
for modify in ('closed','policy_fault','missing_flag','unknown_version','unsafe'):
 x=copy.deepcopy(st)
 if modify=='closed':x['roof'].update(state='CLOSED',limit_open=False,limit_closed=True)
 elif modify=='policy_fault':x['weather_policy']['config_fault']=True
 elif modify=='missing_flag':del x['weather_policy']['wind']
 elif modify=='unknown_version':x['weather_policy']['version']=2
 else:x['safe']=False
 try:gate.require_capture_ready(x)
 except gate.Blocked:pass
 else:raise AssertionError(modify)
# Startup must respect a deliberately omitted MLX but still wait for firmware's recovery.
x=copy.deepcopy(st);x['weather_policy']['clouds']=False;x['sensors']['mlx1_valid']=False
cg=clear.ClearGate()
for t in range(0,185,5):
 x['diag']['uptime_s']=t+1;ready,_,_=cg.update(x,t)
assert ready
x['weather_policy']['open_ready']=False;x['diag']['uptime_s']=191;assert not cg.update(x,190)[0]
x['weather_policy']['open_ready']=True;x['diag']['uptime_s']=196;assert cg.update(x,195)[0]
x['weather_policy']['clouds']=True;assert clear.observation(x)[0] is None
print('PASS NINA selected-input policy, strict old firmware fallback, closed-roof protection, invalid-policy veto, startup ignores omitted clouds and waits for recovery')

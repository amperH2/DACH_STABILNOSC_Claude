"""Offline only: python tests/run_all.py --compiler <xtensa-esp32s3-elf-g++> [--node node]."""
from pathlib import Path
import argparse,subprocess,sys,os,json,shutil
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',required=True);parser.add_argument('--node',default=shutil.which('node'));args=parser.parse_args()
if not args.node:parser.error('Node.js required; supply --node')
p=Path(__file__).resolve().parents[1];out=p/'.pio/offline';out.mkdir(parents=True,exist_ok=True);tmp=out/'tmp';tmp.mkdir(exist_ok=True)
os.environ['TEMP']=os.environ['TMP']=str(tmp);os.environ['PYTHONUTF8']='1';results=[];logs=[]
for f in sorted((p/'tests').glob('*')):
 if f.name=='check_nina_integration.py':command=[sys.executable,'-B',str(f),str(p),args.compiler]
 elif f.name=='check_pc_park.py':command=[sys.executable,'-B',str(f)]
 elif f.name.startswith('check_') and f.suffix=='.py':command=[sys.executable,'-B',str(f),args.compiler]
 elif f.suffix=='.cjs':command=[args.node,str(f)]
 elif f.name in ['wind_checks.cpp','wind_glitch_checks.cpp','do_policy_checks.cpp','operator_policy_checks.cpp']:command=[args.compiler,'-std=gnu++17','-fsyntax-only','-I'+str(p/'include'),str(f)]
 else:continue
 try:
  r=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=180);passed=r.returncode==0;log=r.stdout+r.stderr
 except subprocess.TimeoutExpired as exc:passed=False;log='TIMEOUT: '+str(exc)
 results.append({'test':f.name,'passed':passed});logs.append(f.name+'\n'+log);print(('PASS ' if passed else 'FAIL ')+f.name,flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8');(out/'results.log').write_text('\n'.join(logs),encoding='utf-8')
print(str(sum(x['passed'] for x in results))+'/'+str(len(results))+' offline suites passed. No device communication performed.')
sys.exit(any(not x['passed'] for x in results))

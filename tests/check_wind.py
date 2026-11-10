from pathlib import Path
import re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def block(source,name):
 m=re.search(r'(?:static\s+)?(?:bool|void)\s+'+name+r'\([^;\n]*\)\s*\{',source)
 if not m:raise AssertionError('Missing function '+name)
 depth=0
 for i in range(source.index('{',m.start()),len(source)):
  if source[i]=='{':depth+=1
  elif source[i]=='}':
   depth-=1
   if depth==0:return source[m.start():i+1]
 raise AssertionError('Unclosed '+name)
def run(compiler,backup=None):
 subprocess.run([compiler,'-std=gnu++17','-fsyntax-only','-I'+str(ROOT/'include'),str(ROOT/'tests/wind_checks.cpp')],check=True)
 if backup:
  original=Path(backup).read_text(encoding='utf-8');new=(ROOT/'src/main.cpp').read_text(encoding='utf-8')
  for name in ['roofOpen','roofClose','roofStop','roofTick','updatePulseOutputs','parkGuardAllowsClose']:
   assert block(original,name)==block(new,name),name+' changed'
  for key in ['CFG_WIND_WARN','CFG_WIND_SUSTAINED','CFG_WIND_GUST','CFG_WIND_RESET','WIND_FAULT_V_LOW','WIND_FAULT_V_HIGH','WIND_FAULT_MS','WIND_FAULT_UNSAFE']:
   pattern=r'#define\s+'+key+r'\s+(\S+)'
   assert re.search(pattern,original).group(1)==re.search(pattern,new).group(1),key+' changed'
  print('Roof functions and existing wind thresholds unchanged: PASS')
 print('Production C++ wind calibration/validity checks: PASS')
if __name__=='__main__':run(sys.argv[1],sys.argv[2] if len(sys.argv)>2 else None)

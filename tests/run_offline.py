from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
compiler=sys.argv[1];node=sys.argv[2];rc3=sys.argv[3]
python=sys.executable
steps=[
    ('RC4 production logic and integration',[python,'-B',str(root/'tests/check_rc4.py'),compiler,rc3]),
    ('Park and NINA contract',[python,'-B',str(root/'tests/check_nina_integration.py'),str(root),compiler]),
    ('Hardware schema, Modbus vectors, AUX ownership',[python,'-B',str(root/'tests/check_project.py'),compiler]),
    ('RC4 safety invariants',[python,'-B',str(root/'tests/check_rc41_invariants.py')]),
    ('Wind production checks',[python,'-B',str(root/'tests/check_wind.py'),compiler]),
    ('DO production policy',[compiler,'-std=gnu++17','-fsyntax-only','-I'+str(root/'include'),str(root/'tests/do_policy_checks.cpp')]),
]+[(name,[node,str(root/'tests'/file)]) for name,file in [
    ('WWW AUX','check_web.cjs'),('WWW wind','check_wind_web.cjs'),
    ('WWW local DO','check_local_do.cjs'),('WWW manual','check_manual_web.cjs'),('WWW status and layout','check_status_web.cjs')]]
log=[]
for name,args in steps:
    result=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace')
    entry=name+': '+('PASS' if result.returncode==0 else 'FAIL')+'\n'+result.stdout+result.stderr
    log.append(entry);print(entry,flush=True)
    if result.returncode:
        (root/'docs/RC4_TESTS.log').write_text('\n'.join(log),encoding='utf-8')
        raise SystemExit(result.returncode)
(root/'docs/RC4_TESTS.log').write_text('\n'.join(log),encoding='utf-8')
print('ALL 11 OFFLINE SUITES PASS; no hardware connections or upload',flush=True)

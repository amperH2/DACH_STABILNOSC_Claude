Import("env")
from pathlib import Path
import sys,json,configparser
sys.path.insert(0,str(Path(env.subst('$PROJECT_DIR'))/'scripts'))
from config_core import load_hardware,load_secrets,load_observatory
root=Path(env.subst('$PROJECT_DIR'))
profile=env.subst('$PIOENV')
hardware=load_hardware(root/'hardware.ini',profile)
secrets=load_secrets(root/'secrets.ini')
identity=load_observatory(root/'observatory.ini')
# Private per-profile header: credentials never enter compiler command arguments.
# Application options do not force an unnecessary rebuild of every external library.
directory=Path(env.subst('$BUILD_DIR'))/'generated'
directory.mkdir(parents=True,exist_ok=True)
header=directory/'astro_generated_config.h'
content='#pragma once\n'+''.join('#define '+k+' '+str(v)+'\n' for k,v in hardware.items())
content+=''.join('#define '+k+' '+json.dumps(v,ensure_ascii=True)+'\n' for k,v in {**secrets,**identity}.items())
labels=configparser.ConfigParser(interpolation=None,strict=True);labels.optionxform=str
labels.read(root/'do_names.ini',encoding='utf-8-sig')
if labels.sections()!=['outputs'] or set(labels['outputs'])!={f'DO{n}' for n in range(1,8)}:
    raise ValueError('do_names.ini requires [outputs] and DO1..DO7')
for n in range(1,8):
    label=labels['outputs'][f'DO{n}'].strip()
    if not label or len(label)>48 or any(ord(c)<32 for c in label):
        raise ValueError('Invalid DO label: DO'+str(n))
    content+='#define DO'+str(n)+'_NAME '+json.dumps(label,ensure_ascii=False)+'\n'
if not header.exists() or header.read_text(encoding='utf-8')!=content:
    header.write_text(content,encoding='utf-8')
env.Append(CPPPATH=[str(directory)])
print('ASTRO configuration: '+profile+'; '+str(len(hardware))+' validated hardware options')
if not secrets.get('UI_PASS'):print('UI_PASS missing: web control remains locked')

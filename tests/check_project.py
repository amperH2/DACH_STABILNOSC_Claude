from pathlib import Path
import sys,subprocess,tempfile,configparser,itertools,json
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from config_core import load_hardware,load_secrets

def run(compiler):
    checks=0
    data=(ROOT/'hardware.ini').read_text(encoding='utf-8')
    with tempfile.TemporaryDirectory() as directory:
        temporary=Path(directory);bad=temporary/'hardware.ini'
        for profile in ('poe_main','eth_8ro_spare','rs485_service'):
            load_hardware(ROOT/'hardware.ini',profile);checks+=1
        for old,new in [('LED_PWM_ENABLED = 1','LED_PWM_ENABLED ='),('LED_PWM_ENABLED = 1','LED_PWM_ENABLED = 2'),
                        ('LED_PWM_ENABLED = 1','LED_PWM_ENABELD = 1'),('PWM_FREQ_HZ = 1000','PWM_FREQ_HZ = 0'),
                        ('MODBUS_RELAY_ADDR = 1','MODBUS_RELAY_ADDR = 0'),('RELAY_ACTIVE_LOW_CFG = 0','RELAY_ACTIVE_LOW_CFG = 1')]:
            bad.write_text(data.replace(old,new),encoding='utf-8')
            try:load_hardware(bad,'poe_main')
            except (ValueError,configparser.Error):checks+=1
            else:raise AssertionError('Invalid configuration accepted: '+new)
        secret=temporary/'secrets.ini';secret.write_text('[secrets]\nLED_PWM_ENABLED=1\n')
        try:load_secrets(secret)
        except ValueError:checks+=1
        else:raise AssertionError('Hardware flag accepted in secrets')
        hardware=load_hardware(ROOT/'hardware.ini','poe_main')
        fixture=temporary/'fixture.cpp'
        fixture.write_text(r"""
#include "modbus_protocol.h"
constexpr uint8_t on[]={1,5,0,0,255,0,0x8C,0x3A};
static_assert(astro_modbus::crc16(on,6)==0x3A8C,"Vendor relay CH1 ON vector");
constexpr uint8_t addr[]={0,6,0x40,0,0,2,0x1C,0x1A};
static_assert(astro_modbus::crc16(addr,6)==0x1A1C,"Vendor PWM address vector");
constexpr auto f=astro_modbus::pwmFrame(1,1,20000,1000);
constexpr uint8_t expected[]={1,16,0,0,0,3,6,0,0,0x4E,0x20,3,0xE8,0xF0,0xDC};
constexpr bool matches(){for(unsigned i=0;i<15;++i)if(f[i]!=expected[i])return false;return true;}
static_assert(matches(),"Vendor PWM 200Hz 10% vector, including CRC");
constexpr auto ch4=astro_modbus::pwmFrame(2,4,100000,10000);
static_assert(ch4[3]==9&&ch4[11]==0x27&&ch4[12]==0x10,"PWM CH4 register/duty layout");
constexpr auto invalid=astro_modbus::pwmFrame(2,5,100000,0);
static_assert(invalid[0]==0,"Invalid PWM channel rejected");
constexpr uint8_t ack[]={1,16,0,0,0,3,0x80,8};
static_assert(astro_modbus::writeResponseValid(f.data(),15,ack,8),"Valid FC10 response");
constexpr uint8_t bad[]={1,16,0,0,0,3,0,0};
static_assert(!astro_modbus::writeResponseValid(f.data(),15,bad,8),"Corrupt CRC rejected");
constexpr uint8_t exc[]={1,0x90,2,0xCD,0xC1};
static_assert(!astro_modbus::writeResponseValid(f.data(),15,exc,5),"Exception rejected");
constexpr uint8_t other[]={2,16,0,0,0,3,0x80,8};
static_assert(!astro_modbus::writeResponseValid(f.data(),15,other,8),"Other slave rejected");
""",encoding='utf-8')
        base=[compiler,'-DASTRO_TEST_CONFIG=1','-std=c++17','-fsyntax-only','-I'+str(ROOT/'include')]
        def compile_file(defines,expect=True):
            args=base+['-D'+k+'='+str(v) for k,v in defines.items()]+[str(fixture)]
            result=subprocess.run(args,capture_output=True,text=True)
            if (result.returncode==0)!=expect:raise AssertionError(result.stderr[:1500])
        compile_file({});checks+=9
        for enabled in itertools.product((0,1),repeat=5):
            for collision,mount in itertools.product((0,1),repeat=2):
                definitions=hardware.copy();definitions.update({f'AUX{i+1}_ENABLED':v for i,v in enumerate(enabled)})
                definitions.update(COLLISION_SENSOR_ENABLED=collision,COLLISION_POWER_RELAY_ENABLED=collision,
                                   MOUNT_POWER_RELAY_ENABLED=mount)
                code='#include "astro_config.h"\n#include "relay_map.h"\n'
                code+='static_assert(astroAuxChannel(0)==255&&astroAuxChannel(6)==255,"Invalid AUX IDs");\n'
                for i,v in enumerate(enabled):
                    expected=bool(v and not ((i==3 and collision) or (i==4 and mount)))
                    code+=f'static_assert(astroAuxChannel({i+1})=={[6,5,7,3,4][i]},"Stable AUX identity");\n'
                    code+=f'static_assert(astroAuxAvailable({i+1})=={str(expected).lower()},"Exclusive channel owner");\n'
                fixture.write_text(code);compile_file(definitions);checks+=1
        fixture.write_text('#include "astro_config.h"\n')
        for changes in [dict(MODBUS_RELAY_ADDR=2),dict(PWM_MODULE_ENABLED=0,LED_PWM_ENABLED=1),
                        dict(COLLISION_SENSOR_ENABLED=0,COLLISION_POWER_RELAY_ENABLED=1),
                        dict(MOUNT_LIMIT_ENABLED=0,MOUNT_POWER_RELAY_ENABLED=1),
                        dict(OBS_BOARD_8RO=1,ILLUM_DO_ENABLED=1),dict(ROOF_MOTION_ENABLED=1,PARK_GUARD_ENABLED=0)]:
            values=hardware.copy();values.update(changes);compile_file(values,False);checks+=1
    print(str(checks)+' checks passed: config, protocol vectors, 128 AUX ownership combinations, invalid-build guards')
if __name__=='__main__':run(sys.argv[1])

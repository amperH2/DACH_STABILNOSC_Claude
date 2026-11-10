"""Extract real operator NVS/mode functions; replace only storage dependency with a deterministic fake."""
from pathlib import Path
import sys,subprocess,tempfile
from check_wind import block
p=Path(__file__).resolve().parents[1];s=(p/'src/main.cpp').read_text(encoding='utf-8')
prefix=r'''
struct Store { bool beginOK=true,writeOK=true,manual=false,trip=false,hasManual=false,hasTrip=false; };
struct Preferences {
 Store* p;
 constexpr bool begin(const char*,bool){return p->beginOK;}
 constexpr unsigned putBool(const char* k,bool v){if(!p->writeOK)return 0;if(k[1]=='a'){p->manual=v;p->hasManual=true;}else{p->trip=v;p->hasTrip=true;}return 1;}
 constexpr bool getBool(const char* k,bool fallback){return k[1]=='a'?(p->hasManual?p->manual:fallback):(p->hasTrip?p->trip:fallback);}
 constexpr void end(){}
};
struct Text { const char* p="";constexpr Text& operator=(const char* v){p=v;return *this;} };
struct Harness {
 Store store{};
 bool g_manualMode=false,g_manualAuthorized=false,g_mountPowerRestoreBlocked=true,g_mountEstop=false;
 Text g_mountEstopReason;
 template<class... T>constexpr void eventLogEx(T...){}
'''
code=prefix
for name in ['operatorStore','operatorLoad','manualModeOn','manualModeOff','manualModeTick']:
 b=block(s,name).replace('static bool','constexpr bool',1).replace('static void','constexpr void',1)
 b=b.replace('Preferences prefs;','Preferences prefs{&store};')
 code+=b+'\n'
code+=r'''
};
constexpr bool persistentManual(){Harness h;if(!h.manualModeOn()||!h.g_manualAuthorized)return false;h.g_manualMode=false;h.operatorLoad();if(!h.g_manualMode||h.g_manualAuthorized)return false;h.manualModeTick();if(!h.g_manualMode)return false;if(!h.manualModeOff())return false;h.operatorLoad();return !h.g_manualMode&&!h.g_manualAuthorized;}
constexpr bool storageFailure(){Harness h;h.store.beginOK=false;h.operatorLoad();if(!h.g_manualMode||h.g_manualAuthorized||!h.g_mountEstop||!h.g_mountPowerRestoreBlocked)return false;Harness w;w.store.writeOK=false;if(w.manualModeOn()||w.g_manualMode||w.g_manualAuthorized)return false;Harness z;z.manualModeOn();z.store.writeOK=false;return !z.manualModeOff()&&z.g_manualMode;}
constexpr bool mountRetained(){Harness h;if(!h.operatorStore("mountTrip",true))return false;h.operatorLoad();return h.g_mountEstop&&h.g_mountPowerRestoreBlocked&&!h.g_manualAuthorized;}
constexpr bool firstBoot(){Harness h;h.operatorLoad();return h.g_manualMode&&!h.g_manualAuthorized;}
static_assert(firstBoot(),"First install cannot silently enter AUTO from unknown prior service state");
static_assert(persistentManual(),"MANUAL persists restart; authorization never does; no timed AUTO");
static_assert(storageFailure(),"NVS failures cannot silently authorize or surrender MANUAL");
static_assert(mountRetained(),"Mount trip survives reboot");
'''
with tempfile.TemporaryDirectory() as d:
 f=Path(d)/'operator.cpp';f.write_text(code,encoding='utf-8');subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only',str(f)],check=True)
handler=block(s,'handleClientImpl')
assert handler.index('astro_policy::trustedIPv4')<handler.index('g_lastAlpacaContactMs = millis()')
assert handler.index('if(!pl.startsWith("/api/v1/"))')<handler.index('String base = pl;')
assert 'Access-Control-Allow-Origin' not in s and 'if(request.origin[0] ||' in handler
assert 'client.remoteIP()' in block(s,'dispatch')
for name in ['roofOpen']:
 b=block(s,name);assert b.count('hardWeatherBlocksOpen()')>=2 and b.count('astro_policy::weatherMeasurementsReady(')==2
assert 'digitalRead(RG11_PIN)' in block(s,'hardWeatherBlocksOpen')
assert 'g_manualAuthorized' in handler and 'INSPECTED_MOUNT' in handler and 'INSPECTED_STATIONARY' in handler
reset=block(s,'mountEstopReset')
assert reset.index('mountLimitRawTripped()')<reset.index('mountPowerCut(false)')
assert reset.index('modbusRelayActualState[RELAY_ROOF_STOP]')<reset.index('mountPowerCut(false)')
assert 'mountPowerCut(true);return blockRoofCommand' in reset
setup=block(s,'setup');assert setup.index('operatorLoad()')<setup.index('modbusRelayBegin()')
io=(p/'src/hardware_io.h').read_text(encoding='utf-8')
assert 'ch==MOUNT_POWER_RELAY_CH && g_mountPowerRestoreBlocked' in io
assert 'if(MOUNT_POWER_RELAY_ENABLED && g_mountPowerRestoreBlocked)initial^=' in io
assert 'RTC_NOINIT_ATTR static volatile ResetBreadcrumb' in s and 'g_resetBreadcrumb.check=0' in s
assert 'Task watchdog: zadanie lub przyczyna wymagaja diagnozy' in s
sys.path.insert(0,str(p/'scripts'));from config_core import load_secrets
with tempfile.TemporaryDirectory() as d:
 f=Path(d)/'secrets.ini'
 for hosts,valid in [('192.168.1.119',True),('10.0.0.1,192.168.1.119',True),('',True),('192.168.1.0/24',False),('*',False),('192.168.1.119,',False),('256.1.1.1',False),('0.0.0.0',False)]:
  f.write_text('[secrets]\nALPACA_TRUSTED_IPS='+hosts+'\n')
  try:load_secrets(f);accepted=True
  except ValueError:accepted=False
  assert accepted==valid,hosts
print('Operator policy PASS: extracted NVS reboot/failure scenarios; mount latch; source gates, relay initialization, browser/Alpaca access and exact-host config validation')

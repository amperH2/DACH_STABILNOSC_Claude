from pathlib import Path
import json,re,sys,tempfile,subprocess
p=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(p/'scripts'))
from config_core import load_observatory
identity=load_observatory(p/'observatory.ini')
assert identity=={'OBSERVATORY_NAME':'Specula Carpathia','OBSERVATORY_SHORT_NAME':'SPECULA','OBSERVATORY_HOSTNAME':'specula-carpathia'}
with tempfile.TemporaryDirectory() as temp:
 f=Path(temp)/'observatory.ini'
 original=(p/'observatory.ini').read_text(encoding='utf-8')
 for a,b in [('specula-carpathia','bad.local'),('specula-carpathia','-bad'),('SPECULA','bad space'),('Specula Carpathia','bad<script>'),('OBSERVATORY_NAME','UNKNOWN')]:
  f.write_text(original.replace(a,b))
  try:load_observatory(f)
  except ValueError:pass
  else:raise AssertionError('Invalid identity accepted: '+b)
 f.write_text(original.replace('= Specula Carpathia','= "Specula Carpathia"'))
 assert load_observatory(f)==identity
main=(p/'src/main.cpp').read_text(encoding='utf-8')
part=main[main.index('if (method == "GET" && pl == "/management/v1/configureddevices")'):]
part=part[part.index('alpacaSendValue(c, cid,')+len('alpacaSendValue(c, cid,'):part.index(');')]
part=part.replace('OBSERVATORY_NAME',json.dumps(identity['OBSERVATORY_NAME']))
devices=json.loads(''.join(json.loads(x) for x in re.findall(r'"(?:\\.|[^"\\])*"',part)))
assert [(d['DeviceType'],d['DeviceNumber'],d['UniqueID']) for d in devices]==[('SafetyMonitor',0,'ESP32-SM-0'),('ObservingConditions',0,'ESP32-OC-0'),('Dome',0,'ESP32-DOME-0')]
assert all(identity['OBSERVATORY_NAME'] in d['DeviceName'] and identity['OBSERVATORY_NAME'] in d['Description'] for d in devices)
assert devices[2]['SupportedActions']==['RoofPosition','RoofState']
assert 'WiFi.setHostname(OBSERVATORY_HOSTNAME);' in main
assert 'ArduinoOTA.setHostname(OBSERVATORY_HOSTNAME);' in main
assert 'char text[160 + sizeof(OBSERVATORY_PREFIX)]' in main
assert 'const String branded = observatoryMessage(msg);' in main
assert 'String full = observatoryMessage(' in (p/'src/obs_notify.h').read_text(encoding='utf-8')
assert 'char text[300 + sizeof(OBSERVATORY_PREFIX)]' in (p/'src/online_notifications.h').read_text(encoding='utf-8')
assert 'if(channel==ONLINE_PUSHOVER) observatoryCopyMessage' in (p/'src/online_notifications.h').read_text(encoding='utf-8')
header=(p/'include/observatory_identity.h').read_text(encoding='utf-8')
body=header[header.index('#define OBSERVATORY_PREFIX'):]
body=body.replace('inline String','constexpr String').replace('inline void','constexpr void')
cpp=r'''
#include <stddef.h>
constexpr size_t length(const char* s){size_t n=0;while(s[n])++n;return n;}
constexpr int compare(const char* a,const char* b,size_t n){for(size_t i=0;i<n;++i){if(a[i]!=b[i])return a[i]-b[i];if(!a[i])return 0;}return 0;}
constexpr void format(char* dest,size_t cap,const char* fmt,const char* a,const char* b){size_t n=0;for(size_t i=0;a[i]&&n+1<cap;++i)dest[n++]=a[i];if(fmt[2]==' '){if(n+1<cap)dest[n++]=' ';for(size_t i=0;b[i]&&n+1<cap;++i)dest[n++]=b[i];}if(cap)dest[n]=0;}
struct String {
 char text[512]={};
 constexpr String(const char* s=""){for(size_t i=0;s[i]&&i<511;++i)text[i]=s[i];}
 constexpr bool operator==(const char* s)const{return length(text)==length(s)&&compare(text,s,length(s))==0;}
 constexpr bool startsWith(const char* s)const{return compare(text,s,length(s))==0;}
 constexpr String operator+(const String& rhs)const{String out=*this;size_t n=length(out.text);for(size_t i=0;rhs.text[i]&&n<511;++i)out.text[n++]=rhs.text[i];return out;}
};
#define strlen length
#define strncmp compare
#define snprintf format
'''+''.join('#define '+k+' '+json.dumps(v)+'\n' for k,v in identity.items())+body+r'''
constexpr bool test(){
 if(!(observatoryMessage("DACH: Zadanie CLOSE") == "[SPECULA] DACH: Zadanie CLOSE"))return false;
 if(!(observatoryMessage("[SPECULA] DACH: CLOSED potwierdzone krancowka") == "[SPECULA] DACH: CLOSED potwierdzone krancowka"))return false;
 char raw[160]={};for(int i=0;i<159;++i)raw[i]='x';char full[160+sizeof(OBSERVATORY_PREFIX)]={};observatoryCopyMessage(full,sizeof(full),raw);
 if(length(full)!=169 || !String(full).startsWith("[SPECULA] "))return false;
 char pushRaw[300]={};for(int i=0;i<299;++i)pushRaw[i]='y';char pushFull[300+sizeof(OBSERVATORY_PREFIX)]={};observatoryCopyMessage(pushFull,sizeof(pushFull),pushRaw);if(length(pushFull)!=309)return false;
 char twice[256]={};observatoryCopyMessage(twice,sizeof(twice),full);if(length(twice)!=169)return false;
 char empty[64]={};observatoryCopyMessage(empty,sizeof(empty),nullptr);if(compare(empty,"[SPECULA] ",10))return false;
 return true;
}
static_assert(test(),"Production prefix helpers: once, source/cause retained, long log and null-safe");
'''
# Generated macro lines use real newlines, not shell interpolation.
cpp=cpp.replace('\\n','\n') if '\\ninline' in cpp else cpp
out=p/'.pio/offline';out.mkdir(parents=True,exist_ok=True);f=out/'identity.cpp';f.write_text(cpp)
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only',str(f)],check=True)
print('PASS identity validation; compatible Alpaca descriptors; production prefix helpers preserve payload, avoid duplicates and retain long log text')



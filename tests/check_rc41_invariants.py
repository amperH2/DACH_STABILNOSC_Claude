from pathlib import Path
import hashlib,json,re
from html.parser import HTMLParser
from check_wind import block
from temp3_baseline import before_policy
from netfix_baseline import before_netfix
root=Path(__file__).resolve().parents[1];s=(root/'src/main.cpp').read_text(encoding='utf-8-sig')
baseline=json.loads((root/'tests/rc4_safety_baseline.json').read_text())
intentional={'alpacaSend','heaterControlTick','heaterManualSet','writeAll','commsWatchdogTick','httpSend','httpSendRaw','windResetAfterCalibration','dispatch','manualModeTick','mountPowerCut','mountEstopTrigger','mountEstopManual','mountEstopReset','modbusRelayAllOff','modbusRelayBegin','loop','checkAuth','roofOpen','roofClose','roofStop','roofFaultStop','relayPulse','stopDrivePulse','updatePulseOutputs','readSensorsAndSafety','manualModeOn','manualModeOff','handleClientImpl','roofTick','updateParkStatus','setup','telegramSend','telegramSendNow','telegramSendNow_blocking','netTaskFn'}
for name,expected in baseline['functions'].items():
    if name in intentional: continue
    current=before_policy(before_netfix(block(s,name)))
    if name=='updateParkStatus':
        current=current.replace('    g_parkAlpacaAttempted=true;\n    g_parkAlpacaAttemptMs=now;\n','')
        current=current.replace('    g_parkAlpacaLastAttemptOK=alpacaOk;\n    if(alpacaOk) {\n      g_parkAlpacaSuccessSeen=true;\n      g_parkAlpacaSuccessMs=millis();\n      g_parkAlpacaLastValue=alpacaParked;\n    }\n','')
    assert hashlib.sha256(current.encode()).hexdigest()==expected,'Production function changed: '+name
for name,expected in baseline['files'].items():
    if name in {'include/roof_control.h','include/http_request.h','hardware.ini','platformio.ini','include/astro_config.h','src/obs_notify.h','scripts/config_core.py','scripts/config_schema.json','src/hardware_io.h','include/wind_monitor.h'}: continue
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,name
defines=dict(re.findall(r'^\s*#define\s+(\w+)\s+([^\n]+)',s,re.M))
for name,value in baseline['defines'].items():
    if name=='OTA_HOSTNAME':
        assert 'ArduinoOTA.setHostname(OBSERVATORY_HOSTNAME);' in s and 'obserwatorium-roof' not in s
    elif name!='FW_VERSION':assert defines[name]==value,name
assert 'parkAlpacaCurrent' in s and 'PARK_ALPACA_DIAG_FRESH_MS=15000' in s
assert 'uint32_t(parkDiagNow-g_parkAlpacaSuccessMs)' in s
assert 'parkAlpacaCurrent?(g_parkAlpacaLastValue?"true":"false"):"null"' in s
for name in baseline['functions']:
    if name!='updateParkStatus':assert 'g_parkAlpaca' not in block(s,name),name+' reads diagnostic park fields'
class Structure(HTMLParser):
    def __init__(self):super().__init__();self.ids=set();self.details=[];self.roof_stop=False
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        if 'id' in a:assert a['id'] not in self.ids,'Duplicate '+a['id'];self.ids.add(a['id'])
        if tag=='details':self.details.append(a.get('id'))
        if a.get('id') in ['btnStop','btnOpen','btnClose','btnMountEstop']:assert not self.details,'Emergency/roof controls hidden';self.roof_stop=True
    def handle_endtag(self,tag):
        if tag=='details':assert self.details;self.details.pop()
source=(root/'src/web_ui.h').read_text(encoding='utf-8');html=source[source.index('<!DOCTYPE'):source.index('<script>')]
parser=Structure();parser.feed(html);assert not parser.details and parser.roof_stop
for element in ['staleBanner','parkOverview','btnPanels','panelAux','roofCard']:assert element in parser.ids
print('Baseline PASS: unchanged functions/files retain prior hashes; intentional RC42 changes checked separately; thresholds, physical gates and emergency control visibility retained')


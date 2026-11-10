"""Offline integration checks; never connects to hardware or calls upload."""
from pathlib import Path
import sys,subprocess,re,hashlib
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from check_wind import block
s=(root/'src/main.cpp').read_text(encoding='utf-8-sig')
assert 'acceptedStopEpoch=g_stopEpoch;' in s
assert '++g_stopEpoch;' in s
assert 'acceptedStopEpoch!=g_stopEpoch || request.expired(millis())' in s
assert '  heaterControlTick();' in s
compiler=sys.argv[1]
subprocess.run([compiler,'-std=gnu++17','-fsyntax-only','-I'+str(root/'include'),
               str(root/'tests/rc4_checks.cpp')],check=True)
for name,opposite in [('roofOpen','CLOSING'),('roofClose','OPENING')]:
    b=block(s,name)
    assert b.index('roofState==RoofState::'+opposite)<b.index('Idempotent')
    assert ('g_mountEstop || mountLimitRawTripped()' if name=='roofOpen' else 'mountProtectionBlocksClose()') in b
    assert b.count('!parkGuardAllowsClose()' if name=='roofOpen' else '!clearanceAllowsClose()')>=2
    assert 'collisionClearForRoofMove()' in b
    assert 'pulseActive[RELAY_ROOF_STOP]' in b
    assert 'forceIgnorePark' not in b
    assert 'roofFaultStop(' in b
assert 'forceCloseMayIgnorePark(' not in s
assert not re.search(r'roof(?:Open|Close)\((?:true|false)\)',s)
assert 'manualModeTick(' in s and 'MANUAL TIMEOUT' not in s
assert 'MANUAL_MOVE' in s
assert 'g_stopCommandConfirmed' in s and 'g_stopReleaseConfirmed' in s
stop=block(s,'stopDrivePulse')
assert stop.index('modbusRelayWriteCoil(RELAY_ROOF_STOP,true)')<stop.index('relayWrite(ch,false)')
assert 'pulseActive[ch]=!off' in stop and 'pulseActive[ch]=!off' in block(s,'roofRelaysOff')
assert '!modbusRelayOK' not in stop # best effort even when offline
assert 'g_stopCommandConfirmed=ok' in stop
pulse=block(s,'updatePulseOutputs')
assert 'astro_roof::due(' in pulse and 'pulseActive[i]' in pulse
assert pulse.index('if (relayWrite(i, false))')<pulse.index('pulseActive[i]=false')
assert 'pulseEndTime[i]=millis()+250UL' in pulse
tick=block(s,'roofTick')
for text in ['STUCK:','TIMEOUT','Utrata parku','Czujnik kolizji']:
    assert text in tick
assert tick.count('roofFaultStop(')==4
assert 'roofFaultStop(' in block(s,'updateRoofStateFromLimits')
assert 'astro_roof::fromLimits(' in block(s,'updateRoofStateFromLimits')
assert 'roofState == RoofState::OPEN || roofState == RoofState::OPENING' not in block(s,'updateParkStatus')
sensor=block(s,'readSensorsAndSafety')
assert 'g_mlxSamples[i].update(object,ambient,millis())' in sensor
assert 'astro_roof::selectMlx(' in sensor
assert 'lastUpdate_SkyTemperature=sample.at' in sensor
assert 'MLX: przelaczenie MLX' in sensor and 'telegramSend(message,true)' in sensor
assert 'lastUpdate_CloudCover = now' not in sensor
assert 'MLX_MAX_AGE_MS ? g_tAir : g_tempAmb' in sensor
assert 'rawDelta < CLOUD_DELTA_CLEAR' in s
assert 'g_acClr[b]>=CLOUD_DELTA_CLEAR' in s
assert 'astro_roof::cloudNormalize(' in s
assert '#define CLOUD_SUSTAIN_MS 180000UL' in s
handler=block(s,'handleClientImpl')
assert 'readStringUntil' not in handler and 'while (' not in handler
assert 'kInputBudget' in s and 'httpReceiveTick();' in s
assert 'g_wifiHttp[2]' in s and 'g_ethHttp[2]' in s
assert 'getFullLuminosity(' not in s
assert 'g_tslWaiting' in block(s,'tslTick') and 'delay(' not in block(s,'tslTick')
assert 'serviceRoofFast();' in block(s,'writeAll')
assert 'httpUrgentTick();' in block(s,'writeAll')
assert 'if(busy) return;' in block(s,'httpUrgentTick') and 'dispatching=true;' in s
assert 'getParamValue(params,"do")' in s[s.index('bool urgentRequest()'):s.index('void dispatch(bool')]
assert 'roofState!=RoofState::ERROR' in block(s,'safetyInputsUsable')
assert '!g_mountEstop' in block(s,'safetyInputsUsable') and 'modbusRelayOK' in block(s,'safetyInputsUsable')
assert sensor.index('g_wind_ms = readWindSpeedMS()')<sensor.index('sensorRecoveryTick(now)')
assert 'safetyInputsUsable()' in s[s.index('member == "issafe"'):s.index('// ObservingConditions')]
assert len(re.findall(r'\bmanualModeOff\(',s))==2 # definition, explicit off only
for member in ['openshutter','closeshutter']:
    route=s[s.index('member == "'+member+'"'):]
    route=route[:route.index('else if (member',10)]
    assert ('if (g_manualMode) alpacaSendError' in route)==(member=='openshutter')
if len(sys.argv)>2:
    rc3=Path(sys.argv[2])
    for name in ['hardware.ini','platformio.ini','do_names.ini','include/relay_map.h',
                 'include/astro_config.h','include/wind_monitor.h','src/hardware_io.h',
                 'src/local_do.h','src/rs485_service.cpp','scripts/load_config.py']:
        assert (root/name).read_bytes()==(rc3/name).read_bytes(),name+' unexpectedly changed'
    for key in ['CFG_CLOUD_CLEAR','CFG_CLOUD_SCATTERED','CFG_CLOUD_OVERCAST','CLOUD_SUSTAIN_MS',
                'CFG_WIND_WARN','CFG_WIND_SUSTAINED','CFG_WIND_GUST','CFG_WIND_RESET']:
        pattern=r'#define\s+'+key+r'\s+(\S+)'
        assert re.search(pattern,s).group(1)==re.search(pattern,(rc3/'src/main.cpp').read_text(encoding='utf-8-sig')).group(1)
    gpio=r'^\s*#define\s+(\w*(?:PIN|_ADDR))\s+([^\n/]+)'
    assert re.findall(gpio,s,re.M)==re.findall(gpio,(rc3/'src/main.cpp').read_text(encoding='utf-8-sig'),re.M),'GPIO/address macros changed'
print('RC4 PASS: production constexpr state/sensor/parser simulations, fault STOP paths, physical gates, manual ownership, timestamp provenance, hardware and threshold invariants')

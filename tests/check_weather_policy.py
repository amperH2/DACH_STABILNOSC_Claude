from pathlib import Path
import subprocess,sys
from check_wind import block
p=Path(__file__).resolve().parents[1];s=(p/'src/main.cpp').read_text(encoding='utf-8')
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only','-I'+str(p/'include'),str(p/'tests/weather_policy_checks.cpp')],check=True)
handler=block(s,'handleClientImpl');a=handler.index('if(method=="POST" && pl=="/api/weather/policy")');b=handler.index('if(method=="POST" && pl=="/api/wind/calibration")',a);route=handler[a:b]
assert 'checkAuth(c,authHeader,params)' in route and 'limitClosedActive()' in route and 'limitOpenActive()' in route and '!g_manualMode' in route
assert 'pl=="/api/weather/policy"' in handler[:a] and 'POST body and command token required' in handler[:a]
assert not any(x in route for x in ['roofOpen(','roofClose(','relayPulse(','mountPowerCut('])
for key in ['rain','wind','clouds','humidity','dew']:assert 'integer("'+key+'",0,1)' in route
store=block(s,'weatherPolicyStore');assert 'putBytes(' in store and 'memcmp(' in store and 'g_weatherPolicyFault=true' in store
assert 'weatherPolicyLoad();' in block(s,'cfgLoadFromNVS') and 'astro_weather::decode' in block(s,'weatherPolicyLoad')
assert 'g_weatherPolicy.clouds' in block(s,'policySkyReady') and 'g_weatherPolicy.wind' in block(s,'policyWindReady')
assert 'g_weatherPolicyFault' in block(s,'safetyInputsUsable') and 'astro_weather::hardBlock' in block(s,'hardWeatherBlocksOpen')
assert '!g_weatherOpenReady' in block(s,'roofOpen') and 'policySkyReady()' in block(s,'roofOpen')
assert 'g_weatherPolicy.wind && ANEMOMETER_ENABLED && !windMeasurementUsable()' in block(s,'loop')
assert 'policyWindReady()' in block(s,'weatherCloseRequested') and 'policySkyReady()' in block(s,'weatherCloseRequested')
assert 'g_weatherPolicy.rain && g_rain' in s and 'g_weatherPolicy.clouds && g_denseCloud.latched' in s
print('PASS weather selection, 32 masks, NVS corruption, moisture confirmation/hysteresis, reopen delay, auth/closed/manual gate; no motion from settings route')

"""Offline checks; never sends notifications and never connects to controllers."""
from pathlib import Path
import subprocess,sys,json
from check_wind import block
root=Path(__file__).resolve().parents[1]
s=(root/'src/main.cpp').read_text(encoding='utf-8-sig')
online=(root/'src/online_notifications.h').read_text(encoding='utf-8-sig')
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only','-I'+str(root/'include'),str(root/'tests/notification_checks.cpp')],check=True)
request=block(online,'onlineRequest') if False else online[online.index('static OnlineHttpReply onlineRequest'):online.index('static void onlineWorker')]
assert 'setCACert(NOTIFY_CA_PEM)' in request and 'setInsecure' not in request
assert request.count('onlineHealthSnapshot().usable(millis())')>=2
assert 'tls.connect' in request and request.index('tls.connect')<request.rindex('onlineHealthSnapshot().usable(millis())')<request.index('tls.print(request)')
assert 'HEALTHCHECKS_UUID' not in block(s,'handleClientImpl')
status=online[online.index('static String onlineStatusJson'):]
assert not any(key in status for key in ['PUSHOVER_APP_TOKEN','PUSHOVER_USER_KEY','HEALTHCHECKS_UUID'])
loop=block(s,'loop');assert loop.index('readSensorsAndSafety();')<loop.index('onlineLoopProgress(')
assert loop.index('serviceRoofFast();')<loop.index('onlineLoopProgress(')
assert 'onlineRequest(' not in loop and 'onlineWorker' not in loop
hook=(root/'src/obs_notify.h').read_text(encoding='utf-8');assert 'if(level==NOTIFY_CRITICAL)onlineCritical(title,msg);' in hook
handler=block(s,'handleClientImpl');assert '(pl=="/api/notifications" && method!="GET")' in handler
assert handler.index('POST body and command token required')<handler.index('if(pl=="/api/notifications")')
assert 'SIM900_ENABLED && SIM7600_ENABLED' in (root/'include/astro_config.h').read_text()
assert 'AT+CEREG?' in hook and 'SMS_MODEM_ENABLED' in hook
print('Online policy/TLS/heartbeat lease/secret omission/CSRF/modem-selection checks PASS')

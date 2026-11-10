from pathlib import Path
import configparser,json,re

def load_hardware(path,profile):
    schema=json.loads((Path(__file__).parent/'config_schema.json').read_text())
    cfg=configparser.ConfigParser(interpolation=None,strict=True);cfg.optionxform=str
    cfg.read(path,encoding='utf-8-sig')
    if set(cfg.sections()) != {'common','poe_main','eth_8ro_spare','rs485_service'}:
        raise ValueError('hardware.ini: missing or unknown sections')
    allowed=set(schema['switches']+schema['parameters'])
    for section in cfg.sections():
        for key,value in cfg[section].items():
            if key not in allowed:raise ValueError('Unknown hardware flag: '+key)
            if not value.strip():raise ValueError('Empty hardware flag: '+key)
            if key in schema['switches'] and value not in ('0','1'):
                raise ValueError(key+' must be 0 or 1')
            if not re.fullmatch(r'[+-]?\d+',value):raise ValueError(key+' must be an integer')
    values=dict(cfg['common']);values.update(cfg[profile])
    if set(values)!=allowed:raise ValueError('Missing hardware options: '+','.join(sorted(allowed-set(values))))
    result={k:int(v) for k,v in values.items()}
    for key,(lo,hi) in schema['ranges'].items():
        if not lo<=result[key]<=hi:raise ValueError(key+' outside allowed range')
    for key,choices in schema['choices'].items():
        if result[key] not in choices:raise ValueError(key+' has an unsupported value')
    if result['OBS_BOARD_8RO'] != int(profile=='eth_8ro_spare'):
        raise ValueError('Board profile mismatch')
    return result

SECRET_KEYS={'UI_USER','UI_PASS','WIFI_SSID','WIFI_PASS','TELEGRAM_TOKEN','TELEGRAM_CHAT_ID',
             'PARK_ALPACA_HOST','OTA_HOSTNAME','OTA_PASSWORD','MQTT_HOST','MQTT_USER','MQTT_PASS',
             'MQTT_CLIENT_ID','SMS_TO_1','SMS_TO_2','PUSHOVER_APP_TOKEN','PUSHOVER_USER_KEY','HEALTHCHECKS_UUID','ALPACA_TRUSTED_IPS'}
def load_secrets(path):
    if not path.exists():return {}
    cfg=configparser.ConfigParser(interpolation=None,strict=True);cfg.optionxform=str
    cfg.read(path,encoding='utf-8-sig')
    if cfg.sections()!=['secrets']:raise ValueError('secrets.ini must contain only [secrets]')
    if set(cfg['secrets'])-SECRET_KEYS:raise ValueError('secrets.ini: unknown keys; hardware flags belong in hardware.ini')
    values=dict(cfg['secrets'])
    if values.get('ALPACA_TRUSTED_IPS'):
        import ipaddress
        hosts=values['ALPACA_TRUSTED_IPS'].split(',')
        if len(hosts)>8 or any(str(ipaddress.IPv4Address(h))!=h or h=='0.0.0.0' for h in hosts):
            raise ValueError('ALPACA_TRUSTED_IPS requires at most 8 exact canonical IPv4 hosts')
    return values


def load_observatory(path):
    cfg=configparser.ConfigParser(interpolation=None,strict=True);cfg.optionxform=str
    if not cfg.read(path,encoding='utf-8-sig') or cfg.sections()!=['observatory']:
        raise ValueError('observatory.ini requires [observatory]')
    keys={'OBSERVATORY_NAME','OBSERVATORY_SHORT_NAME','OBSERVATORY_HOSTNAME'}
    if set(cfg['observatory'])!=keys:raise ValueError('observatory.ini requires exactly name, short name and hostname')
    values={k:v.strip() for k,v in cfg['observatory'].items()}
    for k,v in list(values.items()):
        if v.startswith('"'):values[k]=json.loads(v)
        if not isinstance(values[k],str):raise ValueError('Identity must be text')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9 ._-]{0,63}',values['OBSERVATORY_NAME']):raise ValueError('Invalid observatory name')
    if not re.fullmatch(r'[A-Z0-9_-]{1,24}',values['OBSERVATORY_SHORT_NAME']):raise ValueError('Invalid short name')
    if not re.fullmatch(r'[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?',values['OBSERVATORY_HOSTNAME']):raise ValueError('Invalid hostname')
    return values

"""Shared read-only interpretation of the RC5.2 TEMP3 weather policy."""
from power_control import demand
FLAGS=('rain','wind','clouds','humidity','dew')

def policy(state):
    fw=state.get('fw','')
    demand(isinstance(fw,str) and fw.startswith('ASTRO-ROOF SAFE RC5.2 '),'Nieznany firmware ESP32')
    p=state.get('weather_policy')
    if p is None:
        demand('TEMP3' not in fw,'TEMP3: brak wymaganej polityki pogody')
        return None
    demand(isinstance(p,dict) and type(p.get('version')) is int and p['version']==1
           and p.get('config_fault') is False,'Nieznana lub uszkodzona polityka pogody')
    demand(all(type(p.get(k)) is bool for k in FLAGS),'Niepelny wybor czujnikow')
    demand(type(p.get('open_ready')) is bool,'Brak stanu gotowosci AUTO')
    for key,low,high in [('reopen_sec',60,3600),('cloud_hold_sec',30,1800),
                         ('moisture_hold_sec',10,1800),('humidity_limit',50,100),('dew_tenths',0,100)]:
        demand(type(p.get(key)) is int and low<=p[key]<=high,'Niepoprawny parametr polityki: '+key)
    if p['humidity'] or p['dew']:
        demand(p.get('thermo_valid') is True,'Brak wiarygodnego pomiaru wilgotnosci/rosy')
    return p

def signature(p):
    if p is None:return None
    return tuple((k,p[k]) for k in (*FLAGS,'reopen_sec','cloud_hold_sec','moisture_hold_sec','humidity_limit','dew_tenths'))

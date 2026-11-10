"""Read-only capture gate: no roof, mount, power or relay commands."""
import sys
from weather_contract import policy
from pathlib import Path
from power_control import Blocked,Client,load_json,credentials,BUNDLE,LOGS,demand

def require_capture_ready(st):
 demand(isinstance(st,dict),'Brak poprawnego stanu ESP32')
 demand(st.get('safe') is True,'UNSAFE: '+str(st.get('reason','brak przyczyny')))
 roof=st.get('roof');ext=st.get('safety_ext');sensors=st.get('sensors')
 demand(all(isinstance(x,dict) for x in (roof,ext,sensors)),'Niepelny stan ESP32')
 demand(roof.get('state')=='OPEN' and roof.get('limit_open') is True and roof.get('limit_closed') is False,'Dach nie jest jednoznacznie OTWARTY')
 demand(roof.get('motion_fault')=='','Blad ruchu dachu')
 demand(st.get('manual_mode') is False,'Fotografowanie wymaga AUTO')
 demand(ext.get('mount_estop') is False and ext.get('comms_lost') is False,'Blokada montazu lub utrata lacznosci')
 p=policy(st)
 if p is None:p={'rain':True,'wind':True,'humidity':False,'dew':False}
 if p['rain']:demand(sensors.get('rain') is False,'Deszcz lub nieznany stan RG11')
 if p['wind']:demand(sensors.get('wind_valid') is True,'Brak wiarygodnego pomiaru wiatru')
 if p['humidity'] or p['dew']:demand(p.get('thermo_valid') is True,'Brak wiarygodnego pomiaru wilgotnosci/rosy')
 return True

def main():
 try:
  cfg=load_json(BUNDLE/'USTAWIENIA/config.json');require_capture_ready(Client(cfg,credentials()).esp());return 0
 except (Blocked,OSError,ValueError) as exc:
  text='STOP ZDJEC/AF: '+str(exc)+'; przejscie do instrukcji koncowych. Dach nie jest tu sterowany.'
  print(text,flush=True);LOGS.mkdir(exist_ok=True);(LOGS/'BLOKADA_FOTOGRAFOWANIA.txt').write_text(text,encoding='utf-8');return 2
if __name__=='__main__':raise SystemExit(main())

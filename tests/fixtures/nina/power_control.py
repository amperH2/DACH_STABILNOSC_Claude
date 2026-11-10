"""Local Inveo CH1 power integration. No switching unless explicitly commissioned.

Only stdlib; Windows credentials are protected for the interactive NINA user by DPAPI.
No roof/mount commands are implemented here. NINA owns those operations.
"""
from pathlib import Path
import argparse,base64,contextlib,ctypes,json,os,re,sys,time,uuid
import urllib.request,urllib.error,xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent
BUNDLE=ROOT.parent
PRIVATE=BUNDLE/"DANE_PRYWATNE"
LOGS=BUNDLE/"LOGI"
class Blocked(RuntimeError): pass

def demand(condition,message):
    if not condition: raise Blocked(message)

def load_json(path):
    try: return json.loads(path.read_text(encoding='utf-8-sig'))
    except (OSError,ValueError): raise Blocked('Brak lub uszkodzony plik: '+path.name) from None

def save_json(path,data):
    temp=path.with_suffix('.tmp')
    temp.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding='utf-8')
    os.replace(temp,path)

def credentials():
    demand(os.name=='nt','Dane logowania wymagaja Windows i konta uzytkownika NINA.')
    path=PRIVATE/'credentials.dpapi'
    demand(path.exists(),'Uruchom 05_KONFIGURACJA.cmd na koncie Windows, na ktorym pracuje NINA.')
    class Blob(ctypes.Structure):
        _fields_=[('size',ctypes.c_ulong),('data',ctypes.POINTER(ctypes.c_ubyte))]
    raw=path.read_bytes();buffer=(ctypes.c_ubyte*len(raw)).from_buffer_copy(raw)
    src=Blob(len(raw),buffer);dst=Blob()
    try:
        demand(ctypes.windll.crypt32.CryptUnprotectData(ctypes.byref(src),None,None,None,None,1,ctypes.byref(dst)),
               'Nie mozna odczytac danych logowania. Uruchom konfiguracje na koncie NINA.')
        return json.loads(ctypes.string_at(dst.data,dst.size).decode('utf-8-sig'))
    finally:
        if dst.data: ctypes.windll.kernel32.LocalFree(dst.data)

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs): return None

class Client:
    def __init__(self,cfg,creds=None):
        self.cfg=cfg;self.creds=creds or {}
        self.opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())
    def get(self,target,path):
        # Fixed private IPs: credentials never follow redirects or enter URL/query strings.
        host=self.cfg['inveo_ip'] if target=='inveo' else self.cfg['esp_host']
        demand(host==('192.168.1.15' if target=='inveo' else '192.168.1.240:11111'),'Nieoczekiwany adres urzadzenia.')
        headers={'Cache-Control':'no-cache','Connection':'close'}
        if target in self.creds:
            auth=self.creds[target]
            token=base64.b64encode((auth['username']+':'+auth['password']).encode('utf-8')).decode('ascii')
            headers['Authorization']='Basic '+token
        req=urllib.request.Request('http://'+host+path,headers=headers,method='GET')
        try:
            with self.opener.open(req,timeout=3) as response:
                data=response.read(100001)
                demand(len(data)<=100000,'Zbyt duza odpowiedz urzadzenia.')
                return data
        except urllib.error.HTTPError as e:
            raise Blocked(f'{target}: HTTP {e.code}; sprawdz lokalne dane logowania/uslugi.') from None
        except (OSError,urllib.error.URLError):
            raise Blocked(target+': brak potwierdzonej odpowiedzi sieciowej.') from None
    def identity(self):
        try: xml=ET.fromstring(self.get('inveo','/names.xml'))
        except ET.ParseError: raise Blocked('Inveo: niepoprawny XML identyfikacji.') from None
        demand(xml.findtext('prod')=='LantickPro-8-0' and xml.findtext('user')=='Amper',
               'Inveo: inny model lub nazwa urzadzenia; sterowanie zablokowane.')
        return {'model':xml.findtext('prod'),'name':xml.findtext('user')}
    def relay(self):
        try: xml=ET.fromstring(self.get('inveo','/status.xml'))
        except ET.ParseError: raise Blocked('Inveo: niepoprawny XML stanu.') from None
        values={name:xml.findtext(name) for name in ('out','on')}
        for text in values.values(): demand(isinstance(text,str) and re.fullmatch('[01]{16}',text),'Inveo: brak jednoznacznego stanu wyjsc.')
        # Manufacturer examples: CH1 is the rightmost/least significant bit.
        demand(values['out']==values['on'],'Inveo: stan wyjsc rozni sie od zadanego; sprawdz tryb kanalu.')
        return int(values['out'],2)
    def esp(self):
        try: return json.loads(self.get('esp','/api/state').decode('utf-8'))
        except (ValueError,UnicodeError): raise Blocked('ESP32: niepoprawny stan JSON.') from None
    def command(self,on):
        # One explicit ON/OFF for CH1 only. Never toggle or issue an all-output command.
        return self.get('inveo','/stat.php?'+('on' if on else 'off')+'=1')

def physical_gate(st):
    demand(isinstance(st,dict) and str(st.get('fw','')).startswith('ASTRO-ROOF SAFE RC5.2 '),'ESP32: nierozpoznany sterownik.')
    roof=st.get('roof',{});ext=st.get('safety_ext',{});diag=st.get('diag',{})
    demand(st.get('manual_mode') is False,'Wymagany AUTO; automatyka nie odcina zasilania w MANUAL.')
    demand(roof.get('state')=='CLOSED' and roof.get('limit_closed') is True and roof.get('limit_open') is False,
           'Dach nie ma jednoznacznie potwierdzonego CLOSED.')
    for key in ('park_guard_enabled','park_ra','park_dec','park_hw_stable','park_motion_ready'):
        demand(st.get(key) is True,'Brak potwierdzonego warunku: '+key)
    demand(roof.get('motion_fault')=='','Zatrzasniety blad ruchu dachu.')
    for key in ('mount_estop','mount_limit_raw','ota_active'):
        demand(ext.get(key) is False,'Blokada lub nieznany stan: '+key)
    demand(diag.get('collision_active') is False,'Kolizja lub brak danych czujnika.')
    if roof.get('stop_attempted') is True:
        demand(roof.get('stop_command_confirmed') is True and roof.get('stop_release_confirmed') is True,
               'STOP lub jego zwolnienie niepotwierdzone.')
    else: demand(roof.get('stop_attempted') is False,'Brak stanu STOP.')
    demand(st.get('modbus_relay_ok') is True and st.get('modbus_relay_readback_current') is True,'Nieaktualny stan przekaznikow dachu.')
    actual=st.get('modbus_relay_actual')
    demand(isinstance(actual,list) and len(actual)>=3 and all(x is False for x in actual[:3]),'Wejscia OPEN/CLOSE/STOP nie sa potwierdzone jako OFF.')
    uptime=diag.get('uptime_s')
    demand(type(uptime) in (int,float) and uptime>=0,'Brak czasu pracy ESP32.')
    return uptime

def commissioned(cfg):
    demand(cfg.get('managed_channel')==1,'Ta wersja obsluguje tylko wspolne CH1; nie zmieniaj kanalow przed przepieciem instalacji.')
    demand(cfg.get('switching_enabled') is True,'Nowy zestaw nieaktywny. Uruchom 05_KONFIGURACJA.cmd raz na koncie NINA. Nie trzeba odpowiadac na pytania przy kazdej sesji.')
    for key in ('ch1_strip_confirmed','no_critical_devices_on_strip','static_mode_confirmed'):
        demand(cfg.get(key) is True,'Niepotwierdzone uruchomienie: '+key)
    demand(type(cfg.get('output_on_means_power_on')) is bool,'Niepotwierdzona polaryzacja zasilania CH1.')
    delay=cfg.get('startup_settle_seconds')
    demand(type(delay) is int and 1<=delay<=300,'Ustaw sprawdzony czas gotowosci urzadzen (1..300 s).')

class Controller:
    def __init__(self,cfg,client,state_path,clock=time.time,sleep=time.sleep,sequence='session',validator=None):
        self.cfg=cfg;self.api=client;self.path=Path(state_path);self.clock=clock;self.sleep=sleep
        self.sequence=sequence;self.validator=validator or validate_sequence
    def stable_gate(self,previous_uptime=None):
        first=physical_gate(self.api.esp());self.sleep(1);second=physical_gate(self.api.esp())
        demand(second>=first and (previous_uptime is None or first>=previous_uptime),'ESP32 zrestartowalo sie; ponow weryfikacje instalacji.')
        return second
    def session(self,phase):
        s=load_json(self.path);age=self.clock()-s.get('started',0)
        demand(s.get('phase')==phase and s.get('run_id') and 0<=age<=36*3600,'Brak aktualnej sesji zasilania lub niewlasciwy etap. Bez automatycznego ponawiania.')
        demand(s.get('configuration_hash')==configuration_hash(self.cfg),'Konfiguracja zmienila sie podczas sesji; automatyka przerwana.')
        return s
    def write_confirmed(self,wanted,previous):
        command_error=None
        try: self.api.command(wanted)
        except Blocked as e: command_error=e
        # A missing HTTP response is not proof that the action failed. Read back;
        # never repeat the output command automatically.
        for _ in range(3):
            self.sleep(1);actual=self.api.relay()
            demand((actual & ~1)==(previous & ~1),'Inveo: zmienil sie inny kanal; przerwano.')
            demand(bool(actual & 1)==wanted,'Inveo: niepotwierdzony stan CH1. Sprawdz instalacje; nie ponawiaj slepo.')
        return command_error is None
    def run(self,action):
        self.api.identity()
        if action=='status': return {'identity':self.api.identity(),'output_mask':self.api.relay(),'switching_enabled':self.cfg.get('switching_enabled') is True}
        if action=='check':
            return {'physical_gate':'PASS','esp_uptime':self.stable_gate(),'output_mask':self.api.relay(),'switching_enabled':self.cfg.get('switching_enabled') is True}
        commissioned(self.cfg)
        self.validator(self.sequence) # Verify safety structure, not formatting or generated IDs.
        if action=='on':
            if self.path.exists():
                old=load_json(self.path)
                if old.get('phase')=='running':
                    state=self.session('running');self.stable_gate(state['esp_uptime'])
                    snapshot=self.api.esp();physical_gate(snapshot)
                    demand(snapshot.get('safe') is True,'Brak SAFE przy ponownym starcie.')
                    demand(bool(self.api.relay() & 1)==self.cfg['output_on_means_power_on'],'Zasilanie zmienilo stan; brak ponownego polecenia ON.')
                    return {'power':'ON','session':state['run_id'],'reused_without_switching':True}
                demand(old.get('phase')=='off','Pozostala niepewna poprzednia operacja. Sprawdz ja; nie kasuj automatycznie stanu.')
            uptime=self.stable_gate()
            startup=self.api.esp();physical_gate(startup)
            demand(startup.get('safe') is True,'Brak SAFE przed wlaczeniem sesji: '+str(startup.get('reason') or 'sterownik nie podal przyczyny'))
            bits=self.api.relay();wanted=self.cfg['output_on_means_power_on']
            state={'run_id':uuid.uuid4().hex,'started':self.clock(),'phase':'on_requested','esp_uptime':uptime,'configuration_hash':configuration_hash(self.cfg)}
            save_json(self.path,state)
            if bool(bits & 1)!=wanted:self.write_confirmed(wanted,bits)
            self.sleep(self.cfg['startup_settle_seconds'])
            demand(bool(self.api.relay() & 1)==wanted,'Zasilanie nie pozostalo w stanie ON.')
            state['phase']='running';save_json(self.path,state)
            return {'power':'ON','session':state['run_id']}
        if action=='prepare-off':
            if self.sequence=='shutdown' and (not self.path.exists() or load_json(self.path).get('phase')=='off'):
                # Explicit NINA end-only procedure: WarmCamera precedes this checkpoint.
                state={'run_id':uuid.uuid4().hex,'started':self.clock(),'phase':'running','configuration_hash':configuration_hash(self.cfg),'esp_uptime':self.stable_gate()}
            else:state=self.session('running')
            uptime=self.stable_gate(state['esp_uptime'])
            demand(bool(self.api.relay() & 1)==self.cfg['output_on_means_power_on'],'Zasilanie zmienilo stan podczas sesji.')
            # This step is placed ONLY after strict WarmCamera success in NINA.
            state.update(phase='ready',ready_at=self.clock(),esp_uptime=uptime,ready_sequence=self.sequence)
            save_json(self.path,state);return {'shutdown_ticket':'ready; expires in 120s'}
        if action=='off':
            state=self.session('ready')
            demand(state.get('ready_sequence')==self.sequence,'Inna procedura koncowa; pozostawiono zasilanie.')
            demand(0<=self.clock()-state.get('ready_at',0)<=120,'Zgoda na zakonczenie wygasla; pozostawiono zasilanie.')
            self.stable_gate(state['esp_uptime']);bits=self.api.relay()
            demand(bool(bits & 1)==self.cfg['output_on_means_power_on'],'CH1 ma nieoczekiwany stan przed OFF.')
            # Last check immediately before request. No firmware command is sent.
            final_uptime=physical_gate(self.api.esp())
            demand(final_uptime>=state['esp_uptime'],'ESP32 zrestartowalo sie przed OFF.')
            demand(0<=self.clock()-state.get('ready_at',0)<=120,'Zgoda wygasla podczas sprawdzania stanow.')
            state['phase']='off_requested';save_json(self.path,state)
            self.write_confirmed(not self.cfg['output_on_means_power_on'],bits)
            state['phase']='off';save_json(self.path,state)
            return {'power':'OFF','session':state['run_id']}
        raise Blocked('Nieznana operacja.')

def validate_sequence(flavor):
    from sequence_contract import validate
    try:validate(BUNDLE/'NINA'/('03_ZAKONCZ_SESJE.json' if flavor=='shutdown' else '01_SESJA_M31.json'))
    except (ValueError,OSError) as e:raise Blocked('Niepoprawna kolejnosc zabezpieczen sekwencji; uruchom 06_NOWY_KOMPUTER_LUB_PRZENIESIENIE.cmd. '+str(e)) from None

def park_precheck(st):
    demand(isinstance(st,dict) and str(st.get('fw','')).startswith('ASTRO-ROOF SAFE RC5.2 '),'Wymagany stan RC5.2 przed parkowaniem.')
    roof=st.get('roof',{});ext=st.get('safety_ext',{})
    demand(roof.get('state') not in ('OPENING','CLOSING','ERROR','UNKNOWN') and roof.get('state') in ('OPEN','CLOSED','STOPPED'),'Dach nie ma potwierdzonego stanu spoczynku; parkowanie przerwane.')
    demand(ext.get('mount_estop') is False and ext.get('mount_limit_raw') is False,'Ochrona montazu aktywna lub nieznana.')
    demand(not (roof.get('limit_closed') and roof.get('limit_open')),'Sprzeczne krancowki dachu.')
    demand(st.get('modbus_relay_readback_current') is True and st.get('modbus_relay_ok') is True,'Brak swiezego odczytu przekaznikow dachu.')
    coils=st.get('modbus_relay_actual')
    demand(isinstance(coils,list) and len(coils)>=3 and all(x is False for x in coils[:3]),'Wejscia ruchu dachu nie sa potwierdzone jako OFF.')
    demand(st.get('close_clearance_confirmed') is True or (roof.get('state')=='OPEN' and roof.get('limit_open') is True and roof.get('limit_closed') is False),'Bez potwierdzonego przeswitu parkowanie wymaga calkowicie otwartego dachu.')
    return {'park_request_gate':'PASS; sam odczyt, nie zlecono ruchu'}

def wait_for_park_gate(api,clock=time.monotonic,sleep=time.sleep):
    deadline=clock()+90
    while True:
        try:return park_precheck(api.esp())
        except Blocked as e:
            if clock()>=deadline:raise Blocked('Nie rozpoczeto parkowania: '+str(e)) from None
        sleep(1) # Read-only wait for a weather closure/roof pulse to finish; never repeat motion.

def park_check(cfg,api,clock=time.monotonic,sleep=time.sleep):
    deadline=clock()+30
    while True:
        st=api.esp()
        valid=str(st.get('fw','')).startswith('ASTRO-ROOF SAFE RC5.2 ') and all(st.get(k) is True for k in ('park_ra','park_dec','park_hw_stable','park_motion_ready'))
        ext=st.get('safety_ext',{})
        valid=valid and ext.get('mount_estop') is False and ext.get('mount_limit_raw') is False
        if valid:return {'park_hardware':'RA i DEC potwierdzone przez RC5.2; pozostale blokady dachu obowiazuja osobno'}
        if clock()>=deadline:raise Blocked('Brak stabilnego PARK RA/DEC po parkowaniu; zasilanie pozostaje wlaczone.')
        sleep(1)

def sequence_hash(path):
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest()

def configuration_hash(cfg):
    import hashlib
    return hashlib.sha256(json.dumps(cfg,sort_keys=True,separators=(',',':')).encode()).hexdigest()

@contextlib.contextmanager
def locked(path):
    import msvcrt
    with path.open('a+b') as f:
        f.seek(0);f.write(b'0');f.flush();f.seek(0)
        try: msvcrt.locking(f.fileno(),msvcrt.LK_NBLCK,1)
        except OSError: raise Blocked('Inna operacja zasilania jest w toku.') from None
        try: yield
        finally: f.seek(0);msvcrt.locking(f.fileno(),msvcrt.LK_UNLCK,1)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['status','diagnose','check','on','prepare-off','off','park-check','park-precheck']);parser.add_argument('--sequence',choices=['session','shutdown'],default='session');args=parser.parse_args()
    try:
        cfg=load_json(BUNDLE/'USTAWIENIA/config.json')
        if args.action not in ('status','diagnose','check','park-check','park-precheck'):commissioned(cfg) # no network commands when unconfigured
        auth={} if args.action=='status' else credentials()
        with locked(PRIVATE/'operation.lock'):
            if args.action=='park-precheck':
                result=wait_for_park_gate(Client(cfg,auth))
            elif args.action=='park-check':
                result=park_check(cfg,Client(cfg,auth))
            elif args.action=='diagnose':
                result=diagnose(cfg,Client(cfg,auth))
                (LOGS/'OSTATNI_WYNIK.txt').write_text(result,encoding='utf-8')
                print(result)
                return 0
            else:result=Controller(cfg,Client(cfg,auth),PRIVATE/'session.json',sequence=args.sequence).run(args.action)
        message=json.dumps(result,ensure_ascii=False)
        print(message)
        (LOGS/'OSTATNI_WYNIK.txt').write_text(time.strftime('%Y-%m-%d %H:%M:%S')+' '+args.action+' OK\n'+message+'\n',encoding='utf-8')
        with (LOGS/'power.log').open('a',encoding='utf-8') as f:f.write(time.strftime('%Y-%m-%d %H:%M:%S')+' '+args.action+' '+message+'\n')
        return 0
    except Exception as e:
        # Avoid credential-bearing tracebacks and HTTP request/response dumps.
        message=str(e) if isinstance(e,Blocked) else 'Blad wewnetrzny; sterowanie przerwane. Sprawdz konfiguracje.'
        print('ODMOWA: '+message,file=sys.stderr)
        (LOGS/'OSTATNI_WYNIK.txt').write_text(time.strftime('%Y-%m-%d %H:%M:%S')+' '+args.action+' ODMOWA\n'+message+'\nNie wyslano automatycznego ponowienia. Jesli komenda byla wyslana, jej fizyczny wynik wymaga sprawdzenia.\n',encoding='utf-8')
        if args.action=='diagnose':(LOGS/'OSTATNI_WYNIK.txt').write_text('Odczyt nie powiodl sie: '+message+'\nNie wyslano komendy przelaczenia.\n',encoding='utf-8')
        with (LOGS/'power.log').open('a',encoding='utf-8') as f:f.write(time.strftime('%Y-%m-%d %H:%M:%S')+' '+args.action+' BLOCKED '+message+'\n')
        return 2

def diagnose(cfg,api):
    identity=api.identity();bits=api.relay();st=api.esp()
    try:physical_gate(st);gate='OK'
    except Blocked as e:gate=str(e)
    power=cfg.get('output_on_means_power_on')
    lines=['SPRAWDZENIE BEZ PRZELACZANIA ZASILANIA',
      'Inveo: '+str(identity.get('model'))+' / '+str(identity.get('name')),
      'Automatyka w konfiguracji: '+('AKTYWNA' if cfg.get('switching_enabled') is True else 'ZABLOKOWANA'),
      'Firmware: '+str(st.get('fw','BRAK DANYCH')),
      'Tryb ESP32: '+('MANUAL' if st.get('manual_mode') is True else 'AUTO' if st.get('manual_mode') is False else 'BRAK DANYCH'),
      'Pogoda / SAFE: '+('SAFE' if st.get('safe') is True else 'UNSAFE'),
      'Powod ze sterownika: '+str(st.get('reason') or '(brak opisu)'),
      'Dach: '+str(st.get('roof',{}).get('state','BRAK DANYCH')),
      'Pozostale warunki startu: '+gate,
      'Cyfrowy stan wyjscia CH1: '+('ON' if bits&1 else 'OFF'),
      'Ustawienie programu: zasilanie listwy jest wlaczone przy wyjsciu '+('ON' if power is True else 'OFF' if power is False else 'NIEUSTALONYM'),
      'To ustawienie trzeba porownac z faktycznym podlaczeniem; odczyt nie mierzy napiecia w listwie.',
      'Nie wyslano zadnej komendy ON/OFF. Wynik zapisano w DIAGNOZA.txt.']
    return '\n'.join(lines)+'\n'

if __name__=='__main__':sys.exit(main())

"""PC/NINA helper: park an Alpaca Telescope, confirm atpark, then close ESP roof.

Default is DRY RUN: no network calls. --execute is required for real movement.
This is not an Alpaca server and does not assume NINA exposes a Telescope API.
"""
import argparse
import base64
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path

class AlpacaError(RuntimeError):
    pass

class Alpaca:
    def __init__(self, base, device_type, number=0, user='', password='', opener=None):
        u=urllib.parse.urlsplit(base)
        if u.scheme not in ('http','https') or not u.hostname or u.username or u.password or u.query or u.fragment:
            raise AlpacaError('Invalid Alpaca base URL; keep credentials in the local config')
        if not isinstance(number,int) or number<0 or device_type not in ('telescope','dome'):
            raise AlpacaError('Invalid Alpaca device')
        self.base=base.rstrip('/')+'/api/v1/'+device_type+'/'+str(number)+'/'
        self.auth=base64.b64encode((user+':'+password).encode()).decode() if user else ''
        self.opener=opener or urllib.request.urlopen
        self.transaction=0

    def call(self, member, method='GET', timeout=5):
        self.transaction+=1
        data=urllib.parse.urlencode({'ClientID':712,'ClientTransactionID':self.transaction}).encode()
        url=self.base+member
        if method=='GET':url+='?'+data.decode();body=None
        else:body=data
        request=urllib.request.Request(url,data=body,method=method)
        if body is not None:request.add_header('Content-Type','application/x-www-form-urlencoded')
        if self.auth:request.add_header('Authorization','Basic '+self.auth)
        try:
            with self.opener(request,timeout=timeout) as response:
                raw=response.read(16385)
            if len(raw)>16384:raise AlpacaError('Oversized Alpaca reply')
            reply=json.loads(raw)
        except AlpacaError:raise
        except Exception as e:raise AlpacaError('Alpaca request failed: '+type(e).__name__) from None
        if not isinstance(reply,dict) or reply.get('ErrorNumber')!=0:
            # Do not echo arbitrary server text (or credentials) into NINA logs.
            raise AlpacaError('Alpaca rejected '+member)
        return reply.get('Value')

def park_then_close(telescope, roof, timeout=180, clock=time.monotonic, sleep=time.sleep):
    deadline=clock()+timeout
    if telescope.call('connected') is not True:raise AlpacaError('Telescope is not connected')
    if telescope.call('canpark') is not True:raise AlpacaError('Telescope driver cannot park')
    if telescope.call('atpark') is not True:
        telescope.call('park','PUT',timeout=min(5,max(.1,deadline-clock())))
    while True:
        remaining=deadline-clock()
        if remaining<=0:raise AlpacaError('Park timeout: CLOSE was not requested')
        parked=telescope.call('atpark',timeout=min(5,remaining))
        if clock()>=deadline:raise AlpacaError('Park timeout: CLOSE was not requested')
        if parked is True:break
        sleep(min(1,max(0,deadline-clock())))
    # atpark does not override roof-local reeds, collision, E-STOP or relay gates.
    roof.call('closeshutter','PUT')
    while True:
        remaining=deadline-clock()
        if remaining<=0:raise AlpacaError('CLOSE confirmation timeout: inspect roof')
        state=roof.call('shutterstatus',timeout=min(5,remaining))
        if isinstance(state,bool) or not isinstance(state,int):raise AlpacaError('Invalid shutter status')
        if state==1 and clock()<deadline:return  # ASCOM shutterClosed
        if state==4:raise AlpacaError('Roof reports shutterError')
        sleep(min(1,max(0,deadline-clock())))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',type=Path,required=True)
    parser.add_argument('--execute',action='store_true',help='Permit actual PARK/CLOSE commands')
    args=parser.parse_args()
    try:
        c=json.loads(args.config.read_text(encoding='utf-8-sig'))
        timeout=c.get('timeout_sec',180)
        if not isinstance(timeout,(int,float)) or not 10<=timeout<=600:raise AlpacaError('Invalid timeout')
        t=c['telescope'];r=c['roof']
        telescope=Alpaca(t['base_url'],'telescope',t.get('device_number',0),t.get('user',''),t.get('password',''))
        roof=Alpaca(r['base_url'],'dome',r.get('device_number',0),r.get('user',''),r.get('password',''))
        if not args.execute:
            print('DRY RUN: no network, no movement. With --execute: PARK -> confirmed atpark -> CLOSE -> shutterClosed.')
            return 0
        park_then_close(telescope,roof,timeout)
        print('PARK confirmed; roof shutterClosed confirmed by driver. Verify physical commissioning separately.')
        return 0
    except Exception as e:
        print('ERROR: '+(str(e) if isinstance(e,AlpacaError) else type(e).__name__))
        return 2

if __name__=='__main__':raise SystemExit(main())

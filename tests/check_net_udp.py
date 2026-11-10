"""Actual UDP parser simulation: failed discard must not spin; truncated headers fail."""
from pathlib import Path
import re,sys,subprocess
p=Path(__file__).resolve().parents[1]
udp=(p/'lib/Ethernet/src/EthernetUdp.cpp').read_text(encoding='utf-8')
dns=(p/'lib/Ethernet/src/Dns.cpp').read_text(encoding='utf-8')
online=(p/'src/online_notifications.h').read_text(encoding='utf-8')
main=(p/'src/main.cpp').read_text(encoding='utf-8')
assert 'tls.setTimeout(2)' in online and 'tgClient.setTimeout(3)' in main
assert 'labelBudget=128' in dns and 'millis()-packetStarted)>=250' in dns
assert not re.search(r'^\s*iUdp\.read\(',dns[dns.index('uint16_t DNSClient::ProcessResponse'):],re.M)
m=re.search(r'int EthernetUDP::parsePacket\(\)\s*\{',udp);assert m
start=udp.index('{',m.start());depth=0;body=None
for i in range(start,len(udp)):
 if udp[i]=='{':depth+=1
 elif udp[i]=='}':
  depth-=1
  if not depth:body=udp[start:i+1];break
assert body
cpp=r'''
#include <stdint.h>
#include <stddef.h>
struct Address {constexpr Address& operator=(const uint8_t*){return *this;}};
struct Chip {int available=0,received=8;
 constexpr int socketRecvAvailable(uint8_t){return available;}
 constexpr int socketRecv(uint8_t,uint8_t* b,int n){for(int i=0;i<n;++i)b[i]=0;b[7]=3;return received;}
};
struct Parser {
 uint16_t _remaining=0,_remotePort=0;uint8_t sockindex=0;Address _remoteIP;Chip Ethernet;
 uint32_t now=0;bool failRead=false,stopped=false;unsigned faults=0,reads=0;
 constexpr uint32_t millis(){return now++;}
 constexpr void astroEthTimeout(){++faults;}
 constexpr void stop(){stopped=true;}
 constexpr int read(uint8_t*,size_t n){++reads;if(failRead)return -1;int got=n<_remaining?n:_remaining;_remaining-=got;return got;}
 constexpr int parsePacket() BODY
};
constexpr bool discard_stalled(){Parser p;p._remaining=100;p.failRead=true;return p.parsePacket()==0&&p.stopped&&p.faults==1&&p.reads==1;}
constexpr bool discard_ok(){Parser p;p._remaining=100;return p.parsePacket()==0&&!p.stopped&&p._remaining==0;}
constexpr bool truncated(){Parser p;p.Ethernet.available=1;p.Ethernet.received=4;return p.parsePacket()==0&&p.stopped&&p._remaining==0;}
constexpr bool valid(){Parser p;p.Ethernet.available=1;return p.parsePacket()==3&&!p.stopped&&p._remaining==3;}
static_assert(discard_stalled()&&discard_ok(),"UDP read failure/progress handling");
static_assert(truncated()&&valid(),"UDP header integrity");
'''.replace('BODY',body)
out=p/'.pio/offline';out.mkdir(exist_ok=True,parents=True);f=out/'udp_progress.cpp';f.write_text(cpp,encoding='utf-8')
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only',str(f)],check=True)
print('PASS stalled UDP discard, short/valid UDP headers, exact DNS reads/budgets and seconds-based outer TLS timeouts')

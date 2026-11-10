"""Production writeAll simulation: large page, partial sends, stalled peer and wrap."""
from pathlib import Path
import subprocess,sys
from check_netfix import block
p=Path(__file__).resolve().parents[1]
s=(p/'src/main.cpp').read_text(encoding='utf-8')
body=block(s,'writeAll')
fixture=(p/'tests/fixtures/write_all.netfix2.cpp').read_text()
assert body==fixture[fixture.index('{'):fixture.rindex('}')+1]
cpp='''
#include <stdint.h>
#include <stddef.h>
struct Client { size_t sent=0; bool stalled=false,stopped=false,partial=false; uint32_t tick=0; unsigned fast=0;
 constexpr bool connected(){return !stopped;}
 constexpr size_t write(const uint8_t*,size_t n){tick+=20;if(stalled)return 0;if(partial&&n>128)n=128;sent+=n;return n;}
 constexpr void stop(){stopped=true;}
};
struct Run {
 Client c;
 constexpr uint32_t millis(){return c.tick;}
 constexpr void delay(int n){c.tick+=n;}
 constexpr void yield(){}
 constexpr void serviceRoofFast(){++c.fast;}
 constexpr void httpUrgentTick(){}
 constexpr void memcpy(uint8_t* dest,const uint8_t* src,size_t n){for(size_t i=0;i<n;++i)dest[i]=src[i];}
 constexpr void writeAll(Client &c,const uint8_t* data,size_t len) BODY
};
constexpr bool largePage(bool wrap){Run r;uint8_t data[138000]={};r.c.tick=wrap?0xffffff00u:0;r.writeAll(r.c,data,sizeof(data));return r.c.sent==sizeof(data)&&!r.c.stopped&&r.c.fast>100;}
constexpr bool partial(){Run r;uint8_t data[20000]={};r.c.partial=true;r.writeAll(r.c,data,sizeof(data));return r.c.sent==sizeof(data)&&!r.c.stopped;}
constexpr bool stalled(){Run r;uint8_t data[2000]={};r.c.stalled=true;r.writeAll(r.c,data,sizeof(data));return r.c.stopped&&r.c.tick<1100&&r.c.fast>0;}
static_assert(largePage(false)&&largePage(true),"138 kB page delivered including millis wrap");
static_assert(partial()&&stalled(),"partial writes and bounded stalled client");
'''.replace('BODY',body.replace('uint8_t tmp[CHUNK];','uint8_t tmp[CHUNK]={};'))
out=p/'.pio/offline';out.mkdir(parents=True,exist_ok=True);f=out/'http_page.cpp';f.write_text(cpp)
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only',str(f)],check=True)
print('PASS complete 138 kB HTTP page, partial writes, stalled peer, millis wrap and fast safety servicing')



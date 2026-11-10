"""Production writeAll simulation: large page, partial sends, stalled peer and wrap."""
from pathlib import Path
import subprocess,sys
from check_netfix import block
p=Path(__file__).resolve().parents[1]
s=(p/'src/main.cpp').read_text(encoding='utf-8')
body=block(s,'writeAll')
cpp='''
#include <stdint.h>
#include <stddef.h>
struct Client { size_t sent=0; bool stalled=false,stopped=false,partial=false,correct=true; uint32_t tick=0; unsigned fast=0;
 constexpr bool connected(){return !stopped;}
 constexpr size_t write(const uint8_t* data,size_t n){tick+=20;if(stalled)return 0;if(partial&&n>128)n=128;
   for(size_t i=0;i<n;++i)if(data[i]!=(sent+i<4 ? "HDR!"[sent+i] : 0))correct=false;
   sent+=n;return n;
 }
 constexpr void stop(){stopped=true;}
};
struct Run {
 Client c;
 constexpr uint32_t millis(){return c.tick;}
 constexpr void delay(int n){c.tick+=n;}
 constexpr void yield(){}
 constexpr void serviceRoofFast(){++c.fast;}
 constexpr void httpUrgentTick(){}
 constexpr void memcpy(uint8_t* dest,const char* src,size_t n){for(size_t i=0;i<n;++i)dest[i]=src[i];}
 constexpr size_t min(size_t a,size_t b){return a<b?a:b;}
 constexpr void writeAll(Client &c,const char* header,size_t headerLen,const char* body,size_t bodyLen) BODY
};
constexpr bool largePage(bool wrap){Run r;char data[138000]={};r.c.tick=wrap?0xffffff00u:0;r.writeAll(r.c,"HDR!",4,data,sizeof(data));return r.c.sent==sizeof(data)+4&&!r.c.stopped&&r.c.correct&&r.c.fast>100;}
constexpr bool partial(){Run r;char data[20000]={};r.c.partial=true;r.writeAll(r.c,"HDR!",4,data,sizeof(data));return r.c.sent==sizeof(data)+4&&!r.c.stopped&&r.c.correct;}
constexpr bool stalled(){Run r;char data[2000]={};r.c.stalled=true;r.writeAll(r.c,"HDR!",4,data,sizeof(data));return r.c.stopped&&r.c.tick<1100&&r.c.fast>0;}
constexpr bool alpaca(){Run r;char body[40]={};r.writeAll(r.c,"HDR!",4,body,sizeof(body));return r.c.sent==44&&r.c.correct&&r.c.fast==1;}
static_assert(largePage(false)&&largePage(true),"138 kB page delivered including millis wrap");
static_assert(partial()&&stalled(),"partial writes and bounded stalled client");
static_assert(alpaca(),"short Alpaca response header and body sent together");
'''.replace('BODY',body.replace('uint8_t tmp[CHUNK];','uint8_t tmp[CHUNK]={};'))
out=p/'.pio/offline';out.mkdir(parents=True,exist_ok=True);f=out/'http_page.cpp';f.write_text(cpp)
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only',str(f)],check=True)
print('PASS complete 138 kB HTTP page, partial writes, stalled peer, millis wrap and fast safety servicing')



"""Compile-time simulation of the actual bounded Ethernet production bodies."""
from pathlib import Path
import sys,subprocess,re
def block(s,name):
 m=re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{',s)
 assert m,name
 start=s.index('{',m.start());depth=0
 for i in range(start,len(s)):
  if s[i]=='{':depth+=1
  elif s[i]=='}':
   depth-=1
   if depth==0:return s[start:i+1]
 raise AssertionError(name)
p=Path(__file__).resolve().parents[1]
socket=(p/'lib/Ethernet/src/socket.cpp').read_text(encoding='utf-8')
wiz=(p/'lib/Ethernet/src/utility/w5100.cpp').read_text(encoding='utf-8')
client=(p/'lib/Ethernet/src/EthernetClient.cpp').read_text(encoding='utf-8')
header=(p/'include/http_bounded_client.h').read_text(encoding='utf-8')
main=(p/'src/main.cpp').read_text(encoding='utf-8')
assert socket.count('astroGuard.release();')>=3
assert 'delay(1); // let the idle task' in socket
assert 'while (1)' not in socket and 'while (readSnCR(s)) ;' not in wiz
assert 'return sent;' in client
assert 'MSG_DONTWAIT' in header and 'void flush() override {}' in header
assert 'c.setConnectionTimeout(20)' in header and 'c.writeHttp(data,size)' in header
assert 'socketSend(_sockindex, buf, size, 250)' in client
assert header.index('#include <Dns.h>')<header.index('#include <lwip/sockets.h>')
assert 'AstroHttpClient<Socket> client;' in main
assert 'USE_TASK_WDT = 1' in (p/'hardware.ini').read_text()
assert 'arduino-libraries/Ethernet@' not in (p/'platformio.ini').read_text()
for name in ['socketBegin','socketBeginMulticast','socketSend','socketRecv','socketStatus','socketClose','socketBufferData']:
 assert 'ASTRO_ETH_' in block(socket,name),name
for stage in [201,202,203,204]:assert f'diagnosticStage({stage})' in main
source=r'''
#include <stdint.h>
#include <stddef.h>
struct Guard {constexpr void release(){} constexpr bool acquire(){return true;}};
#define ASTRO_ETH_RETURN(value) Guard astroGuard;
namespace SnSR {constexpr uint8_t ESTABLISHED=0x17,CLOSE_WAIT=0x1c,CLOSED=0;}
namespace SnIR {constexpr uint8_t SEND_OK=0x10,TIMEOUT=8;}
constexpr int SPI_ETHERNET_SETTINGS=0,Sock_SEND=1;
struct Spi {int depth=0;constexpr void beginTransaction(int){++depth;}constexpr void endTransaction(){--depth;}};
struct Wiz {
 uint16_t SSIZE=2048; bool unstable=false,ack=true,closed=false,free=true,hwTimeout=false;unsigned reads=0;
 constexpr uint16_t readSnRX_RSR(uint8_t){return unstable?++reads:42;}
 constexpr uint16_t readSnTX_FSR(uint8_t){return unstable?++reads:free?2048:0;}
 constexpr uint8_t readSnSR(uint8_t){return closed?SnSR::CLOSED:SnSR::ESTABLISHED;}
 constexpr uint8_t readSnIR(uint8_t){return ack?SnIR::SEND_OK:hwTimeout?SnIR::TIMEOUT:0;}
 constexpr void writeSnIR(uint8_t,uint8_t){}
 constexpr void execCmdSn(uint8_t,int){}
};
struct Engine {
 Wiz W5100;Spi SPI;uint32_t now=0;unsigned faults=0,written=0;struct State{uint16_t TX_FSR=0;} state[4];
 constexpr uint32_t millis(){return now++;}
 constexpr void yield(){++now;}
 constexpr void delay(unsigned n){now+=n;}
 constexpr void astroEthTimeout(){++faults;}
 constexpr void write_data(uint8_t,int,const uint8_t*,uint16_t n){written+=n;}
 constexpr uint16_t getSnRX_RSR(uint8_t s) RX_BODY
 constexpr uint16_t getSnTX_FSR(uint8_t s) TX_BODY
 constexpr uint16_t socketSend(uint8_t s,const uint8_t* buf,uint16_t len,uint32_t budgetMs=2000) SEND_BODY
 constexpr bool socketSendUDP(uint8_t s) UDP_BODY
};
struct Command {
 uint32_t now=0;bool stuck=false;unsigned reads=0,faults=0;
 constexpr uint32_t millis(){return now++;}
 constexpr void astroEthTimeout(){++faults;}
 constexpr void writeSnCR(uint8_t,int){}
 constexpr uint8_t readSnCR(uint8_t){++reads;return stuck?1:reads<3?1:0;}
 constexpr void execCmdSn(uint8_t s,int _cmd) CMD_BODY
};
constexpr bool rx_ok(){Engine e;return e.getSnRX_RSR(0)==42&&e.faults==0;}
constexpr bool rx_unstable(){Engine e;e.W5100.unstable=true;return e.getSnRX_RSR(0)==0&&e.faults==1&&e.W5100.reads==17;}
constexpr bool tx_unstable(){Engine e;e.W5100.unstable=true;return e.getSnTX_FSR(0)==0&&e.faults==1;}
constexpr bool send_ok(){Engine e;return e.socketSend(0,nullptr,128)==128&&e.written==128&&e.SPI.depth==0&&e.faults==0;}
constexpr bool send_partial(){Engine e;return e.socketSend(0,nullptr,4096)==2048&&e.written==2048;}
constexpr bool send_no_space(uint32_t start){Engine e;e.now=start;e.W5100.free=false;return e.socketSend(0,nullptr,128)==0&&e.SPI.depth==0&&e.faults==1&&uint32_t(e.now-start)<260;}
constexpr bool send_no_ack(uint32_t start){Engine e;e.now=start;e.W5100.ack=false;return e.socketSend(0,nullptr,128)==0&&e.SPI.depth==0&&e.faults==1&&uint32_t(e.now-start)<2010;}
constexpr bool send_http_timeout(uint32_t start,bool hardware){Engine e;e.now=start;e.W5100.ack=false;e.W5100.hwTimeout=hardware;return e.socketSend(0,nullptr,128,250)==0&&e.SPI.depth==0&&e.faults==1&&uint32_t(e.now-start)<(hardware?20:260);}
constexpr bool udp_no_ack(){Engine e;e.W5100.ack=false;return !e.socketSendUDP(0)&&e.faults==1&&e.SPI.depth==0&&e.now<1010;}
constexpr bool command_stuck(uint32_t start){Command c;c.now=start;c.stuck=true;c.execCmdSn(0,1);return c.faults==1&&uint32_t(c.now-start)<25;}
constexpr bool command_ok(){Command c;c.execCmdSn(0,1);return c.faults==0&&c.reads==3;}
static_assert(rx_ok()&&rx_unstable()&&tx_unstable(),"Stable/unstable chip register reads");
static_assert(send_ok()&&send_partial(),"Actual TCP byte count");
static_assert(send_no_space(0)&&send_no_space(0xfffffff0u),"Bounded TX space incl wrap");
static_assert(send_no_ack(0)&&send_no_ack(0xfffffff0u)&&udp_no_ack(),"Bounded SEND and balanced SPI");
static_assert(send_http_timeout(0,false)&&send_http_timeout(0xfffffff0u,false)&&send_http_timeout(0,true),"HTTP budget and W5500 timeout IRQ");
static_assert(command_ok()&&command_stuck(0)&&command_stuck(0xfffffff0u),"Bounded chip command incl wrap");
'''
for token,body in [('RX_BODY',block(socket,'getSnRX_RSR')),('TX_BODY',block(socket,'getSnTX_FSR')),
                   ('SEND_BODY',block(socket,'socketSend')),('UDP_BODY',block(socket,'socketSendUDP')),
                   ('CMD_BODY',block(wiz,'execCmdSn'))]:source=source.replace(token,body)
out=p/'.pio/offline';out.mkdir(parents=True,exist_ok=True)
f=out/'netfix_checks.cpp';f.write_text(source,encoding='utf-8')
subprocess.run([sys.argv[1],'-std=gnu++17','-fsyntax-only',str(f)],check=True)
print('PASS production Ethernet waits, unstable registers, no TX space/SEND_OK, partial writes, SPI release, timer rollover, socket guards and nonblocking HTTP; watchdog retained')

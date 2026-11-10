#pragma once
#include <Client.h>
#if USE_W5500
#include <Ethernet.h>
#include <Dns.h> // declare legacy DNS inet_aton before lwIP defines its macro
#endif
#include <WiFiClient.h>
#include <lwip/sockets.h>
// HTTP only: all Print/Client calls made by route handlers retain this virtual
// bounded transport. WiFiClient::setTimeout does not bound its write retry loop.
inline size_t astroHttpWrite(WiFiClient& c,const uint8_t* data,size_t size) {
  if(c.fd()<0) return 0;
  const int n=::send(c.fd(),data,size,MSG_DONTWAIT);
  return n>0 ? size_t(n) : 0;
}
#if USE_W5500
inline size_t astroHttpWrite(EthernetClient& c,const uint8_t* data,size_t size) {
  return c.write(data,size); // vendored socketSend has bounded chip and send waits
}
#endif
template<class Socket> class AstroHttpClient : public Client {
  Socket socket;
public:
  void attach(Socket incoming) { socket=incoming; socket.setTimeout(1); }
  void setTimeout(unsigned long value) { socket.setTimeout(value); }
  IPAddress remoteIP() { return socket.remoteIP(); }
  int connect(IPAddress ip,uint16_t port) override { return socket.connect(ip,port); }
  int connect(const char* host,uint16_t port) override { return socket.connect(host,port); }
  size_t write(uint8_t value) override { return write(&value,1); }
  size_t write(const uint8_t* data,size_t size) override { return astroHttpWrite(socket,data,size); }
  int available() override { return socket.available(); }
  int read() override { return socket.read(); }
  int read(uint8_t* data,size_t size) override { return socket.read(data,size); }
  int peek() override { return socket.peek(); }
  void flush() override {} // never spin waiting for a remote TCP peer
  void stop() override { socket.stop(); }
  uint8_t connected() override { return socket.connected(); }
  operator bool() override { return bool(socket); }
};

#pragma once
#include <stdint.h>
// One recursive guard for short, bounded chip/socket operations. Never held
// across a DNS lookup, TCP connection wait or complete TLS transaction.
class AstroEthernetGuard {
  bool held=false;
public:
  AstroEthernetGuard();
  ~AstroEthernetGuard();
  bool acquire();
  void release();
  explicit operator bool() const { return held; }
  AstroEthernetGuard(const AstroEthernetGuard&)=delete;
  AstroEthernetGuard& operator=(const AstroEthernetGuard&)=delete;
};
void astroEthTimeout();
uint32_t astroEthTimeoutCount();
#define ASTRO_ETH_RETURN(value) AstroEthernetGuard astroGuard; if(!astroGuard) return value
#define ASTRO_ETH_VOID() AstroEthernetGuard astroGuard; if(!astroGuard) return

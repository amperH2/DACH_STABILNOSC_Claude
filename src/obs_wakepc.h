/*  ============================================================================
 *  ASTRO-ROOF  ::  BUDZENIE KOMPUTERA (Wake-on-LAN)        obs_wakepc.h   v1.0
 *  ----------------------------------------------------------------------------
 *  Przenosi Twoj skrypt z Raspberry Pi:
 *        #!/bin/bash
 *        wakeonlan 90:1B:0E:BB:EB:FD
 *  ... bezposrednio do ESP32. ESP wysyla ten sam "magic packet" przez Ethernet
 *  (W5500), wiec budzenie PC dziala NIEZALEZNIE od Raspberry Pi. Jak RPi padnie,
 *  funkcja zostaje w ESP32 i nigdy Ci nie zniknie.
 *
 *  UZYCIE:
 *    1. Wrzuc plik do src/ (obok main.cpp), w main.cpp dodaj:  #include "obs_wakepc.h"
 *    2. W obsludze /cmd dodaj:   if (doCmd == "pcwake") cmdOk = wakePC();
 *    3. Budzenie z przegladarki/telefonu:   http://<IP-ESP>/cmd?do=pcwake
 *       (zrob sobie zakladke - jedno tkniecie i komputer wstaje)
 *  ============================================================================ */
#ifndef OBS_WAKEPC_H
#define OBS_WAKEPC_H
#include <Arduino.h>

// ======================== KONFIGURACJA =====================================
// MAC karty sieciowej komputera w obserwatorium (z Twojego budzenie.sh):
//   90:1B:0E:BB:EB:FD
static const uint8_t WOL_TARGET_MAC[6] = { 0x90, 0x1B, 0x0E, 0xBB, 0xEB, 0xFD };

// Port WoL (klasyczny: 9; bywa tez 7). wakeonlan domyslnie uzywa 9.
#define WOL_PORT            9
// Adres rozgloszeniowy. 255.255.255.255 = broadcast w lokalnej sieci (OK dla WoL).
// Jak masz inna podsiec, mozesz dac np. 192.168.1.255.
#define WOL_BCAST_A         255
#define WOL_BCAST_B         255
#define WOL_BCAST_C         255
#define WOL_BCAST_D         255

#define WOL_DEBUG           1

// ======================== IMPLEMENTACJA ====================================
// Ethernet (W5500) - glowna, niezawodna droga
#if USE_W5500
  #include <Ethernet.h>
  #include <EthernetUdp.h>
  extern bool g_ethReady;
#endif
// WiFi - awaryjnie, jesli akurat dziala
#if USE_WIFI
  #include <WiFi.h>
  #include <WiFiUdp.h>
  extern bool g_wifiReady;
#endif

// Buduje 102-bajtowy magic packet: 6x 0xFF + 16x MAC
static void wolBuildPacket(uint8_t* buf) {
  for (int i = 0; i < 6; i++) buf[i] = 0xFF;
  for (int r = 0; r < 16; r++)
    for (int b = 0; b < 6; b++)
      buf[6 + r * 6 + b] = WOL_TARGET_MAC[b];
}

// Wysyla magic packet. Zwraca true jesli udalo sie wyslac choc jedna droga.
static bool wakePC() {
  uint8_t pkt[102];
  wolBuildPacket(pkt);
  IPAddress bcast(WOL_BCAST_A, WOL_BCAST_B, WOL_BCAST_C, WOL_BCAST_D);
  bool sent = false;

#if USE_W5500
  if (g_ethReady) {
    EthernetUDP udp;
    if (udp.begin(50000)) {                 // dowolny lokalny port nadawczy
      udp.beginPacket(bcast, WOL_PORT);
      udp.write(pkt, sizeof(pkt));
      if (udp.endPacket()) sent = true;
      udp.stop();
    }
#if WOL_DEBUG
    Serial.printf("[WOL] Ethernet -> %s : %s\n",
                  sent ? "wyslano" : "BLAD", "90:1B:0E:BB:EB:FD");
#endif
  }
#endif

#if USE_WIFI
  if (g_wifiReady) {
    WiFiUDP wudp;
    if (wudp.begin(50001)) {
      wudp.beginPacket(bcast, WOL_PORT);
      wudp.write(pkt, sizeof(pkt));
      if (wudp.endPacket()) sent = true;
      wudp.stop();
    }
#if WOL_DEBUG
    Serial.printf("[WOL] WiFi -> %s\n", sent ? "wyslano" : "BLAD");
#endif
  }
#endif

#if WOL_DEBUG
  if (!sent) Serial.println(F("[WOL] !! Nie wyslano - brak aktywnego lacza (Eth/WiFi)"));
#endif
  return sent;
}

#endif // OBS_WAKEPC_H

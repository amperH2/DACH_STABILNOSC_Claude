#include "modbus_protocol.h"
#pragma once
class TCA9554 {
public:
  bool begin(uint8_t addr, TwoWire* wire = &Wire) {
    _addr = addr; _wire = wire;
    // Najpierw zapisz bezpieczny stan wyjsciowy przy pinach jeszcze jako INPUT,
    // potem przelacz port na OUTPUT, zeby uniknac krotkiego impulsu po starcie.
    if (!writeReg(0x01, _shadow)) return false;
    return writeReg(0x03, 0x00);     // wszystkie 8 pinow jako wyjscia
  }
  bool digitalWrite(uint8_t ch, uint8_t level) {
    if (ch > 7) return false;
    if (level) _shadow |= (uint8_t)(1 << ch); else _shadow &= (uint8_t)~(1 << ch);
    return writeReg(0x01, _shadow);
  }
  bool readOutput(uint8_t& value) {
    _wire->beginTransmission(_addr); _wire->write(0x01);
    if (_wire->endTransmission(false)!=0 || _wire->requestFrom((int)_addr,1)!=1) return false;
    value=(uint8_t)_wire->read(); return true;
  }
  // Ustawia poczatkowy stan (spoczynkowy) przed begin(), np. wszystkie ACTIVE_LOW=HIGH=OFF.
  void setInitialShadow(uint8_t v) { _shadow = v; }
private:
  uint8_t _addr = 0x20;
  uint8_t _shadow = 0xFF;
  TwoWire* _wire = &Wire;
  bool writeReg(uint8_t reg, uint8_t val) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(val);
    return (_wire->endTransmission() == 0);
  }
};
TCA9554 tca;
bool tcaOK = false;

// ═══ ZEWNĘTRZNY MODUŁ PRZEKAŹNIKÓW WAVESHARE 17658 — MODBUS RTU ═══
// ESP32-S3-POE-ETH-8DI-8DO: TX=GPIO17, RX=GPIO18, RTS/DE=GPIO21.
// Moduł Waveshare 17658: domyślnie Modbus RTU 9600 8N1, adres 1.
#ifndef MODBUS_RELAY_ADDR
  #define MODBUS_RELAY_ADDR 1
#endif
#ifndef MODBUS_RELAY_BAUD
  #define MODBUS_RELAY_BAUD 9600
#endif
#ifndef MODBUS_RELAY_TX_PIN
  #define MODBUS_RELAY_TX_PIN 17
#endif
#ifndef MODBUS_RELAY_RX_PIN
  #define MODBUS_RELAY_RX_PIN 18
#endif
#ifndef MODBUS_RELAY_DE_PIN
  #define MODBUS_RELAY_DE_PIN 21
#endif
#ifndef MODBUS_RELAY_RESPONSE_TIMEOUT_MS
  #define MODBUS_RELAY_RESPONSE_TIMEOUT_MS 120UL
#endif
#ifndef MODBUS_RELAY_LOG
  #define MODBUS_RELAY_LOG 1
#endif

HardwareSerial relayRS485(2);
bool modbusRelayOK = false;
bool modbusRelayActualState[8] = {};
bool modbusRelayStateValid = false;
unsigned long g_lastModbusRelayPoll = 0;
unsigned long g_lastModbusPollErrorLogMs = 0;

#ifndef MODBUS_RELAY_POLL_MS
  #define MODBUS_RELAY_POLL_MS 1000UL
#endif

static uint16_t modbusCRC16(const uint8_t* data,size_t len){ return astro_modbus::crc16(data,len); }
static uint32_t lastBusActivityUs=0;
static int modbusBusRead(){ int c=relayRS485.read();if(c>=0)lastBusActivityUs=micros();return c; }
static void modbusRelayDirection(bool tx) {
  if(tx){
    const uint32_t gap=MODBUS_RELAY_BAUD>19200?1750UL:(38500000UL+MODBUS_RELAY_BAUD-1)/MODBUS_RELAY_BAUD;
    const uint32_t elapsed=micros()-lastBusActivityUs;
    if(elapsed<gap)delayMicroseconds(gap-elapsed);
  } else lastBusActivityUs=micros();
#if OBS_BOARD_8RO
  (void)tx; return; // hardware automatic RS485 direction in 8RO
#endif
  digitalWrite(MODBUS_RELAY_DE_PIN, tx ? HIGH : LOW);
  if (tx) delayMicroseconds(50);
}

static bool modbusRelayTransaction(const uint8_t* req, size_t reqLen,
                                   uint8_t* resp, size_t respMax,
                                   size_t* respLen, uint32_t timeoutMs = MODBUS_RELAY_RESPONSE_TIMEOUT_MS) {
  if (respLen) *respLen = 0;
  while (relayRS485.available()) modbusBusRead();

  modbusRelayDirection(true);
  relayRS485.write(req, reqLen);
  relayRS485.flush();
  delayMicroseconds(300);
  modbusRelayDirection(false);

  uint32_t start = millis();
  size_t n = 0;
  while (millis() - start < timeoutMs) {
    while (relayRS485.available()) {
      int c = modbusBusRead();
      if (c < 0) break;
      if (n < respMax) resp[n++] = (uint8_t)c;
    }
    // Minimalna odpowiedź Modbus RTU dla FC05 to 8 bajtów.
    if (n >= 8) break;
    yield();
  }
  if (respLen) *respLen = n;
  // FC05 odpowiedź jest dokładnym echem 8-bajtowego żądania.
  if (n != 8) return false;

  return astro_modbus::writeResponseValid(req,reqLen,resp,n);
}

static bool modbusRelayWriteCoil(uint8_t ch, bool on) {
  if (ch > 7) return false;
#if OBS_BOARD_8RO
  const bool localOK=tca.digitalWrite(ch, (on != (RELAY_ACTIVE_LOW_CFG != 0)) ? HIGH : LOW);
  if (!localOK) tcaOK=false;
  return localOK;
#endif
  uint8_t req[8] = {
    (uint8_t)MODBUS_RELAY_ADDR, 0x05,
    0x00, ch,
    (uint8_t)(on ? 0xFF : 0x00), 0x00,
    0x00, 0x00
  };
  uint16_t crc = modbusCRC16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);
  uint8_t resp[32]; size_t n = 0;
  bool ok = modbusRelayTransaction(req, sizeof(req), resp, sizeof(resp), &n);
#if MODBUS_RELAY_LOG
  if (!ok) Serial.printf("[MODBUS] CH%u %s -> BRAK ODPOWIEDZI/CRC (n=%u)\n", ch + 1, on ? "ON" : "OFF", (unsigned)n);
#endif
  return ok;
}

// Ustawia jeden kanal Waveshare Modbus RTU PWM Output 4CH.
// Rejestry: kanaly 1..4 zaczynaja sie odpowiednio od 0x0000, 0x0003,
// 0x0006 i 0x0009; freq w 0.01 Hz,
// wypelnienie w 0.01%. FC10 ustawia atomowo czestotliwosc i wypelnienie.
#ifndef HEATER_PWM_ENABLED
  #define HEATER_PWM_ENABLED 0
#endif
#ifndef HEATER_PWM_ADDR
  #define HEATER_PWM_ADDR 2
#endif
static_assert(HEATER_PWM_ADDR != MODBUS_RELAY_ADDR, "Relay and PWM modules must use distinct Modbus slave addresses");
// v17.10: HEATER_PWM_ENABLED=1 wlacza caly modul PWM 4CH (adres HEATER_PWM_ADDR).
//   CH1..CH3 = niezalezne grzalki 12 V (automat punktu rosy + tryb reczny z WWW)
//   CH4      = czerwona tasma COB LED 12 V (jasnosc reczna, niezalezna od dew point)
// Wszystkie kanaly pracuja na jednej czestotliwosci PWM_FREQ_HZ (domyslnie 1 kHz):
// dokumentacja Waveshare podaje osobne rejestry czestotliwosci, ale jedna wspolna
// wartosc eliminuje ryzyko, ze LED (min. ~500 Hz) i grzalki dzielilyby jeden timer.
#ifndef PWM_FREQ_HZ
  #define PWM_FREQ_HZ 1000
#endif
static_assert(PWM_FREQ_HZ >= 1 && PWM_FREQ_HZ <= 200000, "PWM_FREQ_HZ musi byc w zakresie 1..200000 Hz");
static const uint32_t HEATER_PWM_FREQ_CENTIHZ = (uint32_t)PWM_FREQ_HZ * 100UL; // jednostka rejestru: 0.01 Hz
#ifndef LED_PWM_ENABLED
  #define LED_PWM_ENABLED 0
#endif

#ifndef LED_PWM_CHANNEL
  #define LED_PWM_CHANNEL 4
#endif
// Tryb reczny grzalki wraca sam do AUTO po tym czasie (ochrona przed zapomnianym 100%).
#ifndef HEATER_MANUAL_TIMEOUT_MIN
  #define HEATER_MANUAL_TIMEOUT_MIN 120
#endif
extern bool g_heaterPwmOnline;
extern float g_heaterDutyPct[3];
extern float g_ledDutyPct;
extern float g_ledBrightPct;
extern bool g_ledPwmOn;

static bool heaterPwmWriteChannel(uint8_t channel, float dutyPct) {
#if PWM_MODULE_ENABLED
  if (channel < 1 || channel > 4 || !isfinite(dutyPct)) return false;
  dutyPct = constrain(dutyPct, 0.0f, 100.0f);
  const auto frame=astro_modbus::pwmFrame((uint8_t)HEATER_PWM_ADDR,channel,HEATER_PWM_FREQ_CENTIHZ,(uint16_t)lroundf(dutyPct*100.0f));
  uint8_t resp[16]; size_t n = 0;
  return modbusRelayTransaction(frame.data(), frame.size(), resp, sizeof(resp), &n);
#else
  (void)channel; (void)dutyPct;
  return false;
#endif
}

static bool heaterPwmAllOff() {
#if PWM_MODULE_ENABLED
  bool ok = true;
  for (uint8_t ch = 1; ch <= 4; ++ch) {
    if (!heaterPwmWriteChannel(ch, 0.0f)) {
      ok = false;
      if (ch == 1) break;   // modul nie odpowiada: nie blokuj loop() kolejnymi timeoutami
    }
    delay(4);
  }
  g_heaterPwmOnline = ok;
  for (uint8_t i = 0; i < 3; ++i) g_heaterDutyPct[i] = 0.0f;
  g_ledDutyPct = 0.0f;
  g_ledPwmOn = false;
  return ok;
#else
  g_heaterPwmOnline = false;
  for (uint8_t i = 0; i < 3; ++i) g_heaterDutyPct[i] = 0.0f;
  g_ledDutyPct = 0.0f;
  g_ledPwmOn = false;
  return false;
#endif
}

// Taśma LED na kanale LED_PWM_CHANNEL (domyslnie CH4). Zwraca true tylko po
// potwierdzonej odpowiedzi modulu. Blad zapisu = modul uznany za offline
// (heaterControlTick sam ponowi sondowanie co 10 s).
static bool ledPwmSet(float pct) {
#if LED_PWM_ENABLED
  if (!g_heaterPwmOnline) { g_ledDutyPct = 0.0f; return false; }
  pct = constrain(pct, 0.0f, 100.0f);
  if (heaterPwmWriteChannel((uint8_t)LED_PWM_CHANNEL, pct)) { g_ledDutyPct = pct; return true; }
  g_heaterPwmOnline = false;
  g_ledDutyPct = 0.0f;
  return false;
#else
  (void)pct;
  g_ledDutyPct = 0.0f;
  return false;
#endif
}

static bool ledPwmRequest(bool on) {
#if LED_PWM_ENABLED
  if (!g_heaterPwmOnline) { g_ledPwmOn = false; g_ledDutyPct = 0.0f; return false; }
  const float duty = on ? g_ledBrightPct : 0.0f;
  if (!ledPwmSet(duty)) { g_ledPwmOn = false; return false; }
  g_ledPwmOn = on;
  return true;
#else
  (void)on;
  g_ledPwmOn = false;
  return false;
#endif
}

#ifdef PWM_SET_ADDRESS_TO
#error "Legacy PWM_SET_ADDRESS_TO removed. Use separate rs485_service environment."
#endif

static bool modbusRelayAllOff() {
  // Używamy standardowego FC05 dla każdego z 8 kanałów zamiast
  // nieudokumentowanego skrótu 0x00FF. Dzięki temu start działa tak samo
  // niezależnie od wariantu firmware modułu 17658.
  for (uint8_t ch = 0; ch < 8; ++ch) {
    if (!modbusRelayWriteCoil(ch, MOUNT_POWER_RELAY_ENABLED && ch==MOUNT_POWER_RELAY_CH && g_mountPowerRestoreBlocked)) return false;
    delay(5);
  }
  return true;
}

static bool modbusRelayBegin() {
#if OBS_BOARD_8RO
  relayRS485.begin(MODBUS_RELAY_BAUD, SERIAL_8N1, MODBUS_RELAY_RX_PIN, MODBUS_RELAY_TX_PIN);
  uint8_t initial=RELAY_ACTIVE_LOW_CFG ? 0xFF : 0x00;
  if(MOUNT_POWER_RELAY_ENABLED && g_mountPowerRestoreBlocked)initial^=(uint8_t)(1U<<MOUNT_POWER_RELAY_CH);
  tca.setInitialShadow(initial);
  tcaOK=tca.begin(OBS_TCA_I2C_ADDR, &Wire);
  modbusRelayOK=tcaOK;
  modbusRelayStateValid=false;
  return tcaOK;
#endif
  pinMode(MODBUS_RELAY_DE_PIN, OUTPUT);
  digitalWrite(MODBUS_RELAY_DE_PIN, LOW); // odbiór
  relayRS485.begin(MODBUS_RELAY_BAUD, SERIAL_8N1, MODBUS_RELAY_RX_PIN, MODBUS_RELAY_TX_PIN);
  delay(30);
  modbusRelayStateValid = false;
  bool ok = modbusRelayAllOff();
  modbusRelayOK = ok;
  Serial.printf("[MODBUS RELAY] %s @addr=%u baud=%lu TX=%d RX=%d DE=%d\n",
                ok ? "OK" : "OFFLINE", (unsigned)MODBUS_RELAY_ADDR,
                (unsigned long)MODBUS_RELAY_BAUD, MODBUS_RELAY_TX_PIN,
                MODBUS_RELAY_RX_PIN, MODBUS_RELAY_DE_PIN);
  return ok;
}

static bool modbusRelayReadAllStates(uint8_t* states) {
  if (!states) return false;
#if OBS_BOARD_8RO
  uint8_t levels=0;
  if (!tca.readOutput(levels)) return false;
  for(uint8_t ch=0;ch<8;++ch) states[ch]=(((levels>>ch)&1) != (RELAY_ACTIVE_LOW_CFG != 0));
  return true;
#endif

  uint8_t req[8] = {
    (uint8_t)MODBUS_RELAY_ADDR, 0x01, // FC01: Read Coil Status
    0x00, 0x00,                       // start address: CH1
    0x00, 0x08,                       // read 8 channels
    0x00, 0x00
  };
  uint16_t reqCrc = modbusCRC16(req, 6);
  req[6] = (uint8_t)(reqCrc & 0xFF);
  req[7] = (uint8_t)(reqCrc >> 8);

  while (relayRS485.available()) modbusBusRead();
  modbusRelayDirection(true);
  relayRS485.write(req, sizeof(req));
  relayRS485.flush();
  delayMicroseconds(300);
  modbusRelayDirection(false);

  // Normalna odpowiedz FC01: adres, funkcja, byte count, bitmapa, CRC = 6 bajtow.
  uint8_t resp[8] = {};
  size_t n = 0;
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < MODBUS_RELAY_RESPONSE_TIMEOUT_MS) {
    while (relayRS485.available()) {
      const int c = modbusBusRead();
      if (c < 0) break;
      if (n < sizeof(resp)) resp[n++] = (uint8_t)c;
    }
    if (n >= 6) break;
    yield();
  }

  if (n != 6) return false;
  if (resp[0] != (uint8_t)MODBUS_RELAY_ADDR) return false;
  if (resp[1] != 0x01) return false;
  if (resp[2] != 0x01) return false; // 8 cewek miesci sie w jednym bajcie
  const uint16_t gotCrc = (uint16_t)resp[4] | ((uint16_t)resp[5] << 8);
  if (gotCrc != modbusCRC16(resp, 4)) return false;

  const uint8_t mask = resp[3];
  for (uint8_t ch = 0; ch < 8; ++ch)
    states[ch] = ((mask >> ch) & 0x01) != 0;
  return true;
}

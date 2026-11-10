/*  ============================================================================
 *  ASTRO-ROOF  ::  NIEZALEZNE POWIADOMIENIA  (obs_notify.h)            v1.0
 *  ----------------------------------------------------------------------------
 *  Trzy kanaly, automatycznie dobierane wg wagi zdarzenia:
 *
 *    NOTIFY_INFO     -> Telegram (po Ethernecie), tylko gdy lacze zyje
 *    NOTIFY_WARN     -> Telegram, + zapis do kolejki gdy lacze padlo
 *    NOTIFY_CRITICAL -> SMS przez SIM900  +  lokalna syrena/buzzer  +  Telegram
 *                       (SMS i syrena ZAWSZE, niezaleznie od sieci/pradu/routera)
 *
 *  SMS wysylany NIEBLOKUJACO (maszyna stanow w notifyTick()) - nie zaburza
 *  watchdoga ani roofTick(). Kolejka, ponawianie, limit antyspamowy, recovery.
 *
 *  INTEGRACJA:
 *    1. Wrzuc ten plik do projektu i w main.cpp (RAZ, u gory) dodaj:
 *         #include "obs_notify.h"
 *    2. W setup():                 notifyBegin();
 *    3. W loop() (na poczatku):    notifyTick();
 *    4. Gdy zna stan lacza:        notifySetLinkUp( eth.linkUp() );   // W5500
 *    5. Podepnij Telegram:         ustaw g_sendTelegram = twojaFunkcja;
 *    6. Zglaszaj zdarzenia, np.:
 *         notifyEvent(NOTIFY_CRITICAL, "DACH", "Nie zamknieto - DESZCZ - OTWARTE");
 *         notifyEvent(NOTIFY_WARN,     "POGODA", "UNSAFE - auto-close start");
 *         notifyEvent(NOTIFY_INFO,     "BOOT",  "Obserwatorium wystartowalo");
 *
 *  >>> ZASILANIE SIM900 <<<  (najczestsza przyczyna problemow!)
 *    SIM900 ciagnie impulsowo do ~2 A przy nadawaniu. Z byle 5V/0.5A bedzie
 *    sie resetowac w polowie SMS-a. Daj osobny zasilacz >=2 A i gruby kondensator
 *    (1000-2200 uF) przy module. Masa WSPOLNA z ESP32.
 *
 *  v17.7 (2026-09): RS485 GPIO17/18/21 jest zajety przez zewnetrzny
 *  Waveshare 17658. SIM900 zostal przeniesiony na GPIO43/44, PWRKEY -> GPIO22,
 *  syrena -> GPIO46 (dzielony z fabrycznym buzzerem).
 *  Szczegoly w komentarzach przy definicjach ponizej.
 *  ============================================================================ */
#ifndef OBS_NOTIFY_H
#define OBS_NOTIFY_H
#include <Arduino.h>
#include "notification_policy.h"
#define SMS_MODEM_ENABLED (SIM900_ENABLED || SIM7600_ENABLED)

// ======================== KONFIGURACJA - DOSTOSUJ ==========================

// --- Numery odbiorcze (format miedzynarodowy!) ---
#ifndef SMS_TO_1
#define SMS_TO_1   "+48XXXXXXXXX"     // Twoj numer
#endif
#ifndef SMS_TO_2
#define SMS_TO_2   ""                 // opcjonalny drugi numer ("" = brak)
#endif

// --- UART do SIM900 ---
// v17.0: Waveshare ESP32-S3-ETH-8DI-8RO - stare piny (47/38/39/13) KOLIDUJA
// z fabrycznymi funkcjami tej plytki (potwierdzone na oficjalnej wiki Waveshare,
// nie z domyslu): GPIO47=TF card MOSI, GPIO38=RGB LED, GPIO13=W5500 MOSI.
// GPIO39 NIE jest resetem W5500 (to bylo bledne zalozenie z niezweryfikowanego
// zrodla) - Waveshare w ogole go nie dokumentuje dla tej plytki jako
// przypisanego, wiec i tak lepiej go nie uzywac bez dodatkowej weryfikacji.
//
// NOWY UKLAD: SIM900 UART przeniesiony poza RS485, poniewaz GPIO17/18/21 sa teraz zajete
// przez zewnetrzny Waveshare 17658. Domyslnie uzywamy GPIO43/44 (UART0 TX/RX)
// jako piny routowalne ESP32-S3; jesli SIM900 nie jest uzywany, pozostaw te piny wolne.
//  ESP TX -> SIM900 RX  ;  ESP RX <- SIM900 TX
#ifndef SIM_TX_PIN
#define SIM_TX_PIN   43    // ESP nadaje -> RX modemu
#endif
#ifndef SIM_RX_PIN
#define SIM_RX_PIN   44    // ESP odbiera <- TX modemu
#endif
// PWRKEY: domyślnie NIE PODŁĄCZONY. GPIO22 nie jest dostępne na ESP32-S3,
// dlatego nie wolno go używać jako fizycznego GPIO na tej płytce. Jeśli chcesz
// sterować PWRKEY sprzętowo, wskaż potwierdzony wolny pin lub wyjście ekspandera.
#ifndef SIM_PWRKEY
#define SIM_PWRKEY   -1
#endif
#ifndef SIM_BAUD
#define SIM_BAUD     9600  // SIM900 czesto 9600; jak autobaud - ustaw AT+IPR=9600
#endif

// --- Lokalna syrena / buzzer (direct GPIO, NIE przez MCP - ma byc niezalezny!) ---
// v17.0: GPIO13 kolidowal z W5500 MOSI. GPIO46 to POTWIERDZONY (wiki Waveshare)
// fabryczny buzzer plytki - podlaczajac syrene/MOSFET na ten sam pin, syrena
// bedzie sie wlaczac razem z wbudowanym buzzerem (to zamierzone: jeden,
// pewny, udokumentowany pin zamiast kolejnego niezweryfikowanego GPIO).
#ifndef LOCAL_ALARM_PIN
#define LOCAL_ALARM_PIN     46
#endif
#ifndef ALARM_ACTIVE_HIGH
#define ALARM_ACTIVE_HIGH   true  // true: HIGH wlacza syrene
#endif

// --- Limity antyspamowe ---
#define SMS_MIN_GAP_MS      20000UL   // min odstep miedzy SMS-ami (ms)
#define SMS_DEDUP_MS        300000UL  // ten sam tekst nie czesciej niz co 5 min
#ifndef SMS_MAX_PER_HOUR
#define SMS_MAX_PER_HOUR    8         // twardy limit na godzine (ochrona kosztow)
#endif
#define SMS_RETRY           2         // ile ponowien przy bledzie wysylki

// --- Diagnostyka ---
#define NOTIFY_DEBUG        1         // 1 = log na Serial

// ======================== TYPY / API =======================================
enum NotifyLevel { NOTIFY_INFO = 0, NOTIFY_WARN = 1, NOTIFY_CRITICAL = 2 };

void notifyBegin();
void notifyTick();
void notifyEvent(NotifyLevel level, const char* title, const char* msg);
void notifySetLinkUp(bool up);
void notifyClearAlarm();              // wycisza lokalna syrene po skasowaniu przez czlowieka
bool notifyGsmReady();                // czy modem zarejestrowany w sieci

// Podepnij tu swoja istniejaca funkcje Telegrama (po Ethernecie).
// Ma zwrocic true gdy wyslano. Jesli nie ustawisz - Telegram pomijany.
typedef bool (*SendTelegramFn)(const char* title, const char* msg);
extern SendTelegramFn g_sendTelegram;

// ======================== IMPLEMENTACJA ====================================
#ifdef OBS_NOTIFY_IMPL_GUARD
#error "obs_notify.h dolaczony wielokrotnie z implementacja - dolacz RAZ."
#endif
#define OBS_NOTIFY_IMPL_GUARD

static HardwareSerial SIM(1);               // UART1 dla modemu
SendTelegramFn g_sendTelegram = nullptr;

// --- stan lacza i kolejka ---
static bool     g_linkUp     = false;
static bool     g_gsmReady   = false;
static bool     g_gsmPresent = false;   // true = SIM900 odpowiedzial choćby raz (handshake OK)
static uint32_t g_lastSmsAt  = 0;
static uint32_t g_hourStart  = 0;
static uint8_t  g_smsThisHour= 0;

// dedupe (ostatni tekst + czas)
static String   g_lastSmsText = "";
static uint32_t g_lastSmsTextAt = 0;

// kolejka SMS (prosty ring)
#define SMSQ_LEN 6
struct SmsItem { String text; uint8_t triesLeft; bool used; };
static SmsItem  g_q[SMSQ_LEN];
static uint8_t  g_qHead = 0, g_qTail = 0;

// --- lokalny alarm ---
static bool     g_alarmLatched = false;

static inline void alarmWrite(bool on) {
  if (!LOCAL_ALARM_ENABLED) return;
  digitalWrite(LOCAL_ALARM_PIN, (on == ALARM_ACTIVE_HIGH) ? HIGH : LOW);
}

// =================== NISKOPOZIOMOWE: silnik AT (nieblokujacy) ===============
enum AtRes { AT_PENDING, AT_OK, AT_FAIL };
static String   g_atBuf;
static String   g_atExpect;
static uint32_t g_atDeadline = 0;
static bool     g_atBusy = false;

static void atStart(const String& cmd, const char* expect, uint32_t toMs) {
  // oprozimy bufor RX
  while (SIM.available()) SIM.read();
  if (cmd.length()) { SIM.print(cmd); SIM.print("\r"); }
  g_atExpect = expect;
  g_atBuf = "";
  g_atDeadline = millis() + toMs;
  g_atBusy = true;
}
static AtRes atPoll() {
  while (SIM.available()) {
    char c = (char)SIM.read();
    g_atBuf += c;
    if (g_atExpect.length() && g_atBuf.indexOf(g_atExpect) >= 0) { g_atBusy = false; return AT_OK; }
    if (g_atBuf.indexOf("ERROR") >= 0) { g_atBusy = false; return AT_FAIL; }
    if (g_atBuf.length() > 400) g_atBuf.remove(0, 200); // nie rosnij w nieskonczonosc
  }
  if (millis() > g_atDeadline) { g_atBusy = false; return AT_FAIL; }
  return AT_PENDING;
}

// =================== MASZYNA STANOW WYSYLKI SMS =============================
enum SmsState {
  S_IDLE, S_CMGF, S_CMGF_W, S_NUM, S_NUM_W, S_BODY, S_BODY_W, S_DONE, S_FAILSTEP
};
static SmsState g_sms = S_IDLE;
static String   g_smsActive;          // tresc aktualnie wysylanego
static uint8_t  g_smsRecip = 0;       // 0 = SMS_TO_1, 1 = SMS_TO_2
static uint8_t  g_smsTries = 0;

static const char* recipNum(uint8_t i) { return (i == 0) ? SMS_TO_1 : SMS_TO_2; }
static bool recipValid(uint8_t i) {
  const char* n = recipNum(i);
  return astro_notify::validPhone(n);
}

// rozpocznij wysylke konkretnej tresci do pierwszego waznego numeru
static void smsBeginSend(const String& text, uint8_t tries) {
  g_smsActive = text;
  g_smsTries  = tries;
  g_smsRecip  = recipValid(0) ? 0 : (recipValid(1) ? 1 : 255);
  if (g_smsRecip == 255) { g_sms = S_IDLE; return; }   // brak numeru
  g_sms = S_CMGF;
}

static void smsTick() {
  switch (g_sms) {
    case S_IDLE: return;

    case S_CMGF:
      atStart("AT+CMGF=1", "OK", 2000); g_sms = S_CMGF_W; break;
    case S_CMGF_W: {
      AtRes r = atPoll();
      if (r == AT_OK)   g_sms = S_NUM;
      else if (r == AT_FAIL) g_sms = S_FAILSTEP;
    } break;

    case S_NUM: {
      String cmd = String("AT+CMGS=\"") + recipNum(g_smsRecip) + "\"";
      atStart(cmd, ">", 4000); g_sms = S_NUM_W;
    } break;
    case S_NUM_W: {
      AtRes r = atPoll();
      if (r == AT_OK)   g_sms = S_BODY;          // dostalismy prompt '>'
      else if (r == AT_FAIL) g_sms = S_FAILSTEP;
    } break;

    case S_BODY: {
      SIM.print(g_smsActive);
      SIM.write(0x1A);                            // Ctrl+Z = wyslij
      g_atBuf = ""; g_atExpect = "+CMGS"; g_atDeadline = millis() + 12000; g_atBusy = true;
      g_sms = S_BODY_W;
    } break;
    case S_BODY_W: {
      AtRes r = atPoll();
      if (r == AT_OK)   g_sms = S_DONE;
      else if (r == AT_FAIL) g_sms = S_FAILSTEP;
    } break;

    case S_DONE: {
#if NOTIFY_DEBUG
      Serial.print(F("[SMS] wyslano -> ")); Serial.println(recipNum(g_smsRecip));
#endif
      // jak jest drugi numer i jeszcze do niego nie wyslano - wyslij teraz
      if (g_smsRecip == 0 && recipValid(1)) { g_smsRecip = 1; g_sms = S_CMGF; }
      else { g_sms = S_IDLE; }
    } break;

    case S_FAILSTEP: {
      if (g_smsTries > 0) {
        g_smsTries--;
#if NOTIFY_DEBUG
        Serial.println(F("[SMS] blad - ponawiam"));
#endif
        g_sms = S_CMGF;                            // ponow od poczatku
      } else {
#if NOTIFY_DEBUG
        Serial.println(F("[SMS] PORAZKA wysylki"));
#endif
        g_sms = S_IDLE;
      }
    } break;
  }
}

// =================== KOLEJKA ===============================================
static bool qPush(const String& text) {
  uint8_t next = (g_qTail + 1) % SMSQ_LEN;
  if (next == g_qHead) return false;              // pelna
  g_q[g_qTail].text = text;
  g_q[g_qTail].triesLeft = SMS_RETRY;
  g_q[g_qTail].used = true;
  g_qTail = next;
  return true;
}
static bool qPop(String& out) {
  if (g_qHead == g_qTail) return false;
  out = g_q[g_qHead].text;
  g_q[g_qHead].used = false;
  g_qHead = (g_qHead + 1) % SMSQ_LEN;
  return true;
}

// limit/dedupe - czy wolno teraz wyslac SMS
static bool smsAllowedNow(const String& text) {
  uint32_t now = millis();
  if (now - g_hourStart > 3600000UL) { g_hourStart = now; g_smsThisHour = 0; }
  if (g_smsThisHour >= SMS_MAX_PER_HOUR) return false;
  if (now - g_lastSmsAt < SMS_MIN_GAP_MS) return false;
  if (text == g_lastSmsText && (now - g_lastSmsTextAt) < SMS_DEDUP_MS) return false;
  return true;
}

// =================== RECOVERY / INIT MODEMU ================================
static uint32_t g_gsmCheckAt = 0;

enum GsmBootState {
  GSM_BOOT_IDLE, GSM_BOOT_PWR_HIGH, GSM_BOOT_PWR_LOW, GSM_BOOT_SETTLE,
  GSM_BOOT_UART_SETTLE, GSM_BOOT_AT_START, GSM_BOOT_AT_WAIT,
  GSM_BOOT_AT_RETRY_WAIT, GSM_BOOT_ECHO_START, GSM_BOOT_ECHO_WAIT,
  GSM_BOOT_CMGF_START, GSM_BOOT_CMGF_WAIT, GSM_BOOT_REG_START,
  GSM_BOOT_REG_WAIT, GSM_BOOT_DONE
};
static GsmBootState g_gsmBootState = GSM_BOOT_IDLE;
static uint32_t g_gsmBootDue = 0;
static uint8_t g_gsmBootAttempts = 0;
static bool g_gsmBootComplete = false;

enum GsmServiceState {
  GSM_SVC_IDLE, GSM_SVC_REG_WAIT, GSM_SVC_PROBE_WAIT, GSM_SVC_HS_WAIT,
  GSM_SVC_RETRY_WAIT, GSM_SVC_CMGF_WAIT, GSM_SVC_RECHECK_WAIT
};
static GsmServiceState g_gsmServiceState = GSM_SVC_IDLE;
static uint32_t g_gsmServiceDue = 0;
static uint8_t g_gsmRecoveryAttempts = 0;

static bool gsmAtRegistered() {
  // ",1" = zarejestrowany w domu, ",5" = roaming.
  const int at=g_atBuf.indexOf(SIM7600_ENABLED?"+CEREG:":"+CREG:");
  if(at<0)return false;
  const int end=g_atBuf.indexOf('\n',at);String line=g_atBuf.substring(at,end<0?g_atBuf.length():end);line.trim();
  const int comma=line.indexOf(',');if(comma<0)return false;
  const int state=line.substring(comma+1).toInt();return state==1 || state==5;
}

static void gsmBootTick() {
  switch (g_gsmBootState) {
    case GSM_BOOT_IDLE: return;
    case GSM_BOOT_PWR_HIGH:
      if ((int32_t)(millis() - g_gsmBootDue) < 0) return;
      digitalWrite(SIM_PWRKEY, LOW);
      g_gsmBootState = GSM_BOOT_PWR_LOW;
      g_gsmBootDue = millis() + 1100UL;
      return;
    case GSM_BOOT_PWR_LOW:
      if ((int32_t)(millis() - g_gsmBootDue) < 0) return;
      digitalWrite(SIM_PWRKEY, HIGH);
      g_gsmBootState = GSM_BOOT_SETTLE;
      g_gsmBootDue = millis() + 3000UL;
      return;
    case GSM_BOOT_SETTLE:
      if ((int32_t)(millis() - g_gsmBootDue) < 0) return;
      SIM.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
      g_gsmBootState = GSM_BOOT_UART_SETTLE;
      g_gsmBootDue = millis() + 300UL;
      return;
    case GSM_BOOT_UART_SETTLE:
      if ((int32_t)(millis() - g_gsmBootDue) < 0) return;
      g_gsmBootAttempts = 0;
      g_gsmBootState = GSM_BOOT_AT_START;
      return;
    case GSM_BOOT_AT_START:
      atStart("AT", "OK", 600);
      g_gsmBootState = GSM_BOOT_AT_WAIT;
      return;
    case GSM_BOOT_AT_WAIT: {
      AtRes r = atPoll();
      if (r == AT_PENDING) return;
      if (r == AT_OK) {
        g_gsmPresent = true;
        g_gsmBootState = GSM_BOOT_ECHO_START;
      } else if (++g_gsmBootAttempts < 5) {
        g_gsmBootState = GSM_BOOT_AT_RETRY_WAIT;
        g_gsmBootDue = millis() + 200UL;
      } else {
        g_gsmPresent = false;
        g_gsmReady = false;
        g_gsmBootState = GSM_BOOT_DONE;
      }
      return;
    }
    case GSM_BOOT_AT_RETRY_WAIT:
      if ((int32_t)(millis() - g_gsmBootDue) < 0) return;
      g_gsmBootState = GSM_BOOT_AT_START;
      return;
    case GSM_BOOT_ECHO_START:
      atStart("ATE0", "OK", 1000);
      g_gsmBootState = GSM_BOOT_ECHO_WAIT;
      return;
    case GSM_BOOT_ECHO_WAIT: {
      AtRes r = atPoll();
      if (r == AT_PENDING) return;
      g_gsmBootState = GSM_BOOT_CMGF_START;
      return;
    }
    case GSM_BOOT_CMGF_START:
      atStart("AT+CMGF=1", "OK", 1000);
      g_gsmBootState = GSM_BOOT_CMGF_WAIT;
      return;
    case GSM_BOOT_CMGF_WAIT: {
      AtRes r = atPoll();
      if (r == AT_PENDING) return;
      g_gsmBootState = GSM_BOOT_REG_START;
      return;
    }
    case GSM_BOOT_REG_START:
      atStart(SIM7600_ENABLED?"AT+CEREG?":"AT+CREG?", "OK", 1000);
      g_gsmBootState = GSM_BOOT_REG_WAIT;
      return;
    case GSM_BOOT_REG_WAIT: {
      AtRes r = atPoll();
      if (r == AT_PENDING) return;
      g_gsmReady = (r == AT_OK) && gsmAtRegistered();
      g_gsmBootState = GSM_BOOT_DONE;
      return;
    }
    case GSM_BOOT_DONE:
#if NOTIFY_DEBUG
      Serial.print(F("[GSM] handshake=")); Serial.print(g_gsmPresent);
      Serial.print(F(" zarejestrowany=")); Serial.println(g_gsmReady);
#endif
      g_hourStart = millis();
      g_gsmCheckAt = millis();
      g_gsmBootComplete = true;
      g_gsmBootState = GSM_BOOT_IDLE;
      return;
  }
}

static void gsmServiceTick() {
  uint32_t now = millis();
  if (g_gsmServiceState == GSM_SVC_IDLE) {
    uint32_t interval = g_gsmPresent ? 60000UL : 300000UL;
    if (now - g_gsmCheckAt < interval) return;
    g_gsmCheckAt = now;
    if (g_gsmPresent) {
      atStart(SIM7600_ENABLED?"AT+CEREG?":"AT+CREG?", "OK", 800);
      g_gsmServiceState = GSM_SVC_REG_WAIT;
    } else {
      atStart("AT", "OK", 600);
      g_gsmServiceState = GSM_SVC_PROBE_WAIT;
    }
    return;
  }

  if (g_gsmServiceState == GSM_SVC_RETRY_WAIT) {
    if ((int32_t)(now - g_gsmServiceDue) < 0) return;
    atStart("AT", "OK", 600);
    g_gsmServiceState = GSM_SVC_HS_WAIT;
    return;
  }

  AtRes r = atPoll();
  if (r == AT_PENDING) return;
  switch (g_gsmServiceState) {
    case GSM_SVC_REG_WAIT:
      g_gsmReady = (r == AT_OK) && gsmAtRegistered();
      if (g_gsmReady) {
        g_gsmServiceState = GSM_SVC_IDLE;
      } else {
        g_gsmRecoveryAttempts = 1;
        atStart("AT", "OK", 600);
        g_gsmServiceState = GSM_SVC_HS_WAIT;
      }
      break;
    case GSM_SVC_PROBE_WAIT:
      if (r == AT_OK) {
        g_gsmPresent = true;
        atStart("AT+CMGF=1", "OK", 1000);
        g_gsmServiceState = GSM_SVC_CMGF_WAIT;
      } else {
        g_gsmServiceState = GSM_SVC_IDLE;
      }
      break;
    case GSM_SVC_HS_WAIT:
      if (r == AT_OK) {
        g_gsmPresent = true;
        atStart("AT+CMGF=1", "OK", 1000);
        g_gsmServiceState = GSM_SVC_CMGF_WAIT;
      } else if (g_gsmRecoveryAttempts < 5) {
        ++g_gsmRecoveryAttempts;
        g_gsmServiceDue = now + 200UL;
        g_gsmServiceState = GSM_SVC_RETRY_WAIT;
      } else {
        g_gsmPresent = false;
        g_gsmReady = false;
        g_gsmServiceState = GSM_SVC_IDLE;
      }
      break;
    case GSM_SVC_CMGF_WAIT:
      atStart(SIM7600_ENABLED?"AT+CEREG?":"AT+CREG?", "OK", 1000);
      g_gsmServiceState = GSM_SVC_RECHECK_WAIT;
      break;
    case GSM_SVC_RECHECK_WAIT:
      g_gsmReady = (r == AT_OK) && gsmAtRegistered();
      g_gsmServiceState = GSM_SVC_IDLE;
      break;
    case GSM_SVC_IDLE:
    case GSM_SVC_RETRY_WAIT:
      break;
  }
}

// =================== PUBLICZNE API =========================================
void notifyBegin() {
  if (LOCAL_ALARM_ENABLED) pinMode(LOCAL_ALARM_PIN, OUTPUT);
  alarmWrite(false);

  if (!SMS_MODEM_ENABLED) return;
  if (SIM_PWRKEY >= 0) {
    pinMode(SIM_PWRKEY, OUTPUT);
    // Impuls PWRKEY i rozruch realizuje notifyTick(), zeby WWW dzialalo rownolegle.
    digitalWrite(SIM_PWRKEY, HIGH);
    g_gsmBootState = GSM_BOOT_PWR_HIGH;
    g_gsmBootDue = millis() + 100UL;
  } else {
    SIM.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
    g_gsmBootState = GSM_BOOT_UART_SETTLE;
    g_gsmBootDue = millis() + 300UL;
  }
}

void notifySetLinkUp(bool up) { g_linkUp = up; }
bool notifyGsmReady()         { return g_gsmReady; }
void notifyClearAlarm()       { g_alarmLatched = false; alarmWrite(false); }

void notifyEvent(NotifyLevel level, const char* title, const char* msg) {
  String full = observatoryMessage(String("[") + title + "] " + msg);
  mqttQueueAlert(full);
#if NOTIFY_DEBUG
  Serial.print(F("[NOTIFY] L")); Serial.print((int)level); Serial.print(F(" ")); Serial.println(full);
#endif

  if(level==NOTIFY_CRITICAL)onlineCritical(title,msg);

  // 1. Telegram - dla kazdego poziomu, jesli lacze zyje i mamy funkcje
  if (g_linkUp && g_sendTelegram) g_sendTelegram(title, msg);

  // 2. CRITICAL: lokalna syrena (natychmiast) + SMS (kolejka)
  if (level == NOTIFY_CRITICAL) {
    g_alarmLatched = true;                         // syrena wyje az czlowiek skasuje
    if (SMS_MODEM_ENABLED) qPush(full);
  }
  // 3. WARN przy padnietym laczu -> tez do SMS (zeby nie zginelo)
  else if (level == NOTIFY_WARN && !g_linkUp) {
    if (SMS_MODEM_ENABLED) qPush(full);
  }
}

void notifyTick() {
  uint32_t now = millis();

  // --- lokalna syrena: wzor pulsowania gdy zatrzasniety alarm ---
  if (g_alarmLatched) alarmWrite((now / 400) % 2);  // 0.4 s on / 0.4 s off
  else                alarmWrite(false);

  if (!SMS_MODEM_ENABLED) return;
  if (!g_gsmBootComplete) {
    gsmBootTick();
    return;
  }

  // --- silnik wysylki SMS ---
  if (g_sms != S_IDLE) { smsTick(); return; }       // zajety wysylka - skoncz ja najpierw

  // Okresowa kontrola modemu korzysta z tego samego UART co SMS, wiec dziala
  // jako nieblokujaca maszyna stanow i ma pierwszenstwo tylko do zakonczenia komendy.
  if (g_gsmServiceState != GSM_SVC_IDLE) { gsmServiceTick(); return; }
  gsmServiceTick();
  if (g_gsmServiceState != GSM_SVC_IDLE) return;

  // --- czy cos w kolejce i czy wolno wyslac ---
  if (g_qHead != g_qTail) {
    String txt = g_q[g_qHead].text;
    if (smsAllowedNow(txt)) {
      String popped; qPop(popped);
      g_lastSmsAt = now; g_smsThisHour++;
      g_lastSmsText = popped; g_lastSmsTextAt = now;
      smsBeginSend(popped, SMS_RETRY);
    }
  }

}

#endif // OBS_NOTIFY_H

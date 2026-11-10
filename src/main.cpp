/* ASTRO-ROOF SAFE RC5.2 TEMP3 — audited joint set, 2026-10-28.
   Shared logic for POE 8DI-8DO (external V3 Modbus relays) and cold spare 8DI-8RO.
   CH1..CH3 roof; CH4..CH8 AUX unless reserved by enabled system function.
   MLX enclosure heater has been removed; PWM heaters remain separate. */

#include <Arduino.h>
#include "astro_config.h"
#include "observatory_identity.h"
#include "relay_map.h"
#include "roof_control.h"
#include "operator_policy.h"
#include "reset_breadcrumb.h"
#include "http_request.h"
#include "http_bounded_client.h"
#if USE_W5500
#include <AstroEthernetGuard.h>
#endif
#include <esp_system.h>    // esp_reset_reason() - diagnostyka przyczyny restartu
#include <string.h>        // memset/strncpy dla TgJob (net-task)
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <esp_adc_cal.h>
#include "wind_monitor.h"
#include "park_safety.h"
#include <freertos/queue.h>
#include <ESP_SSLClient.h>
#include "online_api.h"

// PRO LOG — czarna skrzynka diagnostyczna. Logger nie steruje bezpieczeństwem.
static void eventLog(const char* fmt, ...);
static void eventLogEx(const char* level, const char* category, const char* fmt, ...);
static void eventLogInit();
static void mqttQueueAlert(const String& text);

#ifndef ALPACA_TRUSTED_IPS
#define ALPACA_TRUSTED_IPS ""
#endif
RTC_NOINIT_ATTR static volatile ResetBreadcrumb g_resetBreadcrumb;
static ResetBreadcrumb g_previousBreadcrumb{};
static void diagnosticStage(uint32_t stage,uint32_t endpoint=0) {
  if(!endpoint && stage!=1)endpoint=g_resetBreadcrumb.endpoint;
  const auto record=breadcrumbRecord(stage,millis(),endpoint);
  g_resetBreadcrumb.check=0; // invalidate first; commit checksum last
  g_resetBreadcrumb.magic=record.magic;g_resetBreadcrumb.stage=record.stage;
  g_resetBreadcrumb.at=record.at;g_resetBreadcrumb.endpoint=record.endpoint;
  g_resetBreadcrumb.check=record.check;
}

// zapis edytowalnych progow w NVS (przetrwa restart)

// ── Identyfikacja firmware (zeby nie mylic wersji sterownika) ──────────────
#ifndef FW_VERSION
  #define FW_VERSION "ASTRO-ROOF SAFE | " OBSERVATORY_NAME " | SAFE-RECOVERY-20261105"
#endif

// ============== KONFIG ==============
#ifndef USE_WIFI
  #define USE_WIFI 1
#endif
// plytka ma wbudowany W5500 - domyslnie WLACZONY (kabel = pewniejsze
// polaczenie niz samo WiFi). Nadpisz -D USE_W5500=0 zeby wrocic do WiFi-only.
#ifndef USE_W5500
  #define USE_W5500 1
#endif
#ifndef SERVER_PORT
  #define SERVER_PORT 11111
#endif

// ── DIAG (tymczasowe, do wylaczenia/usuniecia po znalezieniu przyczyny) ──
// DIAG_NET_TEST=1:  jednorazowy test zwykłego (bez TLS) polaczenia TCP:80 po
//                    starcie Ethernetu - sprawdza czy ruch wychodzacy poza TLS
//                    dziala (odsiewa firewall/gateway od problemu specyficznego dla TLS).
// Wylacz oba (= 0) lub usun caly blok #if DIAG_* po zakonczeniu diagnozy.
#ifndef DIAG_TLS_DEBUG
  #define DIAG_TLS_DEBUG 0
#endif
#ifndef DIAG_NET_TEST
  #define DIAG_NET_TEST 0
#endif
// DIAG_ETH_DHCP=1: W5500 prosi o adres przez DHCP zamiast uzywac statycznego IP
// (adres statyczny z konfiguracji W5500). Test hipotezy: niektore routery
// routery wpuszczaja do internetu TYLKO klientow, ktorym sami wydali adres przez
// DHCP - reczne/statyczne IP "widzi" siec lokalna i robi TCP handshake, ale dalej
// nic nie idzie (czyli 1:1 to, co obserwujemy). Domyslnie WYLACZONE (0) - normalny,
// moga wymagac adresu przydzielonego przez DHCP. Wlacz (1) tylko na czas testu.
#ifndef DIAG_ETH_DHCP
  #define DIAG_ETH_DHCP 0
#endif

// ── Haslo UI ──────────────────────────────────────────────
#ifndef UI_USER
  #define UI_USER "admin"
#endif
#ifndef UI_PASS
  #define UI_PASS ""
#endif
static const char* AUTH_USER = UI_USER;
static const char* AUTH_PASS = UI_PASS;

// ── Task WDT ──────────────────────────────────────────────
#ifndef USE_TASK_WDT
  #define USE_TASK_WDT 1   // watchdog domyslnie wlaczony dla systemu safety
#endif
#if USE_TASK_WDT
  #include <esp_task_wdt.h>
  #include <esp_idf_version.h>
  static const int WDT_TIMEOUT_SEC = 30;
#endif

// ── WiFi ──────────────────────────────────────────────────
#if USE_WIFI
  #include <WiFi.h>
  #ifndef WIFI_SSID
    #define WIFI_SSID "YOUR_SSID"
  #endif
  #ifndef WIFI_PASS
    #define WIFI_PASS "YOUR_PASSWORD"
  #endif
  static const char* WIFI_SSID_C = WIFI_SSID;
  static const char* WIFI_PASS_C = WIFI_PASS;
  WiFiServer wifiServer(SERVER_PORT);
  bool g_wifiReady = false;
  unsigned long g_wifiLastConnectAttemptMs = 0;
#endif

// ── W5500 Ethernet ────────────────────────────────────────
#if USE_W5500
  #include <Ethernet.h>
  #include <Dns.h>          // diagnostyka DNS po Ethernecie
  byte macAddr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };   // powrot do MAC, ktory dzis dzialal (test)
  IPAddress ethIP(192, 168, 1, 240);   // statyczny adres Ethernetu
  IPAddress ethGW(192, 168, 1, 1);
  IPAddress ethDNS(8, 8, 8, 8);   // publiczny DNS (router nie odpowiadal na zapytania)
  IPAddress ethSN(255, 255, 255, 0);
  // fabryczne piny wbudowanego W5500 na Waveshare ESP32-S3-POE-ETH-8DI-8DO.
  // NIE UZYWAC GPIO12-16 do niczego innego (rowniez INT=GPIO12, niewykorzystywany
  // programowo przez ta biblioteke, ale zarezerwowany sprzetowo).
  #define ETH_CS_PIN   16
  #define ETH_MOSI_PIN 13
  #define ETH_MISO_PIN 14
  #define ETH_SCLK_PIN 15
  class EthServer : public EthernetServer {
  public:
    EthServer(uint16_t port) : EthernetServer(port) {}
    void begin(uint16_t port = 0) override { EthernetServer::begin(); }
  };
  EthServer ethServer(SERVER_PORT);
  bool g_ethReady = false;
#endif

#include "mqtt_config.h"
#include <lwip/sockets.h>
// Ethernet DNS has an inet_aton method; avoid leaking the lwIP macro.
#ifdef inet_aton
#undef inet_aton
#endif

// ── Telegram ──────────────────────────────────────────────
// Wlacz: -D USE_TELEGRAM=1 w platformio.ini
// -D TELEGRAM_TOKEN=\"123456:ABCDEFGH\" -D TELEGRAM_CHAT_ID=\"-1001234567890\"
#ifndef USE_TELEGRAM
  #define USE_TELEGRAM 0
#endif
#if USE_TELEGRAM
  #include <WiFiClientSecure.h>
  #include <ESP_SSLClient.h>
  #ifndef TELEGRAM_TOKEN
    #define TELEGRAM_TOKEN "YOUR_BOT_TOKEN"
  #endif
  #ifndef TELEGRAM_CHAT_ID
    #define TELEGRAM_CHAT_ID "YOUR_CHAT_ID"
  #endif
  static const char* TG_TOKEN   = TELEGRAM_TOKEN;
  static const char* TG_CHAT_ID = TELEGRAM_CHAT_ID;
  unsigned long g_tgLastSent = 0;
  const unsigned long TG_MIN_INTERVAL_MS = 30000UL; // max 1 alert co 30s
  #ifndef TG_DUPLICATE_SUPPRESS_MS
    #define TG_DUPLICATE_SUPPRESS_MS 300000UL  // ten sam komunikat max raz / 5 min
  #endif
  String g_tgLastMsg = "";
  unsigned long g_tgLastMsgAt = 0;

  // ══════════════════════════════════════════════════════════════════════
  //  WYSYLKA TELEGRAM NA DRUGIM RDZENIU (core 0)
  //  ══════════════════════════════════════════════════════════════════════
  //  PROBLEM ktory to rozwiazuje: telegramSendNow() robi blokujace polaczenie
  //  TLS (do 3s connect + do 4s handshake, ew. x2 przy failover) WPROST w
  //  loop(). Caly ten czas serwer Alpaca (ten sam watek!) nie odpowiada
  //  NINA - i to niezaleznie od tego, o ktore urzadzenie (SafetyMonitor,
  //  ObservingConditions, Dome/Switch) akurat pyta - stad wrazenie "losowej"
  //  utraty polaczenia raz z jednym, raz z drugim urzadzeniem.
  //
  //  ROZWIAZANIE: prawdziwa wysylka TLS przeniesiona na osobny task
  //  FreeRTOS przypiety do rdzenia 0 (loop()/serwer Alpaca zostaja na
  //  rdzeniu 1, tak jak domyslnie robi to szkielet Arduino-ESP32). Kod
  //  wywolujacy telegramSendNow(...) NIC nie musi wiedziec o tej zmianie -
  //  ta sama nazwa/sygnatura funkcji zostaje, tylko teraz to CIENKI
  //  WRAPPER, ktory wklada zlecenie do kolejki i wraca natychmiast (<1ms).
  //  Prawdziwa, blokujaca implementacja zostala przemianowana na
  //  telegramSendNow_blocking() i jest wolana WYLACZNIE z wnetrza netTask().
  //
  //  BEZPIECZENSTWO WSPOLDZIELONYCH DANYCH: jedyne zmienne globalne pisane
  //  przez telegramSendNow_blocking() i czytane przez glowny watek to
  //  g_tgLastSent / g_tgLastMsg / g_tgLastMsgAt (anti-spam). g_tgLastMsg to
  //  String - kopiowanie/porownywanie Stringa jednoczesnie z dwoch rdzeni
  //  BEZ ochrony to realne ryzyko crasha (nie tylko teoretyczne), wiec te
  //  trzy zmienne sa chronione osobnym mutexem (g_tgStateMutex).
  //
  //  UWAGA DLA LUKASZA: to zmiana architektury, ktorej NIE moglem
  //  skompilowac na prawdziwym ESP32 (brak tu tego toolchaina) - skladnia
  //  API FreeRTOS jest sprawdzona/standardowa, ale przed zaufaniem temu bez
  //  nadzoru przez cala noc, przepatrz Serial po pierwszym wgraniu: powinienes
  //  zobaczyc "[NET-TASK] Start na rdzeniu 0" zaraz po boot, i normalne
  //  "[TG] ..." logi przy pierwszym alercie - identyczne jak wczesniej,
  //  tylko ze NINA nie powinna juz przy tym gubic polaczenia.
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/queue.h>
  #include <freertos/semphr.h>

  #ifndef NET_QUEUE_LEN
    #define NET_QUEUE_LEN 6
  #endif
  #ifndef NET_TASK_STACK
    #define NET_TASK_STACK 10240   // TLS/BearSSL bywa zachlanne na stos - z zapasem
  #endif

  struct TgJob {
    char text[220];
    bool primaryOnly;
  };

  static QueueHandle_t     g_netQueue     = nullptr;
  static SemaphoreHandle_t g_tgStateMutex = nullptr;

  static void telegramSendNow_blocking(const String& msg, bool primaryOnly);

  // Wolane WYLACZNIE z wnetrza netTask() (rdzen 0) - petla konsumujaca kolejke.
  static void netTaskFn(void* pv) {
    Serial.println(F("[NET-TASK] Start na rdzeniu 0"));
    TgJob job;
    for (;;) {
      if (xQueueReceive(g_netQueue, &job, portMAX_DELAY) == pdTRUE) {
        if(!onlineTelegramEnabled())continue;
        if(g_notifyTransportMutex)xSemaphoreTake(g_notifyTransportMutex,portMAX_DELAY);
        if(onlineTelegramEnabled())telegramSendNow_blocking(String(job.text), job.primaryOnly);
        if(g_notifyTransportMutex)xSemaphoreGive(g_notifyTransportMutex);
      }
    }
  }

  // Uruchom raz z setup() - PRZED jakakolwiek mozliwoscia wyslania alertu.
  static void netTaskInit() {
    g_tgStateMutex = xSemaphoreCreateMutex();
    g_netQueue     = xQueueCreate(NET_QUEUE_LEN, sizeof(TgJob));
    if (!g_tgStateMutex || !g_netQueue) {
      Serial.println(F("[NET-TASK] BLAD: nie udalo sie utworzyc mutexu/kolejki!"));
      return;
    }
    xTaskCreatePinnedToCore(netTaskFn, "netTask", NET_TASK_STACK, nullptr, 1, nullptr, 0);
  }

  static void telegramSend(const String& msg, bool force = false);
#endif

// ── Park Guard ────────────────────────────────────────────
// Sprzet: reed/microswitch RA=GPIO9 (DI6), DEC=GPIO10 (DI7); LOW = w parku.
// Software: opcjonalne sprawdzenie przez Alpaca HTTP
// Wlacz: -D PARK_GUARD_ENABLED=1 (domyslnie 0 - nie wymaga sprzetu)
#ifndef PARK_GUARD_ENABLED
  #define PARK_GUARD_ENABLED 0
#endif
// FIX BUG7: #ifndef pozwala nadpisac pin z platformio.ini (-D PARK_SENSOR_PIN=xx)
// reed osi RA -> DI6 / GPIO9 (izolowane wejscie fabryczne Waveshare).
#ifndef PARK_SENSOR_PIN
  #define PARK_SENSOR_PIN 9   // DI6, INPUT_PULLUP, GND = w parku
#endif
// DRUGI reed - os DEC. Park = OBIE osie w parku (oba piny LOW jednoczesnie).
// reed osi DEC -> DI7 / GPIO10.
#ifndef PARK_SENSOR_DEC_PIN
  #define PARK_SENSOR_DEC_PIN 10   // DI7
#endif

// Alpaca mount endpoint do sprawdzenia park (opcjonalne, "" = wylaczone)
// Przyklad: -D PARK_ALPACA_HOST=\"192.168.1.100\" -D PARK_ALPACA_PORT=11111
#ifndef PARK_ALPACA_HOST
  #define PARK_ALPACA_HOST ""
#endif
#ifndef PARK_ALPACA_PORT
  #define PARK_ALPACA_PORT 11111
#endif

// Czas blokady po ruchu teleskopu (ms) - jesli mount sie ruszal, czekaj az sie zatrzyma
#ifndef PARK_SETTLE_MS
  #define PARK_SETTLE_MS 5000UL
#endif

bool g_parkSensorOK    = false;  // czy pin GPIO wykryty (zwarty do GND kiedykolwiek)
bool g_telescopeParked = false;  // aktualny stan (false = unsafe do zamkniecia)
bool g_parkRa          = false;  // os RA w parku (reed GPIO9 LOW)
bool g_parkDec         = false;  // os DEC w parku (reed GPIO10 LOW)
unsigned long g_lastParkCheck = 0;
const unsigned long PARK_CHECK_INTERVAL_MS = 5000UL;

// ── Stan parkowania (rozszerzony dla AP900) ──────────────────────────────
// Poprzedni stan - do wykrywania momentu zaparkowania
bool   g_wasParkedLastCycle  = false;
// Czas kiedy teleskop ostatnio zaparkował (millis)
unsigned long g_lastParkedAtMs = 0;
// Czy pokazujemy animację parkowania w UI
bool g_showParkAnimation = false;
const unsigned long PARK_ANIM_DURATION_MS = 8000UL; // 8s animacji po parkowaniu
// Blokada otwarcia dachu gdy montaz nie w parku (OPEN guard)
// Niezalezna od PARK_GUARD_ENABLED - zawsze aktywna gdy mamy info o parku
bool g_openGuardActive = false;
// Licznik nieudanych prob polaczenia Alpaca (zeby nie floodowac logiem)
uint8_t g_alpacaFailCount = 0;
// Read-only diagnostics. These fields do not participate in roof interlocks.
bool g_parkAlpacaAttempted=false, g_parkAlpacaSuccessSeen=false;
bool g_parkAlpacaLastAttemptOK=false, g_parkAlpacaLastValue=false;
uint32_t g_parkAlpacaAttemptMs=0, g_parkAlpacaSuccessMs=0;
constexpr uint32_t PARK_ALPACA_DIAG_FRESH_MS=15000;
const uint8_t ALPACA_MAX_FAILS = 3; // po 3 blędach - ostrzezenie

// ═══ PINY — WARIANT: Waveshare ESP32-S3-POE-ETH-8DI-8DO + Waveshare Modbus RTU Relay 17658 ═══
// I2C: fabryczna magistrala plytki (SCL=GPIO41, SDA=GPIO42). Na niej wisza:
// ═══ WARIANT SPRZETOWY: RS485 Modbus RTU -> ZEWN. Waveshare 8CH Relay 17658 ═══
// RS485 GPIO17/18/21 steruje zewnetrznym modulem Waveshare 8CH Modbus Relay 17658.
// TCA9554 0x20: DO1..DO7 pozostaja lokalne; DO8 steruje oswietleniem. Dach obslugiwany przez zewnetrzny Modbus 17658.
// pomocniczych (kolizja/grzalka/montaz).
// grzalka/montaz, zewnetrzny), MLX90614 0x5A/0x5B, BH1750 0x23, SI1145 0x60,
// BME280, TSL2591, RTC.
#define I2C_SDA_PIN  42
#define I2C_SCL_PIN  41
// GPIO2/3 sa zajete przez CAN. GPIO45/47/48 sa polaczone z gniazdem TF;
// GPIO45 jest tez pinem strapujacym i pozostaje zarezerwowany.
// W tym projekcie gniazdo TF nie jest uzywane, wiec GPIO47/48 sa
// przypisane do opcjonalnych czujnikow. Nie wkladaj karty TF, gdy czujniki
// sa podlaczone. Funkcje czujnikow nadal sa domyslnie WYLACZONE.
#ifndef DHT_PIN
  #define DHT_PIN      47
#endif
#ifndef DS18B20_PIN
  #define DS18B20_PIN  48
#endif
// RG11 (deszcz): DI8 / GPIO11 - fabryczne izolowane wejscie cyfrowe.
#define RG11_PIN     11
// WIND: GPIO1 (ADC1_CH0) - wolny pin, kandydat do anemometru wg sciagi plytki.
// Nadpisywalny: -D WIND_PIN=xx w secrets.ini
#ifndef WIND_PIN
  #define WIND_PIN   1
#endif
// Krancowki dachu: DI1=CLOSED/GPIO4, DI2=OPEN/GPIO5 - fabryczne izolowane DI.
#define LIMIT_CLOSED_PIN    4
#define LIMIT_OPEN_PIN      5
// Emergency close: DI4 / GPIO7 - fabryczne izolowane DI.
#define EMERGENCY_CLOSE_PIN 7

// ════════════════════════════════════════════════════════════════════
// niezalezne powiadomienia - SMS (SIM900) + lokalna syrena.
// Piny SIM900/syreny opisano w obs_notify.h (GPIO47=linia TF przypisana tu do
// DHT22, GPIO38=RGB, GPIO13=W5500 MOSI). RS485 jest zarezerwowany dla Modbus.
// SIM900 UART jest na GPIO43/44; PWRKEY pozostaje niepodlaczony,
// syrena=GPIO46 (dzielona z fabrycznym buzzerem). Szczegoly w obs_notify.h.
// Numer SMS i piny ustawisz w pliku obs_notify.h.
#include "obs_notify.h"
// budzenie komputera w obserwatorium (Wake-on-LAN, niezalezne od RPi)
#include "obs_wakepc.h"
// ════════════════════════════════════════════════════════════════════
// Czujniki parku: RA = GPIO9 / DI6, DEC = GPIO10 / DI7.

// ── czujnik kolizji / przeszkody dachu ────────────────────────
// Obslugiwany typ: czujnik laserowy NPN NC (Normally Closed).
// Okablowanie NPN NC:
//   - Brak przeszkody: tranzystor NPN przewodzi -> wyjscie LOW (GND)
//   - Przeszkoda LUB brak zasilania: tranzystor nie przewodzi -> wyjscie HIGH (floating)
//   - Pin ESP32: INPUT_PULLUP, ACTIVE_LOW=0 (HIGH = przeszkoda/brak zasil.)
//   - Dzieki temu utrata zasilania czujnika = stan bezpieczny (nie blokuje dachu po powrocie)
// Domyslnie wylaczone, zeby nie zepsuc istniejacego okablowania.
// Wlacz przez: -D COLLISION_SENSOR_ENABLED=1
// DI5 / GPIO8 - fabryczne izolowane wejscie cyfrowe.
// ACTIVE_LOW=0: stan HIGH na pinie = PRZESZKODA (logika NPN NC).
#ifndef COLLISION_SENSOR_ENABLED
  #define COLLISION_SENSOR_ENABLED 0
#endif
#ifndef COLLISION_SENSOR_PIN
  #define COLLISION_SENSOR_PIN 8
#endif
#ifndef COLLISION_SENSOR_ACTIVE_LOW
  #define COLLISION_SENSOR_ACTIVE_LOW 0   // NPN NC: HIGH = przeszkoda / brak zasilania
#endif
#ifndef COLLISION_DEBOUNCE_MS
  #define COLLISION_DEBOUNCE_MS 80UL
#endif

// ── Przekaznik zasilania czujnika kolizji (dedykowany kanal AUX) ─────────
// Czujnik dostaje zasilanie TYLKO gdy dach sie porusza.
// Gdy dach stoi (OPEN/CLOSED/STOPPED/ERROR) -> zasilanie odciete -> laser nie swieci.
// Dzieki temu czujnik NPN NC nie przeszkadza w akwizycji astrofoto.
// WAZNE: dla NPN NC brak zasilania = wyjscie HIGH = "brak przeszkody" -> bezpieczne.
// Dedykowany kanal CH4 Waveshare 17658.
// Wlacz przez: -D COLLISION_POWER_RELAY_ENABLED=1
#ifndef COLLISION_POWER_RELAY_ENABLED
  #define COLLISION_POWER_RELAY_ENABLED 0
#endif
#ifndef COLLISION_POWER_RELAY_CH
  // kodzie pozostaje 0..10; relayWrite() kieruje ch<=7 do Waveshare 17658 przez Modbus,
  #define COLLISION_POWER_RELAY_CH  3
#endif
#ifndef COLLISION_POWER_WARMUP_MS
  #define COLLISION_POWER_WARMUP_MS 600UL  // czas inicjalizacji NPN po wlaczeniu zasilania [ms]
#endif

// Firmware nie udostepnia zdalnego ignorowania Park Guard, niezaleznie od starej flagi.
// To chroni teleskop i montaz przed zmiazdzeniem przez dach.
// Zmien na 1 tylko jesli masz fizyczny margines/wyciete okno/czujniki antykolizyjne
// i akceptujesz ryzyko kolizji.
#ifndef FORCE_CLOSE_ALLOW_IGNORE_PARK
  #define FORCE_CLOSE_ALLOW_IGNORE_PARK 0
#endif

// ════════════════════════════════════════════════════════════════════════
// KRANCOWKA OVERTRAVEL MONTAZU (MOUNT E-STOP)
// ════════════════════════════════════════════════════════════════════════
// Cel: gdy montaz (AP900) wyjedzie poza bezpieczna obwiednie - np. przez zle
// limity w NINA/APCC albo zla konfiguracje samego AP900 - krancowka MA GO
// ZATRZYMAC, zanim cokolwiek uderzy w sciane/dach/pier.
//
// ZASADA NADRZEDNA (przeczytaj zanim podlaczysz):
//   Krancowke overtravel WEPNIJ SPRZETOWO tak, by FIZYCZNIE przerywala ruch
//   montazu NIEZALEZNIE od ESP - lancuch stykow NC w szereg ze stykiem stycznika
//   zasilania montazu lub z wejsciem STOP/e-stop AP900. Wtedy zla konfiguracja
//   NINA/AP900, padnieta siec i zawieszony ESP - zadne z nich nie pozwoli
//   montazowi pojechac dalej, bo zatrzymuje go sama mechanika.
//   ESP w tym ukladzie tylko OBSERWUJE trip, zatrzaskuje E-STOP, BLOKUJE dach
//   i KRZYCZY (SMS/syrena/Telegram). To jest wlasciwa kolejnosc zaufania.
//
// Okablowanie wejscia ESP (fail-safe NC, ZALECANE):
//   - Lancuch stykow NC (rozwierny) miedzy MOUNT_LIMIT_PIN a GND.
//   - INPUT_PULLUP. Montaz w obwiedni = wszystkie styki zwarte = pin LOW.
//   - Trip (styk rozwarty) LUB przerwany kabel = pin HIGH = TRIP (bezpiecznie!).
//   - Czyli przerwanie przewodu tez wyzwala E-STOP - to celowe.
// Domyslnie WYLACZONE, zeby nie zmienic istniejacego ukladu. Wlacz: -D MOUNT_LIMIT_ENABLED=1
#ifndef MOUNT_LIMIT_ENABLED
  #define MOUNT_LIMIT_ENABLED 0
#endif
#ifndef MOUNT_LIMIT_PIN
  #define MOUNT_LIMIT_PIN 6          // DI3 / GPIO6 - fabryczne izolowane DI. Zmien: -D MOUNT_LIMIT_PIN=xx
#endif
// MOUNT_LIMIT_TRIP_WHEN_HIGH=1 (NC fail-safe): HIGH na pinie = trip/przerwany kabel.
// =0 (NO): LOW na pinie = trip (styk zwiera do GND gdy uderzy). NC jest bezpieczniejszy.
#ifndef MOUNT_LIMIT_TRIP_WHEN_HIGH
  #define MOUNT_LIMIT_TRIP_WHEN_HIGH 1
#endif
#ifndef MOUNT_LIMIT_DEBOUNCE_MS
  #define MOUNT_LIMIT_DEBOUNCE_MS 40UL
#endif
// Ochrona montazu blokuje OPEN. CLOSE: bez potwierdzonego przeswitu blokuje zawsze;
// z przeswitem blokuje tylko przy MOUNT_ESTOP_BLOCKS_CLOSE=1. To nie jest STOP dla ludzi.
#ifndef MOUNT_ESTOP_BLOCKS_CLOSE
  #define MOUNT_ESTOP_BLOCKS_CLOSE 0
#endif
// OPCJONALNY przekaznik ODCINAJACY zasilanie montazu na czas E-STOP (teeth).
// Semantyka: relay ON = zasilanie montazu ODCIETE. Stan spoczynkowy (OFF) = montaz
// zasilany. Restart zachowuje zapisany trip: nie przywraca sam zasilania po ochronie.
// To jest tylko "smart" warstwa; prawdziwe odciecie zrob sprzetowo (patrz wyzej).
#ifndef MOUNT_POWER_RELAY_ENABLED
  #define MOUNT_POWER_RELAY_ENABLED 0
#endif
#ifndef MOUNT_POWER_RELAY_CH
  #define MOUNT_POWER_RELAY_CH 4     // CH5 Waveshare 17658.
#endif

// ════════════════════════════════════════════════════════════════════════
// WATCHDOG LACZNOSCI Z NINA (utrata komunikacji = ESP odciety)
// ════════════════════════════════════════════════════════════════════════
// Gdy padnie WiFi/W5500 albo PC z NINA, ESP przestaje dostawac zapytania Alpaca.
// Sam ESP nadal chroni dach od POGODY (czujniki lokalne) - ale nikt zdalnie nie
// zamknie/zaparkuje, a krzyzowa weryfikacja parku przez Alpaca przestaje dzialac.
// Dlatego: jesli przy OTWARTYM dachu brak kontaktu Alpaca > timeout -> ALARM
// kanalem NIEZALEZNYM od LAN (SMS SIM900 + syrena - to siec komorkowa, nie WiFi).
#ifndef COMMS_WATCHDOG_ENABLED
  #define COMMS_WATCHDOG_ENABLED 1
#endif
#ifndef COMMS_LOST_TIMEOUT_MS
  #define COMMS_LOST_TIMEOUT_MS 300000UL   // 5 min bez Alpaca przy otwartym dachu
#endif
// COMMS_LOST_AUTOPARK=1: po utracie laczności traktuj jako UNSAFE -> uruchom
// istniejace auto-zamykanie (ktore i tak respektuje Park Guard). Bezpieczne
// auto-domkniecie wymaga, by ESP znal stan parku - najlepiej z reed switcha GPIO
// (PARK_SENSOR_PIN), bo Alpaca tez bywa wtedy nieosiagalna. Domyslnie 0 (tylko alarm).
#ifndef COMMS_LOST_AUTOPARK
  #define COMMS_LOST_AUTOPARK 0
#endif
// Brak JAKIEGOKOLWIEK lacza (WiFi i ETH) przez ten czas -> krzyk po SMS/syrenie.
#ifndef LINK_LOST_ALERT_MS
  #define LINK_LOST_ALERT_MS 120000UL      // 2 min
#endif

// ════════════════════════════════════════════════════════════════════════
// BEZPIECZNE OTA (aktualizacja firmware przez siec)
// ════════════════════════════════════════════════════════════════════════
// OTA dziala po WiFi (ArduinoOTA/mDNS). Zabezpieczenia:
//   - WYMAGANE haslo (puste haslo = OTA NIE startuje, ostrzezenie na Serial),
//   - OTA odmawia startu gdy dach OPENING/CLOSING albo gdy aktywny E-STOP montazu,
//   - na czas wgrywania: relaye dachu OFF, watchdog karmiony, logi postepu.
// Wlacz: -D USE_OTA=1 i -D OTA_PASSWORD=\"twoje_haslo\" w platformio.ini.
#ifndef USE_OTA
  #define USE_OTA 0
#endif
// Hostname is defined centrally in observatory.ini.
#ifndef OTA_PASSWORD
  #define OTA_PASSWORD ""
#endif
#if USE_OTA && USE_WIFI
  #include <ArduinoOTA.h>
  bool g_otaReady   = false;   // OTA wystartowalo (haslo ustawione, WiFi gotowe)
  bool g_otaActive  = false;   // trwa wgrywanie (blokuje ruch dachu)
#endif


// ============== OPCJONALNE CZUJNIKI ==============
// Domyslna konfiguracja dla tej plytki: tylko czujniki majace sens dla
// sterownika dachu pozostaja aktywne. Czujniki OFF nie sa inicjalizowane,
// nie sa odczytywane i NIE pojawiaja sie w WWW/Alpaca.
// Mozna je wlaczyc przez build_flags_sensors w secrets.ini, jesli zostana
// fizycznie podlaczone zgodnie z mapa GPIO.
#ifndef ENABLE_DHT22
  #define ENABLE_DHT22 0
#endif
#ifndef ENABLE_DS18B20
  #define ENABLE_DS18B20 0
#endif
#ifndef ENABLE_BH1750
  #define ENABLE_BH1750 0
#endif
#ifndef ENABLE_TSL2591
  #define ENABLE_TSL2591 0
#endif
#ifndef ENABLE_SI1145
  #define ENABLE_SI1145 0
#endif
#ifndef ENABLE_BMP280
  #define ENABLE_BMP280 0
#endif
#ifndef ENABLE_BME280
  #define ENABLE_BME280 1
#endif

// ============== Czujniki / biblioteki ==============
#if ENABLE_BH1750
#include <BH1750.h>
#endif
#if ENABLE_TSL2591
#include <Adafruit_TSL2591.h>   // SQM - jakosc nieba (szeroki zakres, do 0.0002-88000 lx)
#endif
#include <Adafruit_MLX90614.h>
#if ENABLE_BMP280
#include <Adafruit_BMP280.h>
#endif
#if ENABLE_BME280
#include <Adafruit_BME280.h>   // temperatura + wilgotnosc + cisnienie
#endif
#if ENABLE_DHT22
#include <DHT.h>
#endif
#if ENABLE_SI1145
#include <Adafruit_SI1145.h>
#endif
#if ENABLE_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#if ENABLE_BH1750
BH1750 bh1750;
#endif
bool bhOK = false;
float g_skyLux = NAN;

// TSL2591 - drugi, znacznie czulszy czujnik swiatla (SQM). Niezalezny
// od BH1750 - nie zastepuje istniejacego pomiaru "SkyLux", tylko go uzupelnia
// wlasnym, dedykowanym panelem w UI (patrz karta "Jakosc nieba" w HTML nizej).
#if ENABLE_TSL2591
Adafruit_TSL2591 tsl591 = Adafruit_TSL2591(2591);
#endif
bool tslOK = false;
float g_tslLux  = NAN;      // wyliczony lux (biblioteka Adafruit, z uwzglednieniem gain/czasu calkowania)
uint16_t g_tslFull = 0;     // surowy kanal pelnego spektrum (widzialne + IR)
uint16_t g_tslIR   = 0;     // surowy kanal samej podczerwieni
uint8_t  g_tslGainIdx = 1;  // 0=LOW(1x) 1=MED(25x) 2=HIGH(428x) 3=MAX(9876x) - auto-dobor w loop()

Adafruit_MLX90614 mlx;
bool mlxOK = false;

// DRUGI MLX90614 (nowa glowica z TSL2591, adres 0x5B po przeadresowaniu
// - patrz mlx90614_zmiana_adresu.ino).
// DRUGI CZUJNIK JEST REALNYM FAILOVEREM BEZPIECZENSTWA, nie
// tylko danymi/logiem. MLX1 (0x5A) jest domyslny; przy bledzie aktywnego
// pomiaru i swiezej parze zapasu firmware przelacza logike chmur/bezpieczenstwa
// (g_cloudDelta, setUnsafe) na MLX2. Powrot na MLX1 po jego odzyskaniu jest
// RECZNY (przycisk w WWW) - celowo, zeby nie oscylowac miedzy czujnikami
// przy niestabilnym MLX1. Patrz blok "MLX90614 FAILOVER" w petli czujnikow.
#define MLX2_I2C_ADDR 0x5B
Adafruit_MLX90614 mlx2;
bool mlx2OK = false;
float g_tempSky2 = NAN;   // T nieba z DRUGIEGO czujnika (dane/log, lub AKTYWNE zrodlo gdy failover wlaczony)
float g_tempAmb2 = NAN;   // T otoczenia z DRUGIEGO czujnika
astro_roof::MlxSample g_mlxSamples[2];
bool g_useMlx2AsFailover = false;  // ktory MLX aktualnie zasila bezpieczenstwo (false=MLX1, true=MLX2)

#if ENABLE_BMP280
Adafruit_BMP280 bmp;
#endif
bool bmpOK = false;

#if ENABLE_BME280
Adafruit_BME280 bme280;
#endif
bool bme280OK = false;

enum BaroType { BARO_NONE, BARO_BMP280, BARO_BME280 };   // BARO_BMP180 -> BARO_BME280
BaroType g_baroType = BARO_NONE;

#if ENABLE_DHT22
DHT dht(DHT_PIN, DHT22);
#endif
bool dhtOK = false;

#if ENABLE_SI1145
Adafruit_SI1145 si1145;
#endif
bool uvOK = false;
float g_uvIndex = NAN;

#if ENABLE_DS18B20
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
#endif
bool dsOK = false;
float g_tExt = NAN;
#include "focuser_temperature_bridge.h"

// ═══ WARIANT SPRZETOWY: ESP32-S3-POE-ETH-8DI-8DO + WAVESHARE 17658 MODBUS ═══
// TCA9554PWR: DO1..DO7 pozostaja dostepne lokalnie, DO8 steruje swiatlem.
// Zewnetrzny Waveshare 17658 steruje dachem/AUX przez izolowany RS485/Modbus.
// Rejestry zgodne z PCA9554/TCA9554 (minimalny wlasny sterownik na Wire).
// Rejestry: 0x00 Input, 0x01 Output, 0x02 Polarity, 0x03 Configuration
// (bit=1 -> wejscie, bit=0 -> wyjscie).
static bool g_mountPowerRestoreBlocked=true; // loaded before any output initialization
#include "hardware_io.h"
#include "local_do.h"

#ifndef OBS_TCA_I2C_ADDR
  #define OBS_TCA_I2C_ADDR 0x20   // TCA9554 wbudowany na plytce Waveshare
#endif
uint8_t g_tcaAddr = (uint8_t)(OBS_TCA_I2C_ADDR);
#ifndef RTC_PCF85063_ADDR
  #define RTC_PCF85063_ADDR 0x51
#endif
static bool g_rtcPresent = false;
static bool g_rtcTimeValid = false;
static bool g_rtcTimeLost = true;
static uint32_t g_rtcLastSyncMs = 0;
static time_t g_rtcLastSyncEpoch = 0;
static uint32_t g_rtcLastTickMs = 0;

static uint8_t rtcBcdToDec(uint8_t value) {
  return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}
static uint8_t rtcDecToBcd(uint8_t value) {
  return (uint8_t)(((value / 10) << 4) | (value % 10));
}
static bool rtcBcdValid(uint8_t value) {
  return ((value & 0x0F) <= 9) && (((value >> 4) & 0x0F) <= 9);
}
static int64_t rtcDaysSinceEpoch(int year, unsigned month, unsigned day) {
  year -= (month <= 2);
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const int shiftedMonth = (int)month + (month > 2 ? -3 : 9);
  const unsigned doy = (153U * (unsigned)shiftedMonth + 2U) / 5U + day - 1U;
  const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
  return (int64_t)era * 146097LL + (int64_t)doe - 719468LL;
}
static bool pcf85063ReadControl1(uint8_t& value) {
  Wire.beginTransmission(RTC_PCF85063_ADDR);
  Wire.write((uint8_t)0x00); // Control_1, includes 12/24-hour mode
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_PCF85063_ADDR, (uint8_t)1) != 1) return false;
  value = (uint8_t)Wire.read();
  return true;
}
static bool pcf85063ReadUtc(time_t& epoch) {
  uint8_t control1 = 0;
  if (!pcf85063ReadControl1(control1)) return false;
  const bool mode12Hour = (control1 & 0x04) != 0;
  Wire.beginTransmission(RTC_PCF85063_ADDR);
  Wire.write((uint8_t)0x04); // seconds through years
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_PCF85063_ADDR, (size_t)7) != 7) return false;
  uint8_t r[7];
  for (uint8_t i = 0; i < 7; ++i) r[i] = (uint8_t)Wire.read();

  const uint8_t sec = (uint8_t)(r[0] & 0x7F);
  g_rtcTimeLost = (r[0] & 0x80) != 0; // voltage-low flag
  const uint8_t min = (uint8_t)(r[1] & 0x7F);
  const uint8_t hour = (uint8_t)(r[2] & (mode12Hour ? 0x1F : 0x3F));
  const uint8_t day = (uint8_t)(r[3] & 0x3F);
  const uint8_t month = (uint8_t)(r[5] & 0x1F);
  const uint8_t year = r[6];
  if (!rtcBcdValid(sec) || !rtcBcdValid(min) || !rtcBcdValid(hour) ||
      !rtcBcdValid(day) || !rtcBcdValid(month) || !rtcBcdValid(year)) return false;

  const int y = 2000 + rtcBcdToDec(year);
  const unsigned mo = rtcBcdToDec(month);
  const unsigned d = rtcBcdToDec(day);
  unsigned hh = rtcBcdToDec(hour);
  const unsigned mm = rtcBcdToDec(min);
  const unsigned ss = rtcBcdToDec(sec);
  if (mode12Hour) {
    if (hh < 1 || hh > 12) return false;
    hh = (hh % 12) + ((r[2] & 0x20) ? 12 : 0); // convert AM/PM to 24 h
  }
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
      hh > 23 || mm > 59 || ss > 59 || g_rtcTimeLost) return false;

  epoch = (time_t)(rtcDaysSinceEpoch(y, mo, d) * 86400LL +
                   (int64_t)hh * 3600LL + (int64_t)mm * 60LL + (int64_t)ss);
  return epoch > 1700000000;
}
static bool pcf85063WriteUtc(time_t epoch) {
  struct tm utc = {};
  if (!gmtime_r(&epoch, &utc)) return false;
  const int year = utc.tm_year + 1900;
  if (year < 2000 || year > 2099) return false;
  // Keep the RTC in 24-hour mode; preserve its other Control_1 settings.
  uint8_t control1 = 0;
  if (!pcf85063ReadControl1(control1)) return false;
  if (control1 & 0x04) {
    Wire.beginTransmission(RTC_PCF85063_ADDR);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)(control1 & (uint8_t)~0x04));
    if (Wire.endTransmission() != 0) return false;
  }
  const uint8_t r[7] = {
    rtcDecToBcd((uint8_t)utc.tm_sec),
    rtcDecToBcd((uint8_t)utc.tm_min),
    rtcDecToBcd((uint8_t)utc.tm_hour),
    rtcDecToBcd((uint8_t)utc.tm_mday),
    (uint8_t)utc.tm_wday,
    rtcDecToBcd((uint8_t)(utc.tm_mon + 1)),
    rtcDecToBcd((uint8_t)(year - 2000))
  };
  Wire.beginTransmission(RTC_PCF85063_ADDR);
  Wire.write((uint8_t)0x04);
  Wire.write(r, sizeof(r));
  return Wire.endTransmission() == 0;
}
static void rtcBegin() {
  Wire.beginTransmission(RTC_PCF85063_ADDR);
  g_rtcPresent = (Wire.endTransmission() == 0);
  if (!g_rtcPresent) {
    Serial.printf("[RTC] Brak ukladu PCF85063 @0x%02X\n", RTC_PCF85063_ADDR);
    eventLogEx("WARN", "RTC", "RTC offline @0x%02X", RTC_PCF85063_ADDR);
    return;
  }

  time_t epoch = 0;
  if (pcf85063ReadUtc(epoch)) {
    struct timeval tv = {};
    tv.tv_sec = epoch;
    if (settimeofday(&tv, nullptr) == 0) {
      g_rtcTimeValid = true;
      g_rtcTimeLost = false;
      g_rtcLastSyncEpoch = epoch;
      g_rtcLastSyncMs = millis();
      Serial.println(F("[RTC] Czas odczytany z PCF85063 (UTC)"));
      eventLogEx("INFO", "RTC", "Czas odczytany z RTC");
      return;
    }
  }
  g_rtcTimeValid = false;
  Serial.printf("[RTC] Uklad wykryty @0x%02X, czas wymaga synchronizacji%s\n",
                RTC_PCF85063_ADDR, g_rtcTimeLost ? " (flaga utraty zasilania)" : "");
  eventLogEx("WARN", "RTC", "Uklad wykryty, czas wymaga synchronizacji");
}
static void rtcNtpStart() {
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); // Europe/Warsaw
  tzset();
#if USE_WIFI
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  Serial.println(F("[NTP] Synchronizacja czasu wlaczona (strefa Europe/Warsaw)"));
#endif
}
static void rtcTick() {
  const uint32_t nowMs = millis();
  if (nowMs - g_rtcLastTickMs < 5000UL) return;
  g_rtcLastTickMs = nowMs;
  if (!g_rtcPresent) return;

  const time_t now = time(nullptr);
  if (now <= 1700000000) return;
  bool syncNeeded = !g_rtcTimeValid;
  if (g_rtcTimeValid) {
    const int64_t expected = (int64_t)g_rtcLastSyncEpoch +
                             (int64_t)((nowMs - g_rtcLastSyncMs) / 1000UL);
    const int64_t difference = (int64_t)now - expected;
    if (difference > 5 || difference < -5 ||
        nowMs - g_rtcLastSyncMs >= 21600000UL) syncNeeded = true; // 6 h
  }
  if (syncNeeded && pcf85063WriteUtc(now)) {
    g_rtcTimeValid = true;
    g_rtcTimeLost = false;
    g_rtcLastSyncEpoch = now;
    g_rtcLastSyncMs = nowMs;
    Serial.println(F("[RTC] Zegar PCF85063 zsynchronizowany z czasem systemowym"));
    eventLogEx("INFO", "RTC", "RTC zsynchronizowany z NTP/czasem systemowym");
  }
}
static String rtcClockText() {
  const time_t now = time(nullptr);
  if (now <= 1700000000) return "";
  struct tm local = {};
  if (!localtime_r(&now, &local)) return "";
  char text[24];
  if (strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local) == 0) return "";
  return String(text);
}

// ══════════════════════════════════════════════════════════════════════
// WARIANT SPRZETOWY: ESP32-S3-POE-ETH-8DI-8DO + WAVESHARE 17658 MODBUS
// Kanaly dachu/AUX sa sterowane przez Modbus RTU.
// RELAY_ACTIVE_LOW_CFG nie odwroci polaryzacji modułu Waveshare 17658.
// ══════════════════════════════════════════════════════════════════════
#ifndef RELAY_LOG
  #define RELAY_LOG 0
#endif

#ifndef RELAY_ROOF_OPEN_CH
  #define RELAY_ROOF_OPEN_CH  0
#endif
#ifndef RELAY_ROOF_CLOSE_CH
  #define RELAY_ROOF_CLOSE_CH 1
#endif
#ifndef RELAY_ROOF_STOP_CH
  #define RELAY_ROOF_STOP_CH  2
#endif
const uint8_t RELAY_ROOF_OPEN = RELAY_ROOF_OPEN_CH;
const uint8_t RELAY_ROOF_CLOSE = RELAY_ROOF_CLOSE_CH;
const uint8_t RELAY_ROOF_STOP = RELAY_ROOF_STOP_CH;
static_assert(RELAY_ROOF_OPEN_CH == 0 && RELAY_ROOF_CLOSE_CH == 1 && RELAY_ROOF_STOP_CH == 2, "Roof must occupy CH1..CH3");
static_assert(COLLISION_POWER_RELAY_CH == 3 && MOUNT_POWER_RELAY_CH == 4, "System power relays must occupy CH4/CH5");

bool relayState[16] = {}; // logiczny stan zadany przez program
unsigned long pulseEndTime[16] = {};
bool pulseActive[16] = {};

static bool modbusRelayPollNow() {
  uint8_t states[8] = {};
  g_lastModbusRelayPoll = millis();
  if (!modbusRelayReadAllStates(states)) {
    modbusRelayStateValid = false; // ostatnie wartości zachowujemy, ale nie pokazujemy jako aktualne
    const uint32_t now = millis();
    if (g_lastModbusPollErrorLogMs == 0 ||
        (uint32_t)(now - g_lastModbusPollErrorLogMs) >= 5000UL) {
      g_lastModbusPollErrorLogMs = now;
      Serial.println(F("[MODBUS STATE] FC01 read error/CRC; ACT=UNKNOWN"));
    }
    return false;
  }

  uint8_t actualMask = 0;
  uint8_t commandedMask = 0;
  const bool wasValid = modbusRelayStateValid;
  for (uint8_t ch = 0; ch < 8; ++ch) {
    if (states[ch]) actualMask |= (uint8_t)(1U << ch);
    if (relayState[ch]) commandedMask |= (uint8_t)(1U << ch);
  }

  uint8_t previousActualMask = 0;
  for (uint8_t ch = 0; ch < 8; ++ch)
    if (modbusRelayActualState[ch]) previousActualMask |= (uint8_t)(1U << ch);

  modbusRelayStateValid = true;
  for (uint8_t ch = 0; ch < 8; ++ch)
    modbusRelayActualState[ch] = states[ch];

  if (!wasValid || actualMask != previousActualMask) {
    Serial.print(F("[MODBUS STATE] "));
    for (uint8_t ch = 0; ch < 8; ++ch) {
      if (ch) Serial.print(' ');
      Serial.printf("CH%u=%s", ch + 1, states[ch] ? "ON" : "OFF");
    }
    Serial.println();
  }

  static uint8_t lastMismatchMask = 0;
  static uint8_t lastCommandedMask = 0;
  static uint8_t lastActualMask = 0;
  uint8_t mismatchMask = 0;
  for (uint8_t ch = 0; ch < 8; ++ch) {
    const uint8_t bit = (uint8_t)(1U << ch);
    const bool commanded = (commandedMask & bit) != 0;
    const bool actual = (actualMask & bit) != 0;
    if (commanded == actual) continue;
    mismatchMask |= bit;
    const bool sameMismatch =
        (lastMismatchMask & bit) &&
        (((lastCommandedMask & bit) != 0) == commanded) &&
        (((lastActualMask & bit) != 0) == actual);
    if (!sameMismatch) {
      Serial.printf("[MODBUS MISMATCH] CH%u commanded=%s actual=%s\n",
                    ch + 1, commanded ? "ON" : "OFF", actual ? "ON" : "OFF");
    }
  }
  lastMismatchMask = mismatchMask;
  lastCommandedMask = commandedMask;
  lastActualMask = actualMask;
  return true;
}

static void modbusRelayPollTick() {
  const uint32_t now = millis();
  if ((uint32_t)(now - g_lastModbusRelayPoll) < MODBUS_RELAY_POLL_MS) return;
  modbusRelayPollNow();
}

// ============== Dach / stan ==============
// RoofState and pure transition rules live in roof_control.h.
RoofState roofState = RoofState::UNKNOWN;
bool g_pendingAutoClose = false;
unsigned long roofMoveStartMs = 0;
// ROOF_TIMEOUT_SEC pochodzi z centralnego hardware.ini.
#ifndef ROOF_TIMEOUT_SEC
  #define ROOF_TIMEOUT_SEC 90
#endif
const unsigned long ROOF_MOVE_TIMEOUT_MS = (unsigned long)ROOF_TIMEOUT_SEC * 1000UL;

// detekcja zawieszonego/nieudanego startu ruchu dachu.
// Dziala na istniejacych krancowkach: po komendzie ruchu oczekujemy,
// ze krancowka startowa odpusci w zadanym czasie. Jesli nie - STOP + ERROR.
#ifndef ROOF_STUCK_CHECK_MS
  #define ROOF_STUCK_CHECK_MS 15000UL
#endif
bool g_roofMoveStartLimitClosed = false;
bool g_roofMoveStartLimitOpen   = false;
bool g_roofStuckChecked         = false;
String g_roofMotionFault        = "";
enum class RoofFaultKind { NONE, COMMUNICATION, PHYSICAL, STOP_UNCONFIRMED, PARK_LOST };
RoofFaultKind g_roofFaultKind=RoofFaultKind::NONE;
bool g_stopCommandConfirmed = false;
bool g_stopReleaseConfirmed = true;
bool g_stopAttempted = false;
uint32_t g_stopEpoch = 0; // Invalidate requests accepted before a STOP.

static void roofMotionWatchStart() {
  g_roofMoveStartLimitClosed = (digitalRead(LIMIT_CLOSED_PIN) == LOW);
  g_roofMoveStartLimitOpen   = (digitalRead(LIMIT_OPEN_PIN) == LOW);
  g_roofStuckChecked         = false;
  g_roofMotionFault          = "";
}


// ── stan czujnika kolizji / przeszkody ────────────────────────
bool g_collisionSensorActive = false;
bool g_collisionSensorRawLast = false;
unsigned long g_collisionLastChangeMs = 0;
String g_collisionFault = "";

static bool readCollisionRawActive() {
#if COLLISION_SENSOR_ENABLED
  bool v = (digitalRead(COLLISION_SENSOR_PIN) == LOW);
  #if !COLLISION_SENSOR_ACTIVE_LOW
    v = !v;
  #endif
  return v;
#else
  return false;
#endif
}

static void collisionSensorTick() {
#if COLLISION_SENSOR_ENABLED
  bool raw = readCollisionRawActive();
  unsigned long now = millis();
  if (raw != g_collisionSensorRawLast) {
    g_collisionSensorRawLast = raw;
    g_collisionLastChangeMs = now;
  }
  if ((now - g_collisionLastChangeMs) >= (unsigned long)COLLISION_DEBOUNCE_MS) {
    if (g_collisionSensorActive != raw) {
      g_collisionSensorActive = raw;
      if (g_collisionSensorActive) {
        g_collisionFault = "CZUJNIK KOLIZJI: przeszkoda / kontakt podczas pracy dachu";
        Serial.println(F("!!! COLLISION SENSOR ACTIVE !!!"));
      } else {
        Serial.println(F("[COLLISION] sensor clear"));
      }
    }
  }
#else
  g_collisionSensorActive = false;
#endif
}

String g_lastRoofBlockReason = ""; // musi byc przed collisionClearForRoofMove

// ── stan E-STOP montazu (krancowka overtravel) ─────────────────────
bool          g_mountEstop          = false;   // zatrzasniety E-STOP (wymaga recznego resetu)
bool          g_mountLimitRawLast   = false;   // ostatni surowy odczyt (debounce)
unsigned long g_mountLimitChangeMs  = 0;
unsigned long g_mountEstopSinceMs   = 0;
String        g_mountEstopReason     = "";

// ── watchdog laczności z NINA + zywy stan linku ────────────────────
unsigned long g_lastAlpacaContactMs   = 0;  // zapytania Alpaca/NINA (watchdog)
unsigned long g_lastSafetyMonitorContactMs = 0; // wyłącznie świeży poll SafetyMonitor (brama OPEN)
bool          g_smConnected         = false;  // stan Connected SafetyMonitor
bool          g_commsLost           = false;
unsigned long g_commsLostSinceMs    = 0;
unsigned long g_commsNoContactSinceMs = 0; // watchdog: czas od startu oczekiwania, gdy NINA jeszcze nie odezwała się

// ════════════════════════════════════════════════════════════════════════
// JEDEN wspolny straznik "czy NINA aktywna" dla calego kodu.
// ════════════════════════════════════════════════════════════════════════
// i throttle mialy KAZDY wlasne okno 30s liczone osobno - przez co w pewnych
// zaczela pytac) blokujacy handshake TLS mogl wejsc petli w droge i NINA
// gubila SafetyMonitor. Teraz wszyscy pytaja ten sam helper z jednym, krotszym
// oknem. Krotsze okno (3s) = mniej falszywych "NINA aktywna", ale wystarcza,
// bo NINA odpytuje Alpaca co ~1s. Jesli helper mowi "aktywna" - ZADEN blokujacy
// TLS nie startuje w tym przebiegu loop().
#ifndef NINA_ACTIVE_WINDOW_MS
  #define NINA_ACTIVE_WINDOW_MS 3000UL   // NINA pytala w ostatnich 3s = sesja w toku
#endif
static inline bool ninaIsActive() {
  // TWARDY WARUNEK OTWARCIA: NINA musi mieć aktywnie połączony SafetyMonitor
  // oraz ESP32 musi właśnie odbierać jego zapytania Alpaca. Sam dashboard HTTP,
  // samo odpytywanie Dome/ObservingConditions ani stary Connected=true nie wystarcza.
  if (!g_smConnected) return false;
  if (g_lastSafetyMonitorContactMs == 0) return false;
  unsigned long age = millis() - g_lastSafetyMonitorContactMs;
  if (age > 3600000UL) return false;  // overflow protection
  return age < NINA_ACTIVE_WINDOW_MS;  // 3000ms window
}
bool          g_wifiLinkUp          = false;    // ZYWY stan (nie mylic z g_wifiReady = "wystartowano")
bool          g_ethLinkUp           = false;
bool          g_netLinkUp              = false;
unsigned long g_linkDownSinceMs     = 0;
bool          g_linkLostAlertSent   = false;


static bool collisionClearForRoofMove() {
#if COLLISION_SENSOR_ENABLED
  collisionSensorTick();
  if (g_collisionSensorActive) {
    g_lastRoofBlockReason = "Czujnik kolizji/przeszkody aktywny - ruch dachu zablokowany";
    return false;
  }
#endif
  return true;
}

// Remote park/collision bypasses are not available.

// ============== Dodatkowe zabezpieczenia dachu ==============
#ifndef ROOF_BLOCK_REDUNDANT_COMMANDS
  #define ROOF_BLOCK_REDUNDANT_COMMANDS 1
#endif
#ifndef ROOF_INTERLOCK_RELAYS
  #define ROOF_INTERLOCK_RELAYS 1
#endif

// ============== Meteo / safety ==============
bool   g_unsafe  = true;
String g_reason  = "INIT";

// ============== Tryb MANUAL ==============
bool g_manualMode=false;
bool g_manualAuthorized=false; // RAM only: never restored from NVS.
static bool operatorStore(const char* key,bool value) {
  Preferences prefs;
  if(!prefs.begin("obsop",false))return false;
  const bool ok=prefs.putBool(key,value)==1 && prefs.getBool(key,!value)==value;
  prefs.end();return ok;
}
static void operatorLoad() {
  Preferences prefs;
  if(!prefs.begin("obsop",false)) {
    g_manualMode=true;g_manualAuthorized=false;g_mountPowerRestoreBlocked=true;
    g_mountEstop=true;g_mountEstopReason="Brak odczytu pamieci stanu operatora";
    eventLogEx("CRITICAL","OPERATOR","NVS unavailable; MANUAL locked and mount power blocked");
    return;
  }
  g_manualMode=prefs.getBool("manual",true); // First RC5.2 boot: unknown prior ownership => locked MANUAL.
  g_mountPowerRestoreBlocked=prefs.getBool("mountTrip",false);
  prefs.end();g_manualAuthorized=false;
  if(g_mountPowerRestoreBlocked){g_mountEstop=true;g_mountEstopReason="Zachowana blokada montazu po restarcie";}
}
static bool manualModeOn() {
  if(!operatorStore("manual",true))return false;
  g_manualMode=true;g_manualAuthorized=true;
  eventLogEx("WARN","OPERATOR","MANUAL enabled/re-authorized; persistent, no automatic expiry");
  return true;
}
static bool manualModeOff() {
  if(!operatorStore("manual",false))return false;
  g_manualMode=false;g_manualAuthorized=false;
  eventLogEx("WARN","OPERATOR","AUTO explicitly selected");
  return true;
}
static void manualModeTick() {
  // Ownership is persistent; time never silently switches MANUAL to AUTO.
}

bool  g_rain    = false;
// ═══ RG-11 na PRZERWANIU SPRZETOWYM ═══════════════════════════
// Poczatek deszczu (zbocze na pinie RG-11) lapiemy w ISR, wiec detekcja jest
// NIEZALEZNA od obciazenia loop(): nawet gdy petla chwilowo wisi na sieci
// (np. blokujacy connect Alpaca), zbocze nie ginie. Stan podtrzymany (latched,
// tryb "It's Raining" trzyma przekaznik 30s-5min) i tak dolapuje zwykly poll.
#ifndef RG11_RAIN_WHEN_LOW
  #define RG11_RAIN_WHEN_LOW 1
#endif
volatile bool g_rainIsrFlag = false;
void IRAM_ATTR rg11Isr() { g_rainIsrFlag = true; }  // krotki, bez flash/print - IRAM-safe
// ═══════════════════════════════════════════════════════════════════════════
float g_wind_ms = 0.0f;
float g_tAir    = NAN;
float g_hAir    = NAN;
float g_dewPoint = NAN;
float g_tempSky = NAN;
float g_tempAmb = NAN;
float g_cloudDelta = NAN;
float g_pressure_hPa = NAN;

// ============== Historia sensorow (ringbuffer, 60 probek) ==============
// Probki co ~60s -> ostatnia godzina
struct HistorySample {
  float wind_ms;
  float cloud_delta;
  float t_air;
  bool  rain;
};
const int HISTORY_SIZE = 60;
HistorySample g_history[HISTORY_SIZE];
int  g_historyHead  = 0;
int  g_historyCount = 0;
unsigned long g_lastHistorySample = 0;
const unsigned long HISTORY_INTERVAL_MS = 60000UL;

static void historySample() {
  unsigned long now = millis();
  if (now - g_lastHistorySample < HISTORY_INTERVAL_MS) return;
  g_lastHistorySample = now;
  g_history[g_historyHead] = { g_wind_ms, g_cloudDelta, g_tAir, g_rain };
  g_historyHead = (g_historyHead + 1) % HISTORY_SIZE;
  if (g_historyCount < HISTORY_SIZE) g_historyCount++;
}

// ============== Anemometr CG-FS, wariant 0.4-2.0 V ==============
// Wyjscie analogowe: 0.4V = 0 m/s, 2.0V = 32.4 m/s
// Seller confirmed CG-FS. Variant and correct supply require installation checks.
// Defaults live in wind_monitor.h; a valid NVS record overrides them.
static WindCalibration g_windCal;
static WindValidity g_windValidity;
static esp_adc_cal_characteristics_t g_windAdcChars;
static float g_windAdcRaw=NAN, g_windAdcVolts=NAN;
static uint16_t g_windAdcMin=0,g_windAdcMax=0;
static unsigned g_windAdcRails=0;
static uint32_t g_windRejectedFrames=0,g_windHeldFrames=0;
static uint32_t g_windStableSince=0;
static float g_windStableReference=NAN;
static bool windMeasurementUsable();
static void windCalibrationLoad() {
  Preferences prefs;
  if(!prefs.begin("windcal",true)) return;
  WindCalibration stored;
  const bool ok=prefs.getBytesLength("record")==sizeof(stored) &&
    prefs.getBytes("record",&stored,sizeof(stored))==sizeof(stored) && windRecordValid(stored);
  prefs.end();
  if(ok) g_windCal=stored; // absent/corrupt records remain UNVERIFIED
}
static bool windCalibrationStore(WindCalibration candidate) {
  candidate.checksum=windChecksum(candidate);
  Preferences prefs;
  if(!prefs.begin("windcal",false)) return false;
  WindCalibration readback;
  const bool ok=prefs.putBytes("record",&candidate,sizeof(candidate))==sizeof(candidate) &&
    prefs.getBytes("record",&readback,sizeof(readback))==sizeof(readback) &&
    windRecordValid(readback) && memcmp(&candidate,&readback,sizeof(candidate))==0;
  if(!ok) {
    // Best effort persist an unverified record when write/readback fails.
    candidate.verified=0; candidate.checksum=windChecksum(candidate);
    prefs.putBytes("record",&candidate,sizeof(candidate));
  }
  prefs.end();
  if(ok) g_windCal=readback;
  else g_windCal.verified=0;
  return ok;
}
static const int   WIND_ADC_SAMPLES   = 16;

// ============== Progi bezpieczenstwa (przemyslane) ==============
//
// ZASADA: chronimy sprzet przed USZKODZENIEM (deszcz, silny wiatr), a nie
// przed gorsza jakoscia zdjec (chmury). Lekki wiatr i male chmury = SAFE.
//
// ── CHMURY (MLX90614, delta = T_otoczenia - T_nieba) ───────────────────────
// Czyste, zimne niebo w podczerwieni -> duza delta. Zachmurzenie -> mala delta.
// Wartosci zaleza od pory roku/wilgotnosci, wiec progi sa konserwatywne.
//   delta >= CLEAR        -> CZYSTE        (SAFE)
//   SCATTERED.. CLEAR     -> CZESC. CHMURY (SAFE - "male chmury")
//   OVERCAST.. SCATTERED  -> ZACHMURZENIE  (SAFE - tylko raportowane do NINA)
//   delta <  OVERCAST     -> GESTE CHMURY  (kandydat na UNSAFE, patrz nizej)
//
// UWAGA te progi sa EDYTOWALNE z poziomu web UI i zapisywane w NVS
// (przetrwaja restart). Wartosci ponizej to tylko DOMYSLNE (po pierwszym starcie
// lub po "Przywroc domyslne"). Mozna je tez nadpisac kompilacyjnie -D CFG_...
#ifndef CFG_CLOUD_CLEAR
  #define CFG_CLOUD_CLEAR 16.0f
#endif
#ifndef CFG_CLOUD_SCATTERED
  #define CFG_CLOUD_SCATTERED 9.0f
#endif
#ifndef CFG_CLOUD_OVERCAST
  #define CFG_CLOUD_OVERCAST 3.0f
#endif
float CLOUD_DELTA_CLEAR         = CFG_CLOUD_CLEAR;      // powyzej -> niebo czyste
float CLOUD_DELTA_SCATTERED     = CFG_CLOUD_SCATTERED;  // powyzej -> rozproszone chmury (SAFE)
float CLOUD_DELTA_OVERCAST      = CFG_CLOUD_OVERCAST;   // ponizej (trwale) -> realnie zasniete
float CLOUD_DELTA_OVERCAST_CLEAR= CFG_CLOUD_OVERCAST + 3.0f; // histereza (auto = OVERCAST+3)
const float CLOUD_HYST_MARGIN   = 3.0f;                // margines histerezy chmur [C]
// Chmury robia UNSAFE TYLKO gdy delta < OVERCAST utrzymane przez CLOUD_SUSTAIN_MS.
// przelacznik runtime (edytowalny z UI, zapisywany w NVS). Define = DOMYSLNY.
#ifndef CLOUD_CAN_TRIGGER_UNSAFE
  #define CLOUD_CAN_TRIGGER_UNSAFE 1     // 0 = chmury nigdy nie robia UNSAFE (tylko RG11+wiatr)
#endif
#include "weather_policy.h"
astro_weather::Policy g_weatherPolicy;
bool& g_cloudCanTriggerUnsafe = g_weatherPolicy.clouds;
bool g_weatherPolicyFault=false;
astro_weather::Moisture g_moisture;
astro_weather::Stable g_weatherStable;
bool g_weatherOpenReady=false;
static void weatherPolicyLoad();
#ifndef CLOUD_SUSTAIN_MS
  #define CLOUD_SUSTAIN_MS 180000UL      // 3 min trwalego OVERCAST zanim UNSAFE
#endif

// ── WIATR (anemometr analogowy) ────────────────────────────────────────────
// Liczymy srednia trwala (sustained) i podmuch (gust) z okna probkowania.
// Lekki/umiarkowany wiatr = SAFE. UNSAFE dopiero przy silnym, TRWALYM wietrze.
//   ~ <10 m/s (~36 km/h)  -> bez znaczenia dla bezpieczenstwa
//   ~14 m/s  (~50 km/h)   -> silny, dlugotrwaly -> zamykamy
//   ~22 m/s  (~80 km/h)   -> wichura/podmuch -> zamykamy natychmiast
// (EDYTOWALNE z web UI + NVS, jak progi chmur.)
#ifndef CFG_WIND_WARN
  #define CFG_WIND_WARN 10.0f
#endif
#ifndef CFG_WIND_SUSTAINED
  #define CFG_WIND_SUSTAINED 14.0f
#endif
#ifndef CFG_WIND_GUST
  #define CFG_WIND_GUST 22.0f
#endif
#ifndef CFG_WIND_RESET
  #define CFG_WIND_RESET 10.0f
#endif
float WIND_WARN_MS            = CFG_WIND_WARN;       // tylko UI/ostrzezenie, NIE robi UNSAFE
float WIND_SUSTAINED_LIMIT_MS = CFG_WIND_SUSTAINED;  // srednia trwala powyzej -> UNSAFE
float WIND_GUST_LIMIT_MS      = CFG_WIND_GUST;       // pojedynczy podmuch powyzej -> UNSAFE
float WIND_SAFE_RESET_MS      = CFG_WIND_RESET;      // histereza: srednia musi spasc ponizej
static bool windMeasurementUsable() {
  return windUsable(g_windCal,g_windValidity,millis()) &&
    g_windCal.maxMS>=WIND_GUST_LIMIT_MS && g_windCal.maxMS>=WIND_SUSTAINED_LIMIT_MS;
}
static const char* windStatusText() {
  if(g_windCal.verified && (g_windCal.maxMS<WIND_GUST_LIMIT_MS || g_windCal.maxMS<WIND_SUSTAINED_LIMIT_MS)) return "RANGE_TOO_SMALL";
  return windStatus(g_windCal,g_windValidity,millis());
}

// Okno usredniania: readSensorsAndSafety dodaje probke co ok. 1 s.
#ifndef WIND_WINDOW_SAMPLES
  #define WIND_WINDOW_SAMPLES 64            // ~64*1s = ~64s okna sredniej/gust
#endif

// Legacy electrical fault timer is retained. Additional trust/freshness/range
// gates act immediately; the old broad interval cannot prove sensor health.
#ifndef WIND_FAULT_V_LOW
  #define WIND_FAULT_V_LOW 0.03f       // retained legacy low electrical-fault threshold; not a model identification
#endif
#ifndef WIND_FAULT_V_HIGH
  #define WIND_FAULT_V_HIGH 3.00f      // >3.0V = zwarcie do zasilania/uszkodzenie (nadpisywalny)
#endif
#ifndef WIND_FAULT_MS
  #define WIND_FAULT_MS 90000UL              // 90s NIEPRZERWANIE poza zakresem = AWARIA
#endif
// ═══ /═══
#ifndef WIND_FAULT_UNSAFE
  #define WIND_FAULT_UNSAFE 1                // 1 = awaria anemometru -> UNSAFE (fail-safe)
#endif

// ── Wlacznik anemometru () ────────────────────────────────────────────
// Gdy WYLACZONY: wiatr NIE wplywa na bezpieczenstwo (brak UNSAFE od wiatru i od
// awarii), brak alarmow "AWARIA ANEMOMETRU". Przydatne przy commissioningu, gdy
// czujnik nie jest jeszcze podlaczony / kabel pływa. Domyslnie WLACZONY.
// Edytowalny z web UI (zapis w NVS). Wartosc surowa nadal widoczna w diagnostyce.
#ifndef ANEMOMETER_ENABLED
  #define ANEMOMETER_ENABLED 1
#endif
bool g_anemometerEnabled = (ANEMOMETER_ENABLED != 0);

// ── MLX (czujnik chmur) ────────────────────────────────────────────────────
#ifndef MLX_FAIL_UNSAFE
  #define MLX_FAIL_UNSAFE 0                  // 0 = brak/NAN MLX NIE blokuje dachu (tylko alert)
#endif

// ═══ KALIBRACJA CHMUR (MLX90614) ══════════════════════════════
// Problem: surowa delta (T_amb - T_sky) MOCNO zalezy od temperatury powietrza
// i wilgotnosci. Czyste niebo zima daje delte 25-35 C, a parne lato 12-18 C -
// STALE progi 16/9/3 nie moga byc dobre przez caly rok ("kalibracja nie dziala").
// Rozwiazania (wszystko edytowalne z web UI, trwale w NVS):
// (a) Korekcja nieba wg AAG CloudWatcher:
//       Td = K1/100*(Tamb - K2/10) + K3/100 * (exp(K4/1000*Tamb))^(K5/100)
//       T_sky_kor = T_sky - Td;  delta = T_powietrza - T_sky_kor
//     Domyslne K = 33/0/4/100/100 (standard AAG). Przy +20 C dodaje ~+7 C do
//     delty, przy -10 C odejmuje ~3 C - splaszcza sezonowosc, progi 16/9/3
//     znow maja sens caly rok. K1=0 i K3=0 = korekcja neutralna.
// (b) Referencja otoczenia dla delty: T POWIETRZA z BME280 (fallback: T_amb
//     MLX). Powod: grzalka przeciwrosna podgrzewa puszke MLX -> T_amb MLX
//     rosnie -> delta sie zawyzala ("czysciej niz jest") DOKLADNIE wtedy,
//     gdy jest mgla/rosa. T_amb MLX zostaje dla dew-margin (tam jest wlasciwa).
// (c) Offsety uzytkownika: trym T_sky / T_amb po porownaniu z referencja
//     (np. sciana o znanej temp., tasma o emisyjnosci ~0.95).
// (d) Blanking: po przelaczeniu mastera grzalek MLX ma stan przejsciowy (gradient
//     termiczny w puszce psuje pomiar OBIEKTU wg noty katalogowej) - przez
//     MLX_HEATER_BLANK_MS trzymamy ostatnia dobra T_sky zamiast czytac smieci.
#ifndef CFG_SKY_CORR_ENABLED
  #define CFG_SKY_CORR_ENABLED 1
#endif
#ifndef CFG_SKY_K1
  #define CFG_SKY_K1 33.0f
#endif
#ifndef CFG_SKY_K2
  #define CFG_SKY_K2 0.0f
#endif
#ifndef CFG_SKY_K3
  #define CFG_SKY_K3 4.0f
#endif
#ifndef CFG_SKY_K4
  #define CFG_SKY_K4 100.0f
#endif
#ifndef CFG_SKY_K5
  #define CFG_SKY_K5 100.0f
#endif
bool  g_skyCorrEnabled = (CFG_SKY_CORR_ENABLED != 0);
float g_skyK1 = CFG_SKY_K1;
float g_skyK2 = CFG_SKY_K2;
float g_skyK3 = CFG_SKY_K3;
float g_skyK4 = CFG_SKY_K4;
float g_skyK5 = CFG_SKY_K5;
float g_mlxSkyOff = 0.0f;        // [C] offset dodawany do surowej T_sky
float g_mlxAmbOff = 0.0f;        // [C] offset dodawany do surowej T_amb MLX
float g_skyTd = NAN;             // diagnostyka: ostatnia poprawka Td
float g_cloudDeltaRaw = NAN;     // diagnostyka: delta surowa (bez korekcji, od T_amb MLX)

// ═══ AUTO-KALIBRACJA CHMUR (samoucząca linia czystego nieba) ═══
// Zamiast recznych K1..K5: firmware uczy sie MAKSYMALNEJ delty (czyste niebo)
// jako funkcji temperatury powietrza. Chmury deltę tylko zmniejszaja, wiec gorna
// obwiednia obserwacji = czyste niebo. Miara chmur = "ile °C ponizej czystego".
// Bezpiecznik: kosz nienauczony -> surowa delta (zachowanie jak dotad, bezpieczne).
#ifndef AUTO_CLOUD_DEFAULT
  #define AUTO_CLOUD_DEFAULT 1          // domyslnie WLACZONE
#endif
#define AC_TMIN     (-25.0f)           // dolna krawedz pierwszego kosza (°C powietrza)
#define AC_TSTEP    5.0f               // szerokosc kosza (°C)
#define AC_BINS     13                 // -25..+40 °C
#define AC_RAISE    0.5f               // max wzrost obwiedni na 1 probke (anty-spike)
#define AC_DECAY_HR 0.02f              // spadek obwiedni na godzine (leczy zawyzenie od glitcha)
#define AC_TRUST    30                 // tyle trafien w koszu -> "zaufany"
#define AC_STABLE   4.0f               // |raw-prev|>tego -> nie ucz (zmienne/przelotne niebo)
#define AC_SAVE_MS  1800000UL          // autozapis nauki do NVS co 30 min

bool     g_autoCloud   = (AUTO_CLOUD_DEFAULT != 0);
float    g_acClr[AC_BINS];             // nauczona delta czystego nieba per kosz (NAN=pusty)
uint16_t g_acHits[AC_BINS];            // pewnosc per kosz (0=pusty)
uint32_t g_acLastDecayMs = 0;
uint32_t g_acLastSaveMs  = 0;
float    g_acPrevRaw   = NAN;
bool     g_acDirty     = false;        // czy nauka zmieniona od ostatniego zapisu
// diagnostyka (do UI):
float    g_acExpected  = NAN;          // przewidywana delta czystego nieba TERAZ
bool     g_acTrusted   = false;        // czy uzyto nauczonej obwiedni (vs surowa)
float    g_acGap       = NAN;          // ile °C ponizej czystego (miara chmur)

static int   acBin(float t){ int b=(int)floorf((t-AC_TMIN)/AC_TSTEP); if(b<0)b=0; if(b>=AC_BINS)b=AC_BINS-1; return b; }
static int   acLearnedBins(){ int n=0; for(int i=0;i<AC_BINS;i++) if(g_acHits[i]>=AC_TRUST) n++; return n; }
static void  acResetLearning(){ for(int i=0;i<AC_BINS;i++){ g_acClr[i]=NAN; g_acHits[i]=0; } g_acPrevRaw=NAN; g_acDirty=true; }

// powolny spadek obwiedni: leczy jednorazowe zawyzenie (glitch), a prawdziwe
// czyste niebo i tak co jakis czas odswieza max. Kierunek bezpieczny: gdyby
// obwiednia byla za wysoka -> niebo wyglada "bardziej pochmurno" -> dach zamkniety.
static void acDecayTick(uint32_t now){
  if(g_acLastDecayMs==0){ g_acLastDecayMs=now; return; }
  uint32_t dt = now - g_acLastDecayMs;
  if(dt < 60000UL) return;                 // co ~minute
  g_acLastDecayMs = now;
  float dec = AC_DECAY_HR * (dt/3600000.0f);
  for(int i=0;i<AC_BINS;i++) if(g_acHits[i]>0 && !isnan(g_acClr[i])) g_acClr[i]-=dec;
}

// uczenie: podnos obwiednia (max) tylko przy stabilnym, suchym, sensownym odczycie.
static void acLearn(float tAir, float rawDelta, bool dry, bool mlxOK){
  if(!mlxOK || !dry || isnan(tAir) || isnan(rawDelta)){ g_acPrevRaw=rawDelta; return; }
  if(rawDelta < CLOUD_DELTA_CLEAR || rawDelta > 60.0f){ g_acPrevRaw=rawDelta; return; }
  bool stable = isnan(g_acPrevRaw) || fabsf(rawDelta-g_acPrevRaw) <= AC_STABLE;
  g_acPrevRaw = rawDelta;
  if(!stable) return;                       // przelotne chmury / szybka zmiana -> nie ucz
  int b = acBin(tAir);
  if(isnan(g_acClr[b])){ g_acClr[b]=rawDelta; g_acHits[b]=1; g_acDirty=true; return; }  // seed
  if(rawDelta > g_acClr[b]){ float d=rawDelta-g_acClr[b]; g_acClr[b]+=(d<AC_RAISE?d:AC_RAISE); g_acDirty=true; }
  if(g_acHits[b] < 60000) g_acHits[b]++;
}

// przewidywana delta czystego nieba w danej T (interpolacja miedzy zaufanymi koszami)
static float acExpectedClear(float tAir){
  if(isnan(tAir)) return NAN;
  int b = acBin(tAir);
  if(g_acHits[b]>=AC_TRUST && isfinite(g_acClr[b]) && g_acClr[b]>=CLOUD_DELTA_CLEAR) return g_acClr[b];
  // brak zaufania w tym koszu -> najblizszy zaufany w promieniu 2 koszy (±10°C)
  for(int r=1;r<=2;r++){
    if(b-r>=0      && g_acHits[b-r]>=AC_TRUST && isfinite(g_acClr[b-r]) && g_acClr[b-r]>=CLOUD_DELTA_CLEAR) return g_acClr[b-r];
    if(b+r<AC_BINS && g_acHits[b+r]>=AC_TRUST && isfinite(g_acClr[b+r]) && g_acClr[b+r]>=CLOUD_DELTA_CLEAR) return g_acClr[b+r];
  }
  return NAN;                                // nic zaufanego w poblizu
}

// przelicz surowa deltę na skale zgodna z progami CLEAR/SCAT/OVC ("ile ponizej czystego")
static float acNormalize(float rawDelta, float tAir, bool mlxOK){
  g_acExpected=NAN; g_acGap=NAN; g_acTrusted=false;
  if(!mlxOK || isnan(rawDelta) || isnan(tAir)) return rawDelta;   // brak danych -> surowa
  float exp = acExpectedClear(tAir);
  if(isnan(exp)) return rawDelta;            // nic nauczonego -> surowa (bezpiecznie jak dotad)
  g_acTrusted=true; g_acExpected=exp;
  float gap = exp - rawDelta; if(gap<0) gap=0;
  g_acGap = gap;
  // Conservative model: never promote the raw classification.
  return astro_roof::cloudNormalize(rawDelta,exp,CLOUD_DELTA_CLEAR);
}

// jednym tapnieciem: "niebo jest teraz czyste" - zapisz biezacy punkt jako zaufany
static bool acSeedClearNow(float tAir, float rawDelta, bool mlxOK){
  if(!mlxOK || isnan(tAir) || isnan(rawDelta)) return false;
  if(rawDelta < CLOUD_DELTA_CLEAR) return false;   // to nie wyglada na czyste niebo - odmow
  int b = acBin(tAir);
  g_acClr[b]  = rawDelta;
  g_acHits[b] = AC_TRUST;                     // od razu zaufany
  g_acDirty   = true;
  return true;
}

// 0 = AUTO (kabel preferowany, failover na WiFi jak dotad)
// 2 = TYLKO WiFi (diagnostycznie)
#ifndef NET_MODE_DEFAULT
  // DOMYSLNIE 0 = AUTO (WiFi preferowane dla chmurowego TLS - patrz
  // netChoose() - ale z failoverem na ETH gdy WiFi nie ma/nie dziala).
  // POWOD ZMIANY z (tam bylo na sztywno 2=TYLKO WiFi): przy sprzecie,
  // gdzie Ethernet jest podstawowym, pewnym lączem (DHCP-rezerwacje, stale
  // podlaczony kabel), a WiFi bywa niepewne w kopule - wymuszony "TYLKO WiFi"
  // (patrz fix WiFi-reconnect w loop() - to byl efekt uboczny tego samego
  // problemu). AUTO nadal preferuje WiFi (bo TLS po W5500 potrafi
  // zakleszczyc), ale spada na ETH gdy WiFi ma problem, zamiast milczec.
  #define NET_MODE_DEFAULT 0
#endif
uint8_t g_netMode = NET_MODE_DEFAULT;        // runtime, zapis w NVS

// Trzy grzalki 12 V sterowane przez osobny Waveshare Modbus RTU PWM Output 4CH.
// Jego wyjscia PWM 3.3 V steruja bramkami MOSFET; GPIO plytki ESP nie sa zajmowane.
#ifndef HEATER_PWM_MAX_SENSOR_AGE_MS
  #define HEATER_PWM_MAX_SENSOR_AGE_MS 10000UL
#endif
#ifndef HEATER_PWM_MARGIN_ZERO_C
  #define HEATER_PWM_MARGIN_ZERO_C 4.0f
#endif
#ifndef HEATER_PWM_MARGIN_FULL_C
  #define HEATER_PWM_MARGIN_FULL_C 1.0f
#endif
bool g_heaterPwmOnline = false;
bool g_heaterSensorValid = false;
float g_heaterDewMarginC = NAN;
bool g_heaterChannelEnabled[3] = {false, false, false};
float g_heaterRatedPowerW[3] = {20.0f, 20.0f, 20.0f};
float g_heaterDutyPct[3] = {0.0f, 0.0f, 0.0f};
// Tryb reczny grzalek: tylko w RAM, NIGDY nie zapisywany w NVS (po restarcie grzalki = AUTO/OFF).
bool  g_heaterManual[3] = {false, false, false};
float g_heaterManualPct[3] = {0.0f, 0.0f, 0.0f};
unsigned long g_heaterManualSinceMs[3] = {0, 0, 0};
// Tasma LED (CH4): jasnosc zapamietana w NVS ("ledBr"), wlaczenie zawsze reczne z WWW.
float g_ledBrightPct = 50.0f;
float g_ledDutyPct = 0.0f;
bool g_ledPwmOn = false;
uint32_t g_heaterLastTxMs[3] = {};
uint8_t g_pwmOffChannel = 0;
bool g_pwmOffConfirmed = false;
unsigned long g_heaterLastProbeMs = 0;

// ── Pozostale ──────────────────────────────────────────────────────────────
const float HUMIDITY_LIMIT        = 90.0f;
const float DEW_POINT_MARGIN      = 3.0f;

// ============== Edytowalne progi: zapis/odczyt w NVS () ==============
// Klucze NVS w przestrzeni "obscfg". Walidacja chroni przed wpisaniem progow,
// ktore wylaczylyby ochrone (np. limit wiatru = 0). Kolejnosc progow chmur
// musi byc: OVERCAST < SCATTERED < CLEAR. Wiatru: RESET <= SUSTAINED <= GUST.
Preferences g_prefs;

static void cfgApplyHysteresis() {
  // histereza chmur zawsze = OVERCAST + margines
  CLOUD_DELTA_OVERCAST_CLEAR = CLOUD_DELTA_OVERCAST + CLOUD_HYST_MARGIN;
}

// Sprawdza spojnosc kandydatow. Zwraca "" gdy OK, albo opis bledu.
static String cfgValidate(float ccClr, float ccSca, float ccOvc,
                          float wWarn, float wSus, float wGust, float wRst) {
  if (ccClr < 5.0f  || ccClr > 45.0f) return "cloud_clear poza 5..45";
  if (ccSca < 2.0f  || ccSca > 35.0f) return "cloud_scattered poza 2..35";
  if (ccOvc < 0.0f  || ccOvc > 20.0f) return "cloud_overcast poza 0..20";
  if (!(ccOvc < ccSca && ccSca < ccClr)) return "wymagane: overcast < scattered < clear";
  if (wWarn < 1.0f  || wWarn > 35.0f) return "wind_warn poza 1..35";
  if (wSus  < 3.0f  || wSus  > 45.0f) return "wind_sustained poza 3..45";
  if (wGust < 5.0f  || wGust > 60.0f) return "wind_gust poza 5..60";
  if (wRst  < 1.0f  || wRst  > 45.0f) return "wind_reset poza 1..45";
  if (!(wRst <= wSus && wSus <= wGust)) return "wymagane: reset <= sustained <= gust";
  return "";
}

static void cfgSaveToNVS() {
  if (!g_prefs.begin("obscfg", false)) return;
  g_prefs.putFloat("ccClr", CLOUD_DELTA_CLEAR);
  g_prefs.putFloat("ccSca", CLOUD_DELTA_SCATTERED);
  g_prefs.putFloat("ccOvc", CLOUD_DELTA_OVERCAST);
  g_prefs.putFloat("wWarn", WIND_WARN_MS);
  g_prefs.putFloat("wSus",  WIND_SUSTAINED_LIMIT_MS);
  g_prefs.putFloat("wGust", WIND_GUST_LIMIT_MS);
  g_prefs.putFloat("wRst",  WIND_SAFE_RESET_MS);
  g_prefs.putUChar("ccTrig", g_cloudCanTriggerUnsafe ? 1 : 0);
  g_prefs.putUChar("anemEn", g_anemometerEnabled ? 1 : 0);
  uint8_t heaterMask = 0;
  for (uint8_t i = 0; i < 3; ++i) if (g_heaterChannelEnabled[i]) heaterMask |= (uint8_t)(1U << i);
  g_prefs.putUChar("heatEn", heaterMask);
  g_prefs.putFloat("heatW1", g_heaterRatedPowerW[0]);
  g_prefs.putFloat("heatW2", g_heaterRatedPowerW[1]);
  g_prefs.putFloat("heatW3", g_heaterRatedPowerW[2]);
  g_prefs.putFloat("ledBr", g_ledBrightPct);
  // ═══ kalibracja MLX + tryb sieci ═══
  g_prefs.putUChar("skyEn", g_skyCorrEnabled ? 1 : 0);
  g_prefs.putFloat("skyK1", g_skyK1);
  g_prefs.putFloat("skyK2", g_skyK2);
  g_prefs.putFloat("skyK3", g_skyK3);
  g_prefs.putFloat("skyK4", g_skyK4);
  g_prefs.putFloat("skyK5", g_skyK5);
  g_prefs.putFloat("mlxSO", g_mlxSkyOff);
  g_prefs.putFloat("mlxAO", g_mlxAmbOff);
  g_prefs.putUChar("netMd", g_netMode);
  // ═══ auto-kalibracja chmur ═══
  g_prefs.putUChar("acOn", g_autoCloud ? 1 : 0);
  g_prefs.putBytes("acClr",  g_acClr,  sizeof(g_acClr));
  g_prefs.putBytes("acHits", g_acHits, sizeof(g_acHits));
  g_acDirty = false;
  g_prefs.end();
}

static void cfgLoadFromNVS() {
  if (!g_prefs.begin("obscfg", true)) { cfgApplyHysteresis(); weatherPolicyLoad(); return; }
  CLOUD_DELTA_CLEAR        = g_prefs.getFloat("ccClr", CLOUD_DELTA_CLEAR);
  CLOUD_DELTA_SCATTERED    = g_prefs.getFloat("ccSca", CLOUD_DELTA_SCATTERED);
  CLOUD_DELTA_OVERCAST     = g_prefs.getFloat("ccOvc", CLOUD_DELTA_OVERCAST);
  WIND_WARN_MS             = g_prefs.getFloat("wWarn", WIND_WARN_MS);
  WIND_SUSTAINED_LIMIT_MS  = g_prefs.getFloat("wSus",  WIND_SUSTAINED_LIMIT_MS);
  WIND_GUST_LIMIT_MS       = g_prefs.getFloat("wGust", WIND_GUST_LIMIT_MS);
  WIND_SAFE_RESET_MS       = g_prefs.getFloat("wRst",  WIND_SAFE_RESET_MS);
  // SANITY: histereza MUSI byc ponizej limitu sredniej, inaczej
  // alarm wiatru migocze i zasypuje alertami (reset >= limit = odwrocona histereza).
  if (WIND_SAFE_RESET_MS >= WIND_SUSTAINED_LIMIT_MS) {
    WIND_SAFE_RESET_MS = WIND_SUSTAINED_LIMIT_MS * 0.75f;   // np. limit 6.0 -> reset 4.5
    Serial.printf("[CFG] KOREKTA: wiatr RST >= SUS - ustawiam RST=%.1f (75%% limitu)\n", WIND_SAFE_RESET_MS);
  }
  g_cloudCanTriggerUnsafe  = g_prefs.getUChar("ccTrig", g_cloudCanTriggerUnsafe ? 1 : 0) != 0;
  g_anemometerEnabled = (ANEMOMETER_ENABLED != 0); // historical NVS cannot disable wind protection
  const uint8_t heaterMask = g_prefs.getUChar("heatEn", 0); // nowe grzalki zawsze domyslnie OFF
  const float storedHeaterW[3] = {
    g_prefs.getFloat("heatW1", 20.0f), g_prefs.getFloat("heatW2", 20.0f), g_prefs.getFloat("heatW3", 20.0f)
  };
  for (uint8_t i = 0; i < 3; ++i) {
    g_heaterChannelEnabled[i] = HEATER_PWM_ENABLED && ((heaterMask & (1U << i)) != 0);
    g_heaterRatedPowerW[i] = (isfinite(storedHeaterW[i]) && storedHeaterW[i] >= 1.0f && storedHeaterW[i] <= 60.0f)
        ? storedHeaterW[i] : 20.0f;
  }
  g_ledBrightPct = g_prefs.getFloat("ledBr", g_ledBrightPct);
  if (!isfinite(g_ledBrightPct) || g_ledBrightPct < 1.0f || g_ledBrightPct > 100.0f) g_ledBrightPct = 50.0f;
  // ═══ kalibracja MLX + tryb sieci (z sanity-clampem) ═══
  g_skyCorrEnabled = g_prefs.getUChar("skyEn", g_skyCorrEnabled ? 1 : 0) != 0;
  g_skyK1     = g_prefs.getFloat("skyK1", g_skyK1);
  g_skyK2     = g_prefs.getFloat("skyK2", g_skyK2);
  g_skyK3     = g_prefs.getFloat("skyK3", g_skyK3);
  g_skyK4     = g_prefs.getFloat("skyK4", g_skyK4);
  g_skyK5     = g_prefs.getFloat("skyK5", g_skyK5);
  g_mlxSkyOff = g_prefs.getFloat("mlxSO", g_mlxSkyOff);
  g_mlxAmbOff = g_prefs.getFloat("mlxAO", g_mlxAmbOff);
  g_netMode   = g_prefs.getUChar("netMd", g_netMode);
  if (isnan(g_skyK1) || g_skyK1 < 0.0f || g_skyK1 > 100.0f)  g_skyK1 = CFG_SKY_K1;
  if (isnan(g_skyK2) || g_skyK2 < -100.0f || g_skyK2 > 100.0f) g_skyK2 = CFG_SKY_K2;
  if (isnan(g_skyK3) || g_skyK3 < 0.0f || g_skyK3 > 100.0f)  g_skyK3 = CFG_SKY_K3;
  if (isnan(g_skyK4) || g_skyK4 < 0.0f || g_skyK4 > 1000.0f) g_skyK4 = CFG_SKY_K4;
  if (isnan(g_skyK5) || g_skyK5 < 0.0f || g_skyK5 > 200.0f)  g_skyK5 = CFG_SKY_K5;
  if (isnan(g_mlxSkyOff) || g_mlxSkyOff < -10.0f || g_mlxSkyOff > 10.0f) g_mlxSkyOff = 0.0f;
  if (isnan(g_mlxAmbOff) || g_mlxAmbOff < -10.0f || g_mlxAmbOff > 10.0f) g_mlxAmbOff = 0.0f;
  if (g_netMode > 2) g_netMode = 0;
  // ═══ auto-kalibracja chmur ═══
  g_autoCloud = g_prefs.getUChar("acOn", g_autoCloud ? 1 : 0) != 0;
  acResetLearning();  // ustaw NAN/0, potem nadpisz jesli w NVS jest poprawny blob
  if (g_prefs.getBytesLength("acClr")  == sizeof(g_acClr))  g_prefs.getBytes("acClr",  g_acClr,  sizeof(g_acClr));
  if (g_prefs.getBytesLength("acHits") == sizeof(g_acHits)) g_prefs.getBytes("acHits", g_acHits, sizeof(g_acHits));
  for (int i=0;i<AC_BINS;i++){                       // sanity: odrzuc absurdy z NVS
    if (!isnan(g_acClr[i]) && (g_acClr[i] < -5.0f || g_acClr[i] > 60.0f)) { g_acClr[i]=NAN; g_acHits[i]=0; }
  }
  g_acDirty = false;
  g_prefs.end();
  // Gdyby w NVS znalazly sie niespojne wartosci (np. po recznej edycji) - odrzuc.
  if (cfgValidate(CLOUD_DELTA_CLEAR, CLOUD_DELTA_SCATTERED, CLOUD_DELTA_OVERCAST,
                  WIND_WARN_MS, WIND_SUSTAINED_LIMIT_MS, WIND_GUST_LIMIT_MS,
                  WIND_SAFE_RESET_MS).length()) {
    Serial.println(F("[CFG] Niespojne progi w NVS - przywracam domyslne"));
    CLOUD_DELTA_CLEAR = CFG_CLOUD_CLEAR; CLOUD_DELTA_SCATTERED = CFG_CLOUD_SCATTERED;
    CLOUD_DELTA_OVERCAST = CFG_CLOUD_OVERCAST;
    WIND_WARN_MS = CFG_WIND_WARN; WIND_SUSTAINED_LIMIT_MS = CFG_WIND_SUSTAINED;
    WIND_GUST_LIMIT_MS = CFG_WIND_GUST; WIND_SAFE_RESET_MS = CFG_WIND_RESET;
  }
  cfgApplyHysteresis();
  weatherPolicyLoad();
  Serial.printf("[CFG] Progi: chmury CLEAR=%.0f SCAT=%.0f OVC=%.0f (trigger=%d) | wiatr WARN=%.0f SUS=%.0f GUST=%.0f RST=%.0f\n",
                CLOUD_DELTA_CLEAR, CLOUD_DELTA_SCATTERED, CLOUD_DELTA_OVERCAST, (int)g_cloudCanTriggerUnsafe,
                WIND_WARN_MS, WIND_SUSTAINED_LIMIT_MS, WIND_GUST_LIMIT_MS, WIND_SAFE_RESET_MS);
}

static void cfgResetDefaults() {
  CLOUD_DELTA_CLEAR = CFG_CLOUD_CLEAR; CLOUD_DELTA_SCATTERED = CFG_CLOUD_SCATTERED;
  CLOUD_DELTA_OVERCAST = CFG_CLOUD_OVERCAST;
  WIND_WARN_MS = CFG_WIND_WARN; WIND_SUSTAINED_LIMIT_MS = CFG_WIND_SUSTAINED;
  WIND_GUST_LIMIT_MS = CFG_WIND_GUST; WIND_SAFE_RESET_MS = CFG_WIND_RESET;
  // Weather-policy choices are retained by a threshold reset.
  for (uint8_t i = 0; i < 3; ++i) {
    g_heaterChannelEnabled[i] = false;
    g_heaterRatedPowerW[i] = 20.0f;
    g_heaterDutyPct[i] = 0.0f;
  }
  g_anemometerEnabled = (ANEMOMETER_ENABLED != 0);
  // ═══ kalibracja MLX + tryb sieci ═══
  g_skyCorrEnabled = (CFG_SKY_CORR_ENABLED != 0);
  g_skyK1 = CFG_SKY_K1; g_skyK2 = CFG_SKY_K2; g_skyK3 = CFG_SKY_K3;
  g_skyK4 = CFG_SKY_K4; g_skyK5 = CFG_SKY_K5;
  g_mlxSkyOff = 0.0f; g_mlxAmbOff = 0.0f;
  g_netMode = NET_MODE_DEFAULT;
  g_autoCloud = (AUTO_CLOUD_DEFAULT != 0);   // wlacz auto (nauki NIE kasujemy - to osobny przycisk "Zapomnij")
  cfgApplyHysteresis();
  cfgSaveToNVS();
}

// ============== Stan analizy pogody () ==============
// Wiatr: okno kroczace do liczenia sredniej trwalej i podmuchu.
float        g_windWindow[WIND_WINDOW_SAMPLES] = {};
uint32_t g_windWindowTimes[WIND_WINDOW_SAMPLES] = {};
int          g_windWindowHead  = 0;
int          g_windWindowCount = 0;
float        g_windSustained   = 0.0f;   // srednia krocząca [m/s]
float        g_windGust        = 0.0f;   // maks. z okna [m/s]
float        g_windRawVolts    = NAN;    // ostatnie napiecie z anemometru [V]
bool         g_windFault       = false;  // awaria anemometru (poza zakresem)
unsigned long g_windFaultSinceMs = 0;    // od kiedy poza zakresem (0 = w normie)

// Zatrzasniete stany pogodowe z histereza (czyszczone przy spadku ponizej resetu)
bool         g_windUnsafeLatched  = false;
bool         g_cloudUnsafeLatched = false;
unsigned long g_cloudOvercastSinceMs = 0; // od kiedy delta < OVERCAST (0 = nie)

enum class SkyCondition { UNKNOWN, CLEAR, SCATTERED, CLOUDY, CRITICAL };
SkyCondition g_skyCondition = SkyCondition::UNKNOWN;

static const char* skyConditionName(SkyCondition s) {
  switch (s) {
    case SkyCondition::CLEAR:     return "CZYSTE";
    case SkyCondition::SCATTERED: return "CZESC. CHMURY";
    case SkyCondition::CLOUDY:    return "ZACHMURZENIE";
    case SkyCondition::CRITICAL:  return "GESTE CHMURY";
    default:                      return "NIEZNANE";
  }
}
static const char* skyConditionIcon(SkyCondition s) {
  switch (s) {
    case SkyCondition::CLEAR:     return "*";
    case SkyCondition::SCATTERED: return "~";
    case SkyCondition::CLOUDY:    return "C";
    case SkyCondition::CRITICAL:  return "R";
    default:                      return "?";
  }
}

unsigned long lastSensorsRead = 0;

// Alpaca
uint32_t serverTransactionID = 1;
bool g_ocConnected   = false;
bool g_domeConnected = false;
float g_averagePeriod = 0.0f;

unsigned long lastUpdate_Temperature    = 0;
unsigned long lastUpdate_Humidity       = 0;
unsigned long lastUpdate_DewPoint       = 0;
unsigned long lastUpdate_SkyTemperature = 0;
unsigned long lastUpdate_CloudCover     = 0;
unsigned long lastUpdate_Pressure       = 0;
unsigned long lastUpdate_WindSpeed      = 0;
unsigned long lastUpdate_RainRate       = 0;

static bool weatherThermoReady() {
 const uint32_t now=millis();
 return isfinite(g_tAir) && isfinite(g_hAir) && isfinite(g_dewPoint) &&
  g_hAir>=0 && g_hAir<=100 && lastUpdate_Temperature && lastUpdate_Humidity && lastUpdate_DewPoint &&
  uint32_t(now-lastUpdate_Temperature)<=10000UL && uint32_t(now-lastUpdate_Humidity)<=10000UL &&
  uint32_t(now-lastUpdate_DewPoint)<=10000UL;
}
static bool policyMoistureBlocked() {
 return ((g_weatherPolicy.humidity || g_weatherPolicy.dew) && !weatherThermoReady()) ||
  (g_weatherPolicy.humidity && g_moisture.humidLatched) || (g_weatherPolicy.dew && g_moisture.dewLatched);
}
static bool policyWindReady(){return !g_weatherPolicy.wind || (ANEMOMETER_ENABLED && windMeasurementUsable());}
static bool policySkyReady(){return !g_weatherPolicy.clouds || g_mlxSamples[0].usable(millis()) || g_mlxSamples[1].usable(millis());}
static bool weatherPolicyStore(const astro_weather::Policy& candidate) {
 uint8_t bytes[astro_weather::BYTES]={},verify[astro_weather::BYTES]={};
 astro_weather::encode(candidate,bytes);Preferences prefs;
 if(!prefs.begin("weatherpol",false)){g_weatherPolicyFault=true;g_weatherOpenReady=false;g_weatherStable.timing=false;return false;}
 bool ok=prefs.putBytes("policy",bytes,sizeof(bytes))==sizeof(bytes) &&
  prefs.getBytesLength("policy")==sizeof(bytes) && prefs.getBytes("policy",verify,sizeof(verify))==sizeof(verify);
 prefs.end();
 astro_weather::Policy stored;
 ok=ok && !memcmp(bytes,verify,sizeof(bytes)) && astro_weather::decode(verify,sizeof(verify),stored);
 if(ok){g_weatherPolicy=stored;g_weatherPolicyFault=false;g_weatherStable.timing=false;g_weatherOpenReady=false;}
 else {g_weatherPolicyFault=true;g_unsafe=true;g_reason="BLAD ZAPISU POLITYKI POGODY";}
 return ok;
}
static void weatherPolicyLoad() {
 Preferences prefs;
 // Absence is an upgrade/first-boot path; malformed existing data is a fault.
 if(!prefs.begin("weatherpol",false)){g_weatherPolicyFault=true;g_weatherPolicy=astro_weather::Policy{};return;}
 const bool exists=prefs.isKey("policy");const size_t length=prefs.getBytesLength("policy");
 uint8_t bytes[astro_weather::BYTES]={};astro_weather::Policy stored;
 if(exists && (length!=sizeof(bytes) || prefs.getBytes("policy",bytes,sizeof(bytes))!=sizeof(bytes) ||
    !astro_weather::decode(bytes,sizeof(bytes),stored)))g_weatherPolicyFault=true;
 else if(exists)g_weatherPolicy=stored;
 prefs.end();
 if(g_weatherPolicyFault)g_weatherPolicy=astro_weather::Policy{};
}
static bool safetyInputsUsable() {
  const uint32_t now=millis();
  return ROOF_MOTION_ENABLED && modbusRelayOK && !g_mountEstop && roofState!=RoofState::ERROR &&
      uint32_t(now-lastSensorsRead)<=3000UL &&
      policySkyReady() && policyWindReady() && !policyMoistureBlocked() && !g_weatherPolicyFault;
}

// ============== Alert tracking (zeby nie spamowac) ==============
bool g_alertRainSent   = false;
bool g_alertUnsafeSent = false;
bool g_alertWindSent   = false;
unsigned long g_lastAlertMs = 0;
const unsigned long ALERT_COOLDOWN_MS = 900000UL; // 15 min miedzy powtorkami alertow (antyspam)
unsigned long g_lastWindAlertMs = 0;              // cooldown takze dla alertu wiatru

// ============== Helpers ==============
static String fmtFloat(float v, uint8_t dec = 1) {
  if (isnan(v)) return "BRAK";
  return String(v, (unsigned int)dec);
}

static float calculateDewPoint(float t, float h) {
  if (isnan(t) || isnan(h) || h <= 0.0f) return NAN;
  const float a = 17.27f;
  const float b = 237.7f;
  float alpha = ((a * t) / (b + t)) + log(h / 100.0f);
  return (b * alpha) / (a - alpha);
}

static const char* roofStateName(RoofState s) {
  switch (s) {
    case RoofState::UNKNOWN: return "UNKNOWN";
    case RoofState::OPEN:    return "OPEN";
    case RoofState::CLOSED:  return "CLOSED";
    case RoofState::OPENING: return "OPENING";
    case RoofState::CLOSING: return "CLOSING";
    case RoofState::STOPPED: return "STOPPED";
    case RoofState::ERROR:   return "ERROR";
  }
  return "?";
}

// ════════════════════════════════════════════════════════════════════════════
//  ILUMINATOR OBSERWATORIUM :: TCA9554 DO8 -> driver MOSFET, ON/OFF
//  ----------------------------------------------------------------------------
//  Swiatlo czerwone jest sterowane RECZNIE przez WWW.
//  Nie ma automatycznego auto-off i nie jest wymagany stan CLOSED.
//  Jedyny automatyczny OFF: rozpoczecie ruchu dachu / STOP dachu.
//  Po zakonczeniu ruchu uzytkownik moze ponownie wlaczyc swiatlo recznie.
// ════════════════════════════════════════════════════════════════════════════
#ifndef ILLUM_DO_ENABLED
  #define ILLUM_DO_ENABLED 1
#endif
#ifndef ILLUM_DO_CHANNEL
  #define ILLUM_DO_CHANNEL 7  // TCA9554 liczy od 0; channel 7 = fizyczne DO8
#endif
#ifndef ILLUM_DO_ACTIVE_HIGH
  #define ILLUM_DO_ACTIVE_HIGH 1
#endif
#if ILLUM_DO_CHANNEL < 0 || ILLUM_DO_CHANNEL > 7
  #error "ILLUM_DO_CHANNEL musi byc w zakresie 0..7 (TCA9554)"
#endif
bool g_illumRequested = false;
bool g_illumOutputOn = false;

// Wyjscie ON/OFF na TCA9554 DO8 (driver MOSFET) - logika bez zmian wzgledem v17.9.
static bool illumOutputDo8(bool on) {
#if ILLUM_DO_ENABLED
  if (!tcaOK) {
    if (!on) g_illumOutputOn = false;
    return false;
  }
  const bool levelHigh = on ? (ILLUM_DO_ACTIVE_HIGH != 0) : (ILLUM_DO_ACTIVE_HIGH == 0);
  if (!tca.digitalWrite((uint8_t)ILLUM_DO_CHANNEL, levelHigh ? HIGH : LOW)) {
    tcaOK = false;
    g_illumOutputOn = false;
    Serial.println(F("[ILLUM] Blad zapisu TCA9554 DO8; stan wyjscia niepotwierdzony"));
    return false;
  }
  g_illumOutputOn = on;
  return true;
#else
  (void)on;
  g_illumOutputOn = false;
  return false;
#endif
}

// Dotychczasowe swiatlo lokalne TCA9554 DO8. Czerwona tasma PWM CH4 ma osobne sterowanie.
static bool illumAvailable() {
  bool any = false, ok = true;
#if ILLUM_DO_ENABLED
  any = true; ok = ok && tcaOK;
#endif
  return any && ok;
}

// Ustawia wyłącznie lokalne wyjście TCA9554.
static bool illumOutput(bool on) {
  bool any = false, ok = true;
#if ILLUM_DO_ENABLED
  any = true;
  if (!illumOutputDo8(on)) ok = false;
#endif
  g_illumOutputOn = (any && ok) ? on : false;
  return any && ok;
}

static void illumForceOff(const char* reason = nullptr) {
  const bool wasOn = g_illumRequested || g_illumOutputOn;
  g_illumRequested = false;
  (void)illumOutput(false);
  g_illumOutputOn = false;
  if (wasOn && reason) Serial.printf("[ILLUM] OFF: %s\n", reason);
}

void illumSetup() {
  g_illumRequested = false;
  g_illumOutputOn = false;
#if ILLUM_DO_ENABLED
  if (!illumAvailable()) {
    Serial.println(F("[ILLUM] OFFLINE - lokalne swiatlo TCA9554 niedostepne; swiatlo zablokowane"));
    return;
  }
  if (illumOutput(false))
    Serial.printf("[ILLUM] TCA9554 DO8 gotowe (start OFF)\n");
  else
    Serial.println(F("[ILLUM] Blad ustawienia OFF"));
#else
  Serial.println(F("[ILLUM] TCA9554 DO8 disabled (ILLUM_DO_ENABLED=0)"));
#endif
}

// Steruje niezależnie lokalnym wyjściem TCA9554; ON wymaga dostępnego układu.
bool illumRequest(bool on) {
  if (!on) {
    g_illumRequested = false;
    (void)illumOutput(false);
    return true;
  }
  if (!illumAvailable()) {
    g_illumRequested = false;
    g_illumOutputOn = false;
    Serial.println(F("[ILLUM] Zadanie ON odrzucone - wyjscie swiatla niedostepne"));
    return false;
  }
  if (!illumOutput(true)) {
    g_illumRequested = false;
    (void)illumOutput(false);   // wycofaj czesciowe wlaczenie (np. DO8 ON, PWM blad)
    return false;
  }
  g_illumRequested = true;
  return true;
}

// Wolaj w loop(); brak automatycznego timeoutu.
// Automatyczny OFF jest wykonywany tylko przez funkcje rozpoczecia ruchu/STOP dachu.
void illumTick() {
  // Celowo puste: czerwone swiatlo ma pozostac wlaczone tak dlugo,
  // jak uzytkownik tego potrzebuje.
}
// ═══ /ILUMINATOR ════════════════════════════════════════════════════════════

static bool limitClosedActive() { return digitalRead(LIMIT_CLOSED_PIN) == LOW; }
static bool limitOpenActive()   { return digitalRead(LIMIT_OPEN_PIN)   == LOW; }

// ═══ WAVESHARE 17658 — KANAŁY 0..7 VIA MODBUS RTU ═══
// Relay channel numbers are zero-based here: 0..7 correspond to physical CH1..CH8.
static bool relayDeviceReady(uint8_t ch) { return ch <= 7 && modbusRelayOK; }

static bool relayWrite(uint8_t ch, bool on) {
  if (ch > 7 || (on && !modbusRelayOK) || (on && ch < 3 && !ROOF_MOTION_ENABLED)) return false;
  const bool ok = modbusRelayWriteCoil(ch, on);
  if (ok) relayState[ch] = on;
  else {
    modbusRelayOK = false;
    modbusRelayStateValid = false;
  }
#if RELAY_LOG
  Serial.printf("[RELAY] CH%u on=%d -> WAVESHARE MODBUS %s\n", ch + 1, (int)on, ok ? "OK" : "ERROR");
#endif
  return ok;
}

static bool relayPulse(uint8_t ch, unsigned long ms = 800) {
  if (ch > 7 || !relayDeviceReady(ch)) return false;
  // An absent echo does not prove that the coil stayed OFF.
  pulseEndTime[ch] = millis() + ms;
  pulseActive[ch] = true;
  const bool confirmed=relayWrite(ch, true);
  if (!confirmed) pulseEndTime[ch]=millis(); // release immediately; never repeat ON
  if (!confirmed) return false;
  return true;
}

static void roofRelaysOff() {
  // OFF remains best-effort even after a previous write failed; try each channel.
  const uint8_t channels[]={RELAY_ROOF_OPEN,RELAY_ROOF_CLOSE,RELAY_ROOF_STOP};
  for (const uint8_t ch : channels) {
    const bool off=relayWrite(ch,false);
    pulseActive[ch]=!off;
    pulseEndTime[ch]=off ? 0 : millis()+250UL;
    if(ch==RELAY_ROOF_STOP) g_stopReleaseConfirmed=off;
  }
}

// ── Zasilanie czujnika kolizji NPN NC przez dedykowany przekaznik ────────
// collisionPowerOn():  wlacza zasilanie czujnika przed ruchem dachu.
// collisionPowerOff(): odcina zasilanie po zakonczeniu ruchu.
//   NPN NC bez zasilania -> wyjscie HIGH -> odczyt "brak przeszkody" -> bezpieczne.
// Obie funkcje sa puste (no-op) gdy COLLISION_POWER_RELAY_ENABLED=0.
static void collisionPowerOn() {
#if COLLISION_SENSOR_ENABLED && COLLISION_POWER_RELAY_ENABLED
  relayWrite((uint8_t)COLLISION_POWER_RELAY_CH, true);
  Serial.println(F("[COLLISION PWR] ON"));
#endif
}

static void collisionPowerOff() {
#if COLLISION_SENSOR_ENABLED && COLLISION_POWER_RELAY_ENABLED
  relayWrite((uint8_t)COLLISION_POWER_RELAY_CH, false);
  g_collisionSensorActive  = false;  // reset: czujnik bez zasilania = nieaktywny
  g_collisionSensorRawLast = false;
  Serial.println(F("[COLLISION PWR] OFF"));
#endif
}

static bool blockRoofCommand(const String& reason) {
  g_lastRoofBlockReason = reason;
  Serial.print(F("!! ROOF COMMAND BLOCKED: "));
  Serial.println(reason);
  eventLogEx("WARN", "ROOF", "BLOCKED: %s", reason.c_str());
  return false;
}

static void updatePulseOutputs() {
  const uint32_t now = millis();
  const uint8_t order[]={RELAY_ROOF_STOP,RELAY_ROOF_OPEN,RELAY_ROOF_CLOSE,3,4,5,6,7,8,9,10,11,12,13,14,15};
  for (uint8_t i : order) {
    if (pulseActive[i] && astro_roof::due(now, (uint32_t)pulseEndTime[i])) {
      if (relayWrite(i, false)) {
        pulseActive[i]=false; pulseEndTime[i]=0;
        if (i==RELAY_ROOF_STOP) g_stopReleaseConfirmed=true;
      } else {
        // Failed release is not silently discarded. Bounded retry, including
        // when the cached module status is offline; OFF remains best effort.
        pulseEndTime[i]=millis()+250UL;
        if (i==RELAY_ROOF_STOP) {
          g_stopReleaseConfirmed=false;
          g_lastRoofBlockReason="STOP: zwolnienie przekaznika niepotwierdzone";
        }
      }
    }
  }
}

// filtr czasowy na "obie krancowki naraz". Zaklocenie elektryczne (dlugie kable
// krancowek biegnace obok zasilania napedu) trwa mikro-/milisekundy, prawdziwe zwarcie
// lub uszkodzenie czujnika trwa dalej. Wymagamy, by stan c&&o utrzymal sie nieprzerwanie
// przez LIMIT_BOTH_DEBOUNCE_MS, zanim zglosimy AWARIE. Opoznienie ~250 ms jest bez
// znaczenia dla bezpieczenstwa (dach rusza sekundami), a kasuje falszywe alarmy.
#ifndef LIMIT_BOTH_DEBOUNCE_MS
  #define LIMIT_BOTH_DEBOUNCE_MS 250UL
#endif

// STOP has priority over auxiliary outputs and cached Modbus health.
// A valid Modbus echo confirms only the electrical command, not physical rest.
static bool stopDrivePulse(unsigned long duration=300) {
  ++g_stopEpoch;
  g_stopAttempted=true;
  g_stopCommandConfirmed=false;
  if (!ROOF_MOTION_ENABLED) {
    g_lastRoofBlockReason="STOP: ruch dachu wylaczony w profilu";
    return false;
  }
  pulseActive[RELAY_ROOF_STOP]=true;
  pulseEndTime[RELAY_ROOF_STOP]=millis()+duration;
  g_stopReleaseConfirmed=false;
  const bool ok=modbusRelayWriteCoil(RELAY_ROOF_STOP,true);
  g_stopCommandConfirmed=ok;
  if (ok) {
    modbusRelayOK=true; relayState[RELAY_ROOF_STOP]=true;
  } else {
    modbusRelayOK=false; modbusRelayStateValid=false;
    g_lastRoofBlockReason="STOP NIEPOTWIERDZONY - stan napedu nieznany";
  }
  // Do not turn STOP off here. Cancel direction inputs after asserting STOP.
  const uint8_t channels[]={RELAY_ROOF_OPEN,RELAY_ROOF_CLOSE};
  for (const uint8_t ch : channels) {
    const bool off=relayWrite(ch,false);
    pulseActive[ch]=!off;
    pulseEndTime[ch]=off ? 0 : millis()+250UL;
  }
  illumForceOff("STOP dachu");
  eventLogEx(ok?"WARN":"CRITICAL","ROOF",ok?
      "STOP command echoed; physical standstill not measured":
      "STOP UNCONFIRMED; drive state unknown");
  return ok;
}
static void roofFaultStop(const String& reason, RoofFaultKind kind=RoofFaultKind::PHYSICAL) {
  const bool confirmed=stopDrivePulse(500);
  roofState=RoofState::ERROR;
  g_roofFaultKind=confirmed ? kind : RoofFaultKind::STOP_UNCONFIRMED;
  g_roofMotionFault=reason;
  if(!confirmed) g_roofMotionFault += "; STOP NIEPOTWIERDZONY - stan napedu nieznany";
  g_roofStuckChecked=true;
  collisionPowerOff();
  eventLogEx("CRITICAL","ROOF","%s",g_roofMotionFault.c_str());
  notifyEvent(NOTIFY_CRITICAL,"DACH",g_roofMotionFault.c_str());
#if USE_TELEGRAM
  telegramSend(g_roofMotionFault,true);
#endif
}

static void updateRoofStateFromLimits() {
  static bool bothFaultAlerted = false;
  static unsigned long bothSinceMs = 0;      // od kiedy obie aktywne (0 = nie)
  bool c = limitClosedActive();
  bool o = limitOpenActive();
  if (c && o) {
    // najpierw filtr czasowy - krotki glitch NIE jest awaria.
    if (bothSinceMs == 0) bothSinceMs = millis();
    if (millis() - bothSinceMs < LIMIT_BOTH_DEBOUNCE_MS) {
      return;   // okno obserwacji: nie ufamy odczytowi, ale tez nie alarmujemy
    }
    // fizycznie niemozliwe (dach nie jest jednoczesnie otwarty i zamkniety)
    // => usterka/zwarcie krancowki. Zatrzymaj naped i alarmuj raz, nie ufaj pozycji.
    if (!bothFaultAlerted) {
      bothFaultAlerted = true;
      roofFaultStop("USTERKA KRANCOWEK: obie aktywne naraz (zwarcie/uszkodzenie czujnika)");
    }
    return;
  }
  bothSinceMs = 0;            // warunek znikl -> wyzeruj licznik filtra
  // Ustapienie sygnalu nie potwierdza sprawnosci napedu.
  // Kazdy blad ruchu pozostaje zatrzasniety do kontroli i swiadomego restartu.
  // Fault remains latched until inspection/restart; no automatic clearing.
  bothFaultAlerted = false;   // usterka ustapila -> pozwol ponownie zaalarmowac w przyszlosci
  if (roofState == RoofState::ERROR && g_roofMotionFault.length()) return;
  roofState=astro_roof::fromLimits(roofState,c,o);
}

// ============== Park Guard ==============
static void updateParkStatus() {
  unsigned long now = millis();
  if (now - g_lastParkCheck < PARK_CHECK_INTERVAL_MS) return;
  g_lastParkCheck = now;

  bool prevParked = g_telescopeParked;

#if PARK_GUARD_ENABLED
  // ── Warstwa 1: Sprzet - DWA reed switche (RA + DEC). Park = OBIE osie ──
  // kazda os osobny pin. Park tylko gdy OBA piny LOW jednoczesnie.
  g_parkRa  = (digitalRead(PARK_SENSOR_PIN)     == LOW);   // os RA
  g_parkDec = (digitalRead(PARK_SENSOR_DEC_PIN) == LOW);   // os DEC
  bool hwParked = (g_parkRa && g_parkDec);
  if (hwParked) g_parkSensorOK = true;                     // widzielismy obie osie w parku
  g_telescopeParked = hwParked;

  // ── Warstwa 2: Alpaca HTTP - pytamy NINE/APCC o status parku ────────────
  // Pytamy ZAWSZE (nie tylko gdy HW nie widzi parku) - podwojne potwierdzenie
  // Jesli HW mowi park=true a Alpaca mowi park=false -> UNSAFE (bezpieczniej)
  // Jesli HW niedostepny (brak reed switcha) -> polegamy na Alpaca
#if USE_WIFI
  // BACKOFF zapytania Alpaca. Reed switche (wyzej) czytamy ZAWSZE co 5 s.
  // Ale gdy PC montazu nie odpowiada (>=ALPACA_MAX_FAILS pudel), nie meczymy
  // jednowatkowej petli co 5 s - pytamy Alpaca co 30 s. Merge logika nizej i tak
  // traktuje brak Alpaca jako "polegaj na HW", wiec BEZPIECZENSTWO BEZ ZMIAN;
  // to tylko mniej zamulania WWW/reakcji, gdy komputer spi/offline.
  static unsigned long g_alpacaLastTryMs = 0;
  const unsigned long  ALPACA_BACKOFF_MS = 30000UL;
  bool alpacaDue = (g_alpacaFailCount < ALPACA_MAX_FAILS) ||
                   (g_alpacaLastTryMs == 0) ||
                   (now - g_alpacaLastTryMs >= ALPACA_BACKOFF_MS);
  if (strlen(PARK_ALPACA_HOST) > 0 && g_wifiReady &&
      WiFi.status() == WL_CONNECTED && alpacaDue && !astro_roof::moving(roofState)) {
    g_alpacaLastTryMs = now;
    g_parkAlpacaAttempted=true;
    g_parkAlpacaAttemptMs=now;
    WiFiClient alpacaClient;
    // WiFiClient uses SECONDS, not milliseconds. Do not permit a 600-second socket wait.
    alpacaClient.setTimeout(1);
    bool alpacaParked = false;
    bool alpacaOk = false;

    // ograniczony timeout connect() (~400 ms). Domyslny (kilka s) blokowal
    // JEDNOWATKOWA petle gdy PC montazu spi/offline -> stad wolniejsze WWW i reakcje
    // ESP przy PARK_GUARD_ENABLED=1. Teraz najwyzej ~400 ms na nieudana probe.
    IPAddress parkAddress;
    // A literal IPv4 avoids an unbounded synchronous DNS lookup in the control loop.
    diagnosticStage(241);
    if (parkAddress.fromString(PARK_ALPACA_HOST) && alpacaClient.connect(parkAddress, PARK_ALPACA_PORT, 400)) {
      diagnosticStage(242);
      const String request=String("GET /api/v1/telescope/0/atpark HTTP/1.0\r\nHost: ")+PARK_ALPACA_HOST+
        "\r\nConnection: close\r\nUser-Agent: ESP32-Observatory/1.0\r\n\r\n";
      // One nonblocking socket send: WiFiClient::print can retry for 10 s per call.
      const int sent=::send(alpacaClient.fd(),request.c_str(),request.length(),MSG_DONTWAIT);
      const bool requestSent=sent==(int)request.length();
      unsigned long t0 = millis();
      String resp = ""; resp.reserve(601);
      diagnosticStage(243);
      while (requestSent && millis() - t0 < 600 && (alpacaClient.connected() || alpacaClient.available())) {
        unsigned budget=64;
        while (budget-- && alpacaClient.available() && millis()-t0<600 && resp.length()<600) {
          const int ch=alpacaClient.read();if(ch<0)break;resp+=(char)ch;
        }
        if (resp.length()>=600) break;
        delay(1); // Let idle tasks run; taskYIELD alone does not guarantee this.
      }
      diagnosticStage(244);
      alpacaClient.stop();

      String lower = resp;
      lower.toLowerCase();
      // Sprawdz czy odpowiedz jest prawidlowa (kod 200 i brak bledu)
      lower.replace(" ","");
      if (lower.indexOf("200ok") >= 0 && lower.indexOf("\"errornumber\":0") >= 0 &&
          (lower.indexOf("\"value\":true")>=0 || lower.indexOf("\"value\":false")>=0)) {
        alpacaOk = true;
        // FIX BUG5: NINA serializuje JSON ze spacją po dwukropku: "Value": true
        // Sprawdzamy obie formy: z i bez spacji
        alpacaParked = (lower.indexOf("\"value\":true") >= 0 ||
                        lower.indexOf("\"value\": true") >= 0);
        g_alpacaFailCount = 0;
        if (alpacaParked) g_parkSensorOK = true;
      } else {
        if (g_alpacaFailCount < 255) g_alpacaFailCount++;
      }
    } else {
      if (g_alpacaFailCount < 255) g_alpacaFailCount++;
    }

    // Logika laczenia HW + Alpaca:
    g_parkAlpacaLastAttemptOK=alpacaOk;
    if(alpacaOk) {
      g_parkAlpacaSuccessSeen=true;
      g_parkAlpacaSuccessMs=millis();
      g_parkAlpacaLastValue=alpacaParked;
    }
    if (g_parkSensorOK && alpacaOk) {
      // Mamy oba zrodla - wymagamy ZE OBA mowia park=true (AND)
      // To jest najbezpieczniejsza opcja dla AP900
      g_telescopeParked = hwParked && alpacaParked;
      if (hwParked && !alpacaParked) {
        Serial.println(F("[PARK] ROZNICA: HW=park, Alpaca=NIEpark -> UNSAFE"));
      }
    } else if (alpacaOk && !g_parkSensorOK) {
      // Brak reed switcha - polegamy tylko na Alpaca
      g_telescopeParked = alpacaParked;
    } else if (g_parkSensorOK && !alpacaOk) {
      // Alpaca niedostepna - polegamy tylko na HW (reed switch)
      g_telescopeParked = hwParked;
      if (g_alpacaFailCount >= ALPACA_MAX_FAILS) {
        Serial.printf("[PARK] Alpaca niedostepna (%d bledow) - tylko HW\n", g_alpacaFailCount);
      }
    } else {
      // Brak obu zrodel - UNSAFE (nie wiemy gdzie jest teleskop!)
      g_telescopeParked = false;
      Serial.println(F("[PARK] BRAK danych (HW i Alpaca) -> UNSAFE"));
    }

    Serial.printf("[PARK] hw=%d alpacaOk=%d alpacaParked=%d -> parked=%d fails=%d\n",
                  (int)hwParked, (int)alpacaOk, (int)alpacaParked,
                  (int)g_telescopeParked, g_alpacaFailCount);
  }
#endif

#else
  // Park guard wylaczony - zawsze "zaparkowany"
  g_telescopeParked = true;
  g_parkSensorOK    = false;
#endif

  // ── Wykrywanie momentu zaparkowania (NIE->TAK) ─────────────────────
  if (!prevParked && g_telescopeParked) {
    g_lastParkedAtMs   = millis();
    g_showParkAnimation = true;
    Serial.println(F("[PARK] *** TELESKOP ZAPARKOWAL! ***"));
#if USE_TELEGRAM
    telegramSend("PARK potwierdzony. Pozostale blokady ruchu dachu nadal obowiazuja; to nie jest zgoda na ruch ani potwierdzenie zamkniecia.");
#endif
  }

  // Wygaszenie animacji po czasie
  if (g_showParkAnimation &&
      (millis() - g_lastParkedAtMs > PARK_ANIM_DURATION_MS)) {
    g_showParkAnimation = false;
  }

  // ── Wykrywanie ruchu (TAK->NIE) - teleskop ruszyl sie z parku ──────
  if (prevParked && !g_telescopeParked) {
    Serial.println(F("[PARK] !! Teleskop opuscil pozycje park!"));
    // Alarm tylko podczas ruchu dachu; normalne sledzenie przy OPEN jest dozwolone.
    //     (niezalezne od USE_TELEGRAM - ma dojsc zawsze)
    if (astro_roof::parkRequiredDuringMotion(roofState,CLOSE_CLEARANCE_CONFIRMED)) {
      notifyEvent(NOTIFY_CRITICAL, "PARK", "Teleskop opuscil park podczas ruchu dachu!");
#if USE_TELEGRAM
      telegramSend("UWAGA: Teleskop opuscil park podczas ruchu dachu! Sprawdz montaz!", true);
#endif
    }
    // ═══ /═══
  }
}

// Czy mozna bezpiecznie zamknac dach?
// Zwraca true jesli park guard nieaktywny lub teleskop zaparkowany
static astro_park::StablePark g_stablePark;
static void sampleHardwarePark() {
#if PARK_GUARD_ENABLED
  g_stablePark.sample(digitalRead(PARK_SENSOR_PIN) == LOW,
                      digitalRead(PARK_SENSOR_DEC_PIN) == LOW, (uint32_t)millis());
#endif
}
static bool parkGuardAllowsClose() {
#if PARK_GUARD_ENABLED
  sampleHardwarePark();
  return astro_roof::parkAllows(g_manualMode,g_telescopeParked, g_stablePark.ready((uint32_t)millis(), PARK_SETTLE_MS));
#else
  return true;
#endif
}

// Operator-confirmed geometry permits CLOSE without a park report or a PC.
// Never used for OPEN or acknowledgement of a communication fault.
static bool clearanceAllowsClose() {
  return astro_roof::closeParkAllows(CLOSE_CLEARANCE_CONFIRMED,parkGuardAllowsClose());
}

static bool mountLimitRawTripped();
static bool mountProtectionBlocksClose() {
  return astro_policy::mountBlocksClose(CLOSE_CLEARANCE_CONFIRMED,MOUNT_ESTOP_BLOCKS_CLOSE,
                                        g_mountEstop || mountLimitRawTripped());
}

// ============== Forward declarations ==============
static void roofTick();
static bool mountLimitRawTripped();
static void mountEstopTick();
static void serviceRoofFast();  // używana w roofOpen/roofClose (definicja na koncu)
static void httpUrgentTick();

// ============== Roof control ==============
// Dense-cloud protection intentionally has no guessed calibration defaults.
static astro_roof::DenseCloud g_denseCloud;
static bool hardWeatherBlocksOpen() {
  bool rainNow=g_rain;
#if ENABLE_RG11
  rainNow=rainNow || (RG11_RAIN_WHEN_LOW ? digitalRead(RG11_PIN)==LOW : digitalRead(RG11_PIN)==HIGH);
#endif
  return astro_weather::hardBlock(g_weatherPolicy,rainNow,g_windUnsafeLatched,g_denseCloud.latched,policyMoistureBlocked()) || g_weatherPolicyFault || (g_weatherPolicy.rain && !ENABLE_RG11);
}
static bool weatherCloseRequested() {
  return astro_roof::closeForWeather(g_manualMode,hardWeatherBlocksOpen(),
    (g_weatherPolicy.clouds && g_cloudUnsafeLatched) || uint32_t(millis()-lastSensorsRead)>3000UL ||
    !policySkyReady() || !policyWindReady() ||
    (COMMS_WATCHDOG_ENABLED && COMMS_LOST_AUTOPARK && g_commsLost));
}

static bool roofOpen() {
  g_lastRoofBlockReason = "";
  const uint32_t commandEpoch=g_stopEpoch;
  if (!modbusRelayOK) return blockRoofCommand("BRAK MODBUS RELAY - przekazniki dachu offline");
  if (limitClosedActive() && limitOpenActive())
    return blockRoofCommand("Obie krancowki dachu aktywne - ruch zablokowany");

  // AUTO requires an active SafetyMonitor; MANUAL is explicit operator control.
  if (!astro_roof::weatherAllowsOpen(g_manualMode,true,ninaIsActive())) {
    return blockRoofCommand("NINA nieaktywna - otwieranie zablokowane. Uruchom NINA i SafetyMonitor/Alpaca.");
  }
  // ═════════════════════════════════════════════════════════════════════════

  // ═══ TWARDA BLOKADA POGODOWA (KRYTYCZNA) ═══════════════════
  // Do tej pory otwarcie przy UNSAFE blokowal TYLKO JavaScript w przegladarce.
  // To zadna ochrona: stara zakladka, wpisany recznie /cmd?do=open, blad w UI
  // albo zdalny klient moglby otworzyc dach W DESZCZ. Teraz blokuje sam ESP.
  // MANUAL omija zwykle chmury/NINA; wiarygodna pogoda i interlocki pozostaja.
  if (!g_manualMode && !g_weatherOpenReady) return blockRoofCommand("Pogoda: trwa wymagany okres stabilnej poprawy");
  if (hardWeatherBlocksOpen()) return blockRoofCommand("DESZCZ / dry-hold, WIATR lub BARDZO GESTE ZACHMURZENIE");
  if (!astro_policy::weatherMeasurementsReady(uint32_t(millis()-lastSensorsRead)<=3000UL,
      policyWindReady(),
      policySkyReady()))
    return blockRoofCommand("Pogoda: dane nieaktualne, anemometr lub oba MLX niewiarygodne");
  if (!astro_roof::weatherAllowsOpen(g_manualMode,!g_unsafe,true)) {
    return blockRoofCommand("UNSAFE (" + g_reason + ") - otwieranie ZABLOKOWANE. Wlacz MANUAL, jesli swiadomie chcesz otworzyc.");
  }
  // ═══════════════════════════════════════════════════════════════════════

#if MOUNT_LIMIT_ENABLED
  // E-STOP montazu zatrzasniety -> nie otwieramy dachu nad montazem poza obwiednia
  if (g_mountEstop || mountLimitRawTripped()) return blockRoofCommand("E-STOP MONTAZU aktywny - otwieranie zablokowane. Zresetuj po sprawdzeniu montazu.");
#endif
#if USE_OTA && USE_WIFI
  if (g_otaActive) return blockRoofCommand("Trwa aktualizacja OTA - ruch dachu zablokowany");
#endif
  updateRoofStateFromLimits();
  if (roofState==RoofState::CLOSING)
    return blockRoofCommand("Dach jest w trakcie zamykania - najpierw STOP");
#if ROOF_BLOCK_REDUNDANT_COMMANDS
  if (roofState == RoofState::ERROR)
    return blockRoofCommand("BLAD dachu: sprawdz instalacje przed ponownym ruchem");
  if (roofState == RoofState::OPEN || limitOpenActive()) {
    roofState = RoofState::OPEN;
    return true; // Idempotent OPEN: potwierdzona krancowka, bez impulsu.
  }
  if (roofState == RoofState::CLOSING) {
    return blockRoofCommand("Dach jest w trakcie zamykania - najpierw STOP");
  }
#endif

  if (roofState == RoofState::ERROR)
    return blockRoofCommand("BLAD dachu: sprawdz instalacje przed ponownym ruchem");
  if (roofState == RoofState::OPENING) return true; // Bez kolejnego impulsu.

  // ── OPEN GUARD: sprawdz park przed otwarciem ───────────────────────────
  // Nie otwieraj dachu jesli nie wiemy gdzie jest teleskop (AP900 safety)
  // MANUAL i aliasy force nadal wymagaja stabilnego parku RA/DEC
#if PARK_GUARD_ENABLED
  if (!parkGuardAllowsClose()) {
    String reason = "OPEN GUARD: teleskop NIE jest w parku";
    if (!g_parkSensorOK && strlen(PARK_ALPACA_HOST) == 0) {
      // FAIL-SAFE: bez wiarygodnego potwierdzenia pozycji montazu nie wolno otworzyc dachu.
      Serial.println(F("[OPEN] BRAK danych o parku - otwieranie ZABLOKOWANE"));
#if USE_TELEGRAM
      telegramSend("BLOKADA: Otwieranie dachu - brak wiarygodnego potwierdzenia pozycji AP900!");
#endif
      return blockRoofCommand(reason + " - brak wiarygodnego potwierdzenia parku");
    } else {
      // Mamy czujnik lub Alpaca i mowia ze NIE jest w parku
#if USE_TELEGRAM
      telegramSend("BLOKADA: Otwieranie dachu zablokowane - AP900 nie w parku!");
#endif
      return blockRoofCommand(reason);
    }
  }
#endif

if (!collisionClearForRoofMove()) return blockRoofCommand(g_lastRoofBlockReason);

  if (pulseActive[RELAY_ROOF_STOP])
    return blockRoofCommand("STOP: odczekaj na zwolnienie impulsu zatrzymania");
  // Ruch dachu jest dozwolony tylko przy zgaszonym oswietleniu.
  illumForceOff("rozpoczecie ruchu dachu");
  // Wlacz zasilanie czujnika NPN NC i odczekaj na inicjalizacje przed ruchem
  collisionPowerOn();
#if COLLISION_SENSOR_ENABLED && COLLISION_POWER_RELAY_ENABLED
  // FIX BUG3: zastapiono blocking delay() petla millis() zeby nie blokowac roofTick()
  { unsigned long _w = millis(); while (millis() - _w < (unsigned long)COLLISION_POWER_WARMUP_MS) { roofTick(); updatePulseOutputs(); yield(); } }
  // Sprawdz ponownie po warmup — czujnik teraz zasilony, odczyt wiarygodny
  if (!collisionClearForRoofMove()) {
    collisionPowerOff();
    return blockRoofCommand(g_lastRoofBlockReason);
  }
#endif

  if (!parkGuardAllowsClose())
    return blockRoofCommand("OPEN: park RA/DEC utracony przed impulsem");
#if ROOF_INTERLOCK_RELAYS
  roofRelaysOff();
#endif
  if (hardWeatherBlocksOpen() || !astro_policy::weatherMeasurementsReady(uint32_t(millis()-lastSensorsRead)<=3000UL,
      policyWindReady(),
      policySkyReady()))
    return blockRoofCommand("OPEN: deszcz/wiatr lub brak wiarygodnej pogody przed impulsem");
  if ((!g_manualMode && (!g_weatherOpenReady || g_unsafe)) || g_mountEstop || mountLimitRawTripped() || !collisionClearForRoofMove() ||
      (limitClosedActive() && limitOpenActive()) || !parkGuardAllowsClose() || hardWeatherBlocksOpen() ||
      pulseActive[RELAY_ROOF_OPEN] || pulseActive[RELAY_ROOF_CLOSE] || pulseActive[RELAY_ROOF_STOP])
    return blockRoofCommand("OPEN: blokada przed impulsem / OFF niepotwierdzone");
  if (roofState==RoofState::ERROR || commandEpoch!=g_stopEpoch)
    return blockRoofCommand("Ruch anulowany przez STOP lub blad w trakcie przygotowania");
  Serial.println(F(">> ROOF: OPEN (impuls)"));
  eventLogEx("INFO", "ROOF", "OPEN command accepted");
  if (!relayPulse(RELAY_ROOF_OPEN)) {
    roofFaultStop("OPEN niepotwierdzone przez Waveshare 17658",RoofFaultKind::COMMUNICATION);
    return blockRoofCommand(g_roofMotionFault);
  }
  roofState = RoofState::OPENING;
  roofMoveStartMs = millis();
  roofMotionWatchStart();
  return true;
}

static bool roofClose() {
  // TWARDY WARUNEK BEZPIECZENSTWA: ZAMKNIECIE JEST NIEZALEZNE OD NINA.
  // Nie sprawdzamy ninaIsActive() ani g_smConnected. Park zalezy od potwierdzonego przeswitu.
  // Jesli dach trzeba zamknac awaryjnie podczas utraty PC/NINA, ta sciezka
  // musi pozostac dostepna. Pozostaja tylko fizyczne/interlockowe blokady:
  // Modbus, konfiguracja przeswitu/PARK, OTA, kolizja i blokady samego dachu.
  g_lastRoofBlockReason = "";
  const uint32_t commandEpoch=g_stopEpoch;
  if (limitClosedActive() && limitOpenActive())
    return blockRoofCommand("Obie krancowki dachu aktywne - ruch zablokowany");
  if (!modbusRelayOK) {
    // ═══ Modbus relay offline - ESP nie moze bezpiecznie sterowac dachem ═══
    notifyEvent(NOTIFY_CRITICAL, "DACH", "Nie moge zamknac - Waveshare 17658 / Modbus offline!");
    return blockRoofCommand("BRAK MODBUS RELAY - przekazniki dachu offline");
  }
  updateRoofStateFromLimits();
  if (roofState==RoofState::OPENING)
    return blockRoofCommand("Dach jest w trakcie otwierania - najpierw STOP");
  if (roofState == RoofState::ERROR)
    return blockRoofCommand("BLAD dachu: sprawdz instalacje przed ponownym ruchem");
  if (mountProtectionBlocksClose())
    return blockRoofCommand("Ochrona montazu blokuje CLOSE bez zatwierdzonego przeswitu lub zgodnie z konfiguracja");
#if USE_OTA && USE_WIFI
  if (g_otaActive) return blockRoofCommand("Trwa aktualizacja OTA - ruch dachu zablokowany");
#endif
#if ROOF_BLOCK_REDUNDANT_COMMANDS
  if (roofState == RoofState::CLOSED || limitClosedActive()) {
    roofState = RoofState::CLOSED;
    return true; // Idempotent CLOSE: potwierdzona krancowka, bez impulsu.
  }
  if (roofState == RoofState::OPENING) {
    return blockRoofCommand("Dach jest w trakcie otwierania - najpierw STOP");
  }
#endif
  // CLOSE uses the installation clearance policy; other interlocks remain mandatory.
  // Utrata PC nie odbiera lokalnych czujnikow RA/DEC.
  if (!clearanceAllowsClose())
    return blockRoofCommand("CLOSE: brak potwierdzonego przeswitu i stabilnego parku RA/DEC");
  if (roofState == RoofState::CLOSING) return true; // Bez kolejnego impulsu.
  if (!collisionClearForRoofMove()) return blockRoofCommand(g_lastRoofBlockReason);

  if (pulseActive[RELAY_ROOF_STOP])
    return blockRoofCommand("STOP: odczekaj na zwolnienie impulsu zatrzymania");
  // Ruch dachu jest dozwolony tylko przy zgaszonym oswietleniu.
  illumForceOff("rozpoczecie ruchu dachu");
  // Wlacz zasilanie czujnika NPN NC i odczekaj na inicjalizacje przed ruchem
  collisionPowerOn();
#if COLLISION_SENSOR_ENABLED && COLLISION_POWER_RELAY_ENABLED
  // FIX BUG3: zastapiono blocking delay() petla millis() zeby nie blokowac roofTick()
  { unsigned long _w = millis(); while (millis() - _w < (unsigned long)COLLISION_POWER_WARMUP_MS) { roofTick(); updatePulseOutputs(); yield(); } }
  // Sprawdz ponownie po warmup — czujnik teraz zasilony, odczyt wiarygodny
  if (!collisionClearForRoofMove()) {
    collisionPowerOff();
    return blockRoofCommand(g_lastRoofBlockReason);
  }
#endif

#if ROOF_INTERLOCK_RELAYS
  roofRelaysOff();
#endif
  if (roofState == RoofState::ERROR || (!clearanceAllowsClose()))
    return blockRoofCommand("CLOSE: blad dachu lub utrata parku przed impulsem");
  if (mountProtectionBlocksClose() || !collisionClearForRoofMove() ||
      (limitClosedActive() && limitOpenActive()) || !clearanceAllowsClose())
    return blockRoofCommand("CLOSE: fizyczna blokada przed impulsem");
  if (pulseActive[RELAY_ROOF_OPEN] || pulseActive[RELAY_ROOF_CLOSE] || pulseActive[RELAY_ROOF_STOP])
    return blockRoofCommand("CLOSE: OFF niepotwierdzone");
  if (roofState==RoofState::ERROR || commandEpoch!=g_stopEpoch)
    return blockRoofCommand("Ruch anulowany przez STOP lub blad w trakcie przygotowania");
  Serial.println(F(">> ROOF: CLOSE (impuls)"));
  eventLogEx("INFO", "ROOF", "CLOSE command accepted");
  if (!relayPulse(RELAY_ROOF_CLOSE)) {
    roofFaultStop("CLOSE niepotwierdzone przez Waveshare 17658",RoofFaultKind::COMMUNICATION);
    return blockRoofCommand(g_roofMotionFault);
  }
  roofState = RoofState::CLOSING;
  roofMoveStartMs = millis();
  roofMotionWatchStart();
  return true;
}

static bool roofStop() {
  const bool wasFault=(roofState==RoofState::ERROR);
  g_lastRoofBlockReason="";
  if (!stopDrivePulse()) {
    roofState=RoofState::ERROR;
    g_roofMotionFault=g_lastRoofBlockReason;
    g_roofFaultKind=RoofFaultKind::STOP_UNCONFIRMED;
    notifyEvent(NOTIFY_CRITICAL,"DACH",g_roofMotionFault.c_str());
    return false;
  }
  roofState=wasFault ? RoofState::ERROR : RoofState::STOPPED;
  return true;
}

static bool acknowledgeCommunicationFault() {
  if (roofState!=RoofState::ERROR || g_roofFaultKind!=RoofFaultKind::COMMUNICATION ||
      (limitClosedActive() && limitOpenActive()) || !clearanceAllowsClose() ||
      mountProtectionBlocksClose() || !collisionClearForRoofMove() || !modbusRelayOK ||
      !g_stopCommandConfirmed || !g_stopReleaseConfirmed ||
      pulseActive[RELAY_ROOF_OPEN] || pulseActive[RELAY_ROOF_CLOSE] || pulseActive[RELAY_ROOF_STOP])
    return blockRoofCommand("ACK: tylko komunikacja; wymagane STOP/OFF, przeswit lub PARK i brak fizycznych blokad");
  if (!modbusRelayPollNow() || modbusRelayActualState[RELAY_ROOF_OPEN] ||
      modbusRelayActualState[RELAY_ROOF_CLOSE] || modbusRelayActualState[RELAY_ROOF_STOP])
    return blockRoofCommand("ACK: rzeczywiste OFF wszystkich wejsc napedu niepotwierdzone");
  g_roofFaultKind=RoofFaultKind::NONE;g_roofMotionFault="";
  roofState=limitClosedActive()?RoofState::CLOSED:limitOpenActive()?RoofState::OPEN:RoofState::STOPPED;
  g_pendingAutoClose=false;
  eventLogEx("WARN","ROOF","Operator inspected stationary roof and acknowledged communication fault; no movement commanded");
  return true;
}

// Explicit operator inspection is mandatory at the authenticated fault_ack endpoint.
// Never called by the automatic weather loop; no motion command is issued here.
static bool parkLossRecoveryReady() {
#if PARK_GUARD_ENABLED
  sampleHardwarePark();
  return digitalRead(PARK_SENSOR_PIN)==LOW && digitalRead(PARK_SENSOR_DEC_PIN)==LOW &&
    g_stablePark.ready((uint32_t)millis(),PARK_SETTLE_MS) && clearanceAllowsClose() &&
    !(limitClosedActive() && limitOpenActive()) && !mountProtectionBlocksClose() &&
    collisionClearForRoofMove() && modbusRelayOK &&
    g_stopAttempted && g_stopCommandConfirmed && g_stopReleaseConfirmed &&
    !pulseActive[RELAY_ROOF_OPEN] && !pulseActive[RELAY_ROOF_CLOSE] && !pulseActive[RELAY_ROOF_STOP];
#else
  return false; // No physical PARK inputs configured: no PARK-loss acknowledgment.
#endif
}
static bool acknowledgeParkLossFault() {
  const uint32_t stopEpoch=g_stopEpoch;
  if(roofState!=RoofState::ERROR || g_roofFaultKind!=RoofFaultKind::PARK_LOST || !parkLossRecoveryReady())
    return blockRoofCommand("ACK PARK: wymagane stabilne RA/DEC, STOP/OFF, inspekcja i brak blokad");
  if(!modbusRelayPollNow() || modbusRelayActualState[RELAY_ROOF_OPEN] ||
     modbusRelayActualState[RELAY_ROOF_CLOSE] || modbusRelayActualState[RELAY_ROOF_STOP] ||
     g_stopEpoch!=stopEpoch || roofState!=RoofState::ERROR || g_roofFaultKind!=RoofFaultKind::PARK_LOST ||
     !parkLossRecoveryReady()) // Recheck inputs, fault identity and concurrent STOP after IO.
    return blockRoofCommand("ACK PARK: odczyt OFF lub ponowne sprawdzenie PARK/blokad nieudane");
  g_roofFaultKind=RoofFaultKind::NONE;g_roofMotionFault="";
  roofState=limitClosedActive()?RoofState::CLOSED:limitOpenActive()?RoofState::OPEN:RoofState::STOPPED;
  g_pendingAutoClose=false;
  eventLogEx("WARN","ROOF","Operator inspected stationary roof; stable PARK restored and STOP/OFF verified; PARK fault acknowledged, no motion commanded");
  return true;
}

// ════════════════════════════════════════════════════════════════════════
// E-STOP MONTAZU (krancowka overtravel)
// ════════════════════════════════════════════════════════════════════════
// Surowy odczyt krancowki: TRIP gdy montaz poza obwiednia LUB przerwany kabel (NC).
static bool mountLimitRawTripped() {
#if MOUNT_LIMIT_ENABLED
  bool high = (digitalRead(MOUNT_LIMIT_PIN) == HIGH);
  #if MOUNT_LIMIT_TRIP_WHEN_HIGH
    return high;        // NC fail-safe: HIGH = styk rozwarty / przerwany kabel = trip
  #else
    return !high;       // NO: LOW = styk zwarty do GND = trip
  #endif
#else
  return false;
#endif
}

// Opcjonalny przekaznik odcinajacy zasilanie montazu (ON = odciete).
static bool mountPowerCut(bool cut) {
#if MOUNT_POWER_RELAY_ENABLED
  const bool ok=relayWrite((uint8_t)MOUNT_POWER_RELAY_CH,cut);
  eventLogEx(ok?"WARN":"CRITICAL","MOUNT",cut?"Mount power CUT command acknowledged=%d":"Mount power RESTORE command acknowledged=%d",(int)ok);
  return ok;
#else
  (void)cut;return true; // No power output configured; never claim measured disconnection.
#endif
}

// Wyzwolenie E-STOP: zatrzask + STOP dachu + (opcjonalnie) odciecie montazu + krzyk.
static bool mountEstopTrigger(const String& reason) {
  if(g_mountEstop)return blockRoofCommand("Ochrona montazu jest juz zatrzasnieta; sprawdz stan i potwierdzenia");
  g_mountEstop=true;g_mountEstopSinceMs=millis();g_mountEstopReason=reason;
  g_mountPowerRestoreBlocked=true;
  const bool saved=operatorStore("mountTrip",true);
  if(!saved)eventLogEx("CRITICAL","MOUNT","Could not persist mount protection latch; inspect before any restart");
  if(ROOF_MOTION_ENABLED && astro_roof::moving(roofState) &&
     (roofState==RoofState::OPENING || mountProtectionBlocksClose())) roofStop();
  const bool cut=mountPowerCut(true);
  eventLogEx("CRITICAL","MOUNT","Mount overtravel/protection: %s; power command ack=%d (physical voltage not measured)",reason.c_str(),(int)cut);
  notifyEvent(NOTIFY_CRITICAL,"MONTAZ","Ochrona montazu aktywna. Zadanie odciecia zgodnie z konfiguracja; sprawdz potwierdzenie. Brak automatycznego przywracania.");
  if(!saved) {
    notifyEvent(NOTIFY_CRITICAL,"PAMIEC","Nie zapisano zatrzasku montazu w NVS. Nie restartuj w celu skasowania bledu; wymagany nadzor i sprawdzenie pamieci.");
    return blockRoofCommand("Zatrzask tylko w RAM: zapis NVS NIEPOTWIERDZONY; sprawdz pamiec przed restartem");
  }
  if(!cut)return blockRoofCommand("Ochrona zatrzasnieta, ale polecenie odciecia montazu NIEPOTWIERDZONE");
  return true;
}

// Reset E-STOP - tylko gdy krancowka NIE jest juz w stanie trip (montaz w obwiedni).
static bool mountEstopReset() {
  if(mountLimitRawTripped() || astro_roof::moving(roofState) || !modbusRelayOK ||
     pulseActive[RELAY_ROOF_OPEN] || pulseActive[RELAY_ROOF_CLOSE] || pulseActive[RELAY_ROOF_STOP] ||
     (g_stopAttempted && (!g_stopCommandConfirmed || !g_stopReleaseConfirmed))) {
    g_lastRoofBlockReason="Montaz: wymagane nieaktywne wejscie ochrony, nieruchomy dach i sprawna komunikacja";return false;
  }
  if(!modbusRelayPollNow() || modbusRelayActualState[RELAY_ROOF_OPEN] ||
     modbusRelayActualState[RELAY_ROOF_CLOSE] || modbusRelayActualState[RELAY_ROOF_STOP])
    return blockRoofCommand("Montaz: wymagane swieze OFF wszystkich wejsc napedu dachu");
  if(!mountPowerCut(false)) {
    mountPowerCut(true);return blockRoofCommand("Przywrocenie niepotwierdzone; ponowiono zadanie odciecia");
  }
#if MOUNT_POWER_RELAY_ENABLED
  if(!modbusRelayPollNow() || modbusRelayActualState[MOUNT_POWER_RELAY_CH]) {
    mountPowerCut(true);return blockRoofCommand("Brak potwierdzenia wyjscia montazu; ponowiono odciecie");
  }
#endif
  if(!operatorStore("mountTrip",false)) {
    mountPowerCut(true);return blockRoofCommand("Brak zapisu odblokowania; przywrocono zadanie odciecia");
  }
  g_mountPowerRestoreBlocked=false;g_mountEstop=false;g_mountEstopReason="";
  eventLogEx("WARN","MOUNT","Operator explicitly restored mount power/protection after inspection");
  return true;
}

// Cykliczna obsluga krancowki (z debounce). Wywolywana z loop() - szybka reakcja.
static void mountEstopTick() {
#if MOUNT_LIMIT_ENABLED
  bool raw = mountLimitRawTripped();
  unsigned long now = millis();
  if (raw != g_mountLimitRawLast) {
    g_mountLimitRawLast = raw;
    g_mountLimitChangeMs = now;
  }
  if ((now - g_mountLimitChangeMs) >= MOUNT_LIMIT_DEBOUNCE_MS) {
    if (raw && !g_mountEstop) {
      mountEstopTrigger("Krancowka overtravel montazu");
    }
  }
#endif
}

// Reczne wyzwolenie E-STOP (panic z web UI) - dziala nawet bez fizycznej krancowki.
static bool mountEstopManual() {
  return mountEstopTrigger("Reczna ochrona montazu");
}

// ════════════════════════════════════════════════════════════════════════
// ZYWY STAN LINKU (naprawia stale g_ethReady po wypieciu kabla W5500)
// ════════════════════════════════════════════════════════════════════════
static void linkStatusTick() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (last != 0 && now - last < 3000UL) return;   // pierwszy raz liczy od razu
  last = now ? now : 1;

  bool wifiUp = false, ethUp = false;
#if USE_WIFI
  wifiUp = (WiFi.status() == WL_CONNECTED);
#endif
#if USE_W5500
  if (g_ethReady) {
    EthernetLinkStatus ls = Ethernet.linkStatus();
    // LinkOFF = kabel wypiety/uszkodzony. Brak IP = brak konfiguracji.
    ethUp = (ls != LinkOFF) && (Ethernet.localIP() != IPAddress(0, 0, 0, 0));
  }
#endif
  g_wifiLinkUp = wifiUp;
  g_ethLinkUp  = ethUp;
  g_netLinkUp     = wifiUp || ethUp;

  if (!g_netLinkUp) {
    if (g_linkDownSinceMs == 0) g_linkDownSinceMs = now;
    if (!g_linkLostAlertSent && (now - g_linkDownSinceMs) >= LINK_LOST_ALERT_MS) {
      g_linkLostAlertSent = true;
      Serial.println(F("!! SIEC: brak WiFi i Ethernetu - zdalny dostep/NINA odciete"));
      // Kanal NIEZALEZNY od LAN (SMS/syrena) - bo wlasnie LAN padl.
      notifyEvent(NOTIFY_CRITICAL, "SIEC", "ESP utracil WiFi i Ethernet - brak komunikacji z NINA/zdalnego dostepu.");
    }
  } else {
    g_linkDownSinceMs = 0;
    g_linkLostAlertSent = false;
  }
}

// ════════════════════════════════════════════════════════════════════════
// LAGODNY RE-INIT NASLUCHU przy krotkiej ciszy Alpaca (zanim duzy alarm).
// ════════════════════════════════════════════════════════════════════════
// Czasem gniazdo serwera (W5500/WiFi) sie "zawiesi" i NINA przestaje dostawac
// odpowiedzi, choc siec fizycznie zyje. Zanim po 5 min odpali sie SMS/syrena,
// sprobuj TANIO odzyskac sesje: po ~75s ciszy Alpaca przy otwartym dachu zrob
// stop()+begin() na serwerach. To czesto odswieza martwe gniazdo bez budzenia
// Cie alarmem. Robione MAKSYMALNIE raz na 2 min (throttle), zeby nie restartowac
// w kolko. Nie rusza czujnikow ani dachu - tylko warstwe nasluchu HTTP/Alpaca.
#ifndef COMMS_SOFT_REINIT_MS
  #define COMMS_SOFT_REINIT_MS 75000UL     // 75s ciszy Alpaca -> sprobuj odswiezyc nasluch
#endif
#ifndef COMMS_SOFT_REINIT_THROTTLE_MS
  #define COMMS_SOFT_REINIT_THROTTLE_MS 120000UL   // nie czesciej niz co 2 min
#endif
// DOMYSLNIE WYLACZONE. Ten "ratunkowy" re-init nasluchu (ethServer.begin/
// wifiServer.begin na dzialajacym serwerze) RWAL aktywne polaczenia www przy
// otwartym dachu bez NINA -> "Connection reset by peer", strona wisla. Prawdziwy
// watchdog laczności (alarm SMS/syrena) jest osobny i dziala dalej. Wlacz: -D COMMS_SOFT_REINIT_ENABLED=1
#ifndef COMMS_SOFT_REINIT_ENABLED
  #define COMMS_SOFT_REINIT_ENABLED 0
#endif
static unsigned long g_lastSoftReinitMs = 0;

static void commsSoftReinitTick(unsigned long now) {
#if COMMS_WATCHDOG_ENABLED && COMMS_SOFT_REINIT_ENABLED
  // tylko przy otwartym/odslonietym dachu - gdy NINA POWINNA z nami gadac
  bool roofExposed = (roofState == RoofState::OPEN ||
                      roofState == RoofState::OPENING ||
                      roofState == RoofState::STOPPED ||
                      roofState == RoofState::UNKNOWN);
  if (!roofExposed) return;
  if (g_lastAlpacaContactMs == 0) return;            // NINA jeszcze nigdy sie nie odezwala
  unsigned long since = now - g_lastAlpacaContactMs;
  if (since > 3600000UL) return;                     // overflow/skok czasu
  if (since < COMMS_SOFT_REINIT_MS) return;          // cisza za krotka - nie ruszaj
  if (g_lastSoftReinitMs != 0 && (now - g_lastSoftReinitMs) < COMMS_SOFT_REINIT_THROTTLE_MS) return;
  g_lastSoftReinitMs = now;

  Serial.println(F("[NET] Cisza Alpaca >75s - lagodny re-init nasluchu serwera (proba ratunku sesji)"));
#if USE_WIFI
  if (g_wifiReady) { wifiServer.begin(); }
#endif
#if USE_W5500
  if (g_ethReady)  { ethServer.begin(); }
#endif
#endif
}

// ════════════════════════════════════════════════════════════════════════
// WATCHDOG LACZNOSCI Z NINA (Alpaca)
// ════════════════════════════════════════════════════════════════════════
// Przy OTWARTYM dachu brak zapytan Alpaca > timeout = utrata kontaktu z NINA.
static void commsWatchdogTick(unsigned long now) {
#if COMMS_WATCHDOG_ENABLED
  bool roofExposed = (roofState == RoofState::OPEN ||
                      roofState == RoofState::OPENING ||
                      roofState == RoofState::STOPPED ||
                      roofState == RoofState::UNKNOWN);

  // Kazde nowe odsloniecie dachu rozpoczyna NOWA sesje watchdogu.
  // Nie wolno uzywac starego kontaktu NINA sprzed poprzedniego zamkniecia,
  // bo po ponownym otwarciu moglibysmy od razu zglosic utrate lacznosci.
  static bool wasRoofExposed = false;
  if (roofExposed && !wasRoofExposed) {
    g_lastAlpacaContactMs = 0;
    g_lastSafetyMonitorContactMs = 0;
    g_commsNoContactSinceMs = now;
    g_commsLost = false;
    g_commsLostSinceMs = 0;
  }

  if (!roofExposed) {
    g_commsLost = false;
    g_commsLostSinceMs = 0;
    g_commsNoContactSinceMs = 0;
    wasRoofExposed = false;
    return;
  }
  wasRoofExposed = true;

  unsigned long since = 0;
  if (g_lastSafetyMonitorContactMs != 0) {
    // Prawdziwy czas ostatniego kontaktu NINA. Nie nadpisujemy go sztucznym baseline.
    since = now - g_lastSafetyMonitorContactMs;
  } else {
    // NINA jeszcze nigdy nie odezwała się po otwarciu dachu.
    // Osobny zegar watchdogu pozwala po 5 min zgłosić brak NINA, ale NIE udaje
    // kontaktu dla ninaIsActive().
    if (g_commsNoContactSinceMs == 0) g_commsNoContactSinceMs = now;
    since = now - g_commsNoContactSinceMs;
  }

  if (since >= COMMS_LOST_TIMEOUT_MS) {
    if (!g_commsLost) {
      g_commsLost = true;
      g_commsLostSinceMs = now;
      Serial.println(F("!! LACZNOSC: brak kontaktu Alpaca z NINA przy otwartym dachu"));
      notifyEvent(NOTIFY_CRITICAL, "LACZNOSC", "Brak kontaktu z NINA (Alpaca) przy otwartym dachu! Sprawdz PC/siec.");
#if USE_TELEGRAM
      telegramSend("UWAGA: ESP bez kontaktu z NINA (Alpaca) >5 min przy otwartym dachu. Sprawdz polaczenie.", true);
#endif
    }
  } else {
    g_commsLost = false;
  }
#endif
}

static void roofTick() {
  sampleHardwarePark();
#if PARK_GUARD_ENABLED
  if (astro_roof::parkRequiredDuringMotion(roofState,CLOSE_CLEARANCE_CONFIRMED) && !(roofState==RoofState::OPENING && limitOpenActive() && !limitClosedActive()) &&
      (digitalRead(PARK_SENSOR_PIN)!=LOW || digitalRead(PARK_SENSOR_DEC_PIN)!=LOW)) {
    roofFaultStop("Utrata parku RA/DEC podczas ruchu dachu",RoofFaultKind::PARK_LOST);
    return;
  }
#endif
  const RoofState before=roofState;
  updateRoofStateFromLimits();
  if (before!=roofState) eventLogEx(roofState==RoofState::ERROR?"CRITICAL":"INFO",
      "ROOF","STATE %s -> %s",roofStateName(before),roofStateName(roofState));
  if (before!=roofState && (roofState==RoofState::OPEN || roofState==RoofState::CLOSED)) {
    g_roofStuckChecked=true; g_roofMotionFault=""; collisionPowerOff();
  }
  if (!astro_roof::moving(roofState)) return;
  const uint32_t elapsed=uint32_t(millis()-roofMoveStartMs);
#if COLLISION_SENSOR_ENABLED
  collisionSensorTick();
  if (g_collisionSensorActive) {
    roofFaultStop(g_collisionFault.length()?g_collisionFault:"Czujnik kolizji aktywny");
    return;
  }
#endif
  if (!g_roofStuckChecked && elapsed>ROOF_STUCK_CHECK_MS) {
    const bool sameClosed=limitClosedActive()==g_roofMoveStartLimitClosed;
    const bool sameOpen=limitOpenActive()==g_roofMoveStartLimitOpen;
    if (sameClosed && sameOpen && (g_roofMoveStartLimitClosed || g_roofMoveStartLimitOpen)) {
      roofFaultStop("STUCK: krancowka poczatkowa nie odpuscila");
      return;
    }
    g_roofStuckChecked=true;
  }
  if (elapsed>ROOF_MOVE_TIMEOUT_MS) roofFaultStop("TIMEOUT ruchu dachu");
}
// Invoked before/after bounded work, without a new task or recursive HTTP.
static void serviceRoofFast() {
  sampleHardwarePark();
  mountEstopTick();
  collisionSensorTick();
  roofTick();
  updatePulseOutputs();
}

static void relayTestRun(bool includeRoof) {
  // CH1-CH8 = Waveshare 17658; CH4-CH6 are reserved for system functions.
  if (!modbusRelayOK) { Serial.println(F("[RELAYTEST] BRAK WAVESHARE 17658 / MODBUS")); return; }
  Serial.printf("[RELAYTEST] start includeRoof=%d\n", (int)includeRoof);
  auto pulse = [&](uint8_t ch, uint16_t ms) {
    Serial.printf("[RELAYTEST] pulse ch=%u\n", ch);
    relayWrite(ch, true); delay(ms); relayWrite(ch, false); delay(200);
  };
  if (includeRoof) {
    pulse(RELAY_ROOF_OPEN, 500);
    pulse(RELAY_ROOF_CLOSE, 500);
    pulse(RELAY_ROOF_STOP, 300);
  }
  for (uint8_t id = 1; id <= 5; ++id) if (astroAuxAvailable(id)) pulse(astroAuxChannel(id), 500);
  Serial.println(F("[RELAYTEST] done"));
}

static int domeShutterStatusFromRoof() {
  switch (roofState) {
    case RoofState::OPEN:    return 0;
    case RoofState::CLOSED:  return 1;
    case RoofState::OPENING: return 2;
    case RoofState::CLOSING: return 3;
    default:                 return 4;
  }
}
static bool domeIsSlewing() {
  return (roofState == RoofState::OPENING || roofState == RoofState::CLOSING);
}

// ============== I2C scan ==============
static void i2cScanOnce() {
  Serial.printf("I2C scan (SDA=%d SCL=%d): ", I2C_SDA_PIN, I2C_SCL_PIN);
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) { Serial.printf("0x%02X ", addr); found++; }
    delay(1);
  }
  if (!found) Serial.print("(nic)");
  Serial.println();
}

// ============== Analog wind read () ==============
// Zwraca chwilowa predkosc [m/s] i zapisuje napiecie do g_windRawVolts.
static float readWindSpeedMS() {
  static_assert(WIND_ADC_SAMPLES==16,"Wind frame policy requires 16 samples");
  uint16_t rawSamples[16];
  for(int i=0;i<WIND_ADC_SAMPLES;++i) {
    rawSamples[i]=analogRead(WIND_PIN);
    delayMicroseconds(200);
  }
  const auto frame=windAdcFrame(rawSamples);
  g_windAdcMin=frame.sorted[0];g_windAdcMax=frame.sorted[15];g_windAdcRails=frame.rails;
  uint32_t sum=0;double millivolts=0;
  // Remove at most two isolated extremes on each side of a 3.2 ms burst.
  // Sustained high wind, a disconnected input, and repeated clipping remain faults.
  for(unsigned i=2;i<14;++i) {
    sum+=frame.sorted[i];
    millivolts+=esp_adc_cal_raw_to_voltage(frame.sorted[i],&g_windAdcChars);
  }
  g_windAdcRaw=float(sum)/12;
  g_windAdcVolts=float(millivolts/12)/1000.0f;
  g_windRawVolts=(g_windAdcVolts*g_windCal.gain+g_windCal.offset)*g_windCal.ratio;
  const uint32_t now=millis();
  const bool accepted=windAcceptSample(g_windCal,g_windValidity,g_windRawVolts,!frame.usable(),now);
  if(!accepted) {
    ++g_windRejectedFrames;
    if(g_windValidity.holding)++g_windHeldFrames;
    static uint32_t lastRejectedLog=0;static bool rejectionLogged=false;
    if(!rejectionLogged || uint32_t(now-lastRejectedLog)>=300000) {
    rejectionLogged=true;lastRejectedLog=now;
    eventLogEx(g_windValidity.holding?"WARN":"ERROR","WIND",
      "ADC frame rejected: sensor=%.3fV raw=%.1f min=%u max=%u rails=%u held=%u age=%lu",
      g_windRawVolts,g_windAdcRaw,(unsigned)g_windAdcMin,(unsigned)g_windAdcMax,
      g_windAdcRails,(unsigned)g_windValidity.holding,(unsigned long)(now-g_windValidity.sampledAt));
    }
  }
  if(!isfinite(g_windStableReference)||fabsf(g_windRawVolts-g_windStableReference)>0.01f) {
    g_windStableSince=now; g_windStableReference=g_windRawVolts;
  }
  return accepted ? windConvert(g_windRawVolts,g_windCal) :
    (g_windValidity.holding ? windConvert(g_windValidity.voltage,g_windCal) : NAN);
}

// Wpisz chwilowy odczyt do okna i przelicz srednia trwala + podmuch.
static void windWindowPush(float ms) {
  if(!isfinite(ms)) return;
  const uint32_t now=millis();
  g_windWindow[g_windWindowHead]=ms; g_windWindowTimes[g_windWindowHead]=now;
  g_windWindowHead=(g_windWindowHead+1)%WIND_WINDOW_SAMPLES;
  if(g_windWindowCount<WIND_WINDOW_SAMPLES) ++g_windWindowCount;
  float sum=0,mx=0; int count=0;
  for(int i=0;i<g_windWindowCount;++i) {
    if(uint32_t(now-g_windWindowTimes[i])>=64000) continue;
    sum+=g_windWindow[i]; mx=fmaxf(mx,g_windWindow[i]); ++count;
  }
  g_windSustained=count?sum/count:0; g_windGust=mx;
}
static void windResetAfterCalibration() {
  g_windValidity.faultLatched=false;
  g_windValidity.present=false; g_windValidity.goodCount=0;g_windValidity.holding=false;g_windValidity.rejectedRun=0;
  g_windWindowCount=0; g_windWindowHead=0;
  g_wind_ms=NAN; g_windSustained=NAN; g_windGust=NAN;
  g_windStableReference=NAN;
  // Do not clear an existing wind latch or alter closure thresholds.
}

// Retained electrical-fault timer, independent of calibrated sensor validity.
// Poza [WIND_FAULT_V_LOW, WIND_FAULT_V_HIGH] przez WIND_FAULT_MS -> AWARIA.
static void windFaultTick(unsigned long now) {
  bool outOfRange = isnan(g_windRawVolts) ||
                    g_windRawVolts < WIND_FAULT_V_LOW ||
                    g_windRawVolts > WIND_FAULT_V_HIGH;
  if (outOfRange) {
    if (g_windFaultSinceMs == 0) g_windFaultSinceMs = now;
    if (!g_windFault && (now - g_windFaultSinceMs) >= WIND_FAULT_MS) {
      g_windFault = true;
      Serial.printf("!! ANEMOMETR AWARIA: napiecie %.3fV poza zakresem awarii\n", g_windRawVolts);
      eventLogEx("ERROR", "WIND", "Anemometer fault: %.2f V out of range", g_windRawVolts);
    }
  } else {
    if (g_windFault) Serial.println(F("[WIND] anemometr z powrotem w normie"));
    g_windFaultSinceMs = 0;
    g_windFault = false;
  }
}

// ════════════════════════════════════════════════════════════════════════
// AUTOMATYCZNY WYBOR TRANSPORTU (ETH <-> WiFi) z health-trackingiem.
// Problem, ktory to naprawia: g_ethReady ustawia sie RAZ przy starcie (W5500
// wykryty) i nie wie, ze modul jest "na wpol zywy" (link PHY jest, ale dane nie
// spadaly na dzialajace WiFi. Tu liczymy kolejne bledy wysylki PER INTERFEJS:
// po NET_TX_FAIL_LIMIT transport uznajemy za niezdrowy i preferujemy drugi, a co
// NET_TX_RECHECK_MS dajemy niezdrowemu szanse na powrot (recovery). Dziala w obie
// strony - raz padnie WiFi, raz Ethernet, firmware sam wybiera zywy kanal.
// ════════════════════════════════════════════════════════════════════════
#if 1 // shared transport helpers also exist when all online channels are compiled off
#ifndef NET_TX_FAIL_LIMIT
  #define NET_TX_FAIL_LIMIT 2          // tyle kolejnych bledow -> transport "niezdrowy"
#endif
#ifndef NET_TX_RECHECK_MS
  #define NET_TX_RECHECK_MS 120000UL   // co ile ponowic probe niezdrowego transportu (recovery)
#endif
enum NetIf { NET_NONE = 0, NET_ETH = 1, NET_WIFI = 2 };
struct NetHealth { uint8_t fails; uint32_t lastTryMs; };
static NetHealth g_ethTx  = {0, 0};
static NetHealth g_wifiTx = {0, 0};

static bool netEthLinkUp() {
#if USE_W5500
  return g_ethReady;
#else
  return false;
#endif
}
static bool netWifiLinkUp() {
#if USE_WIFI
  return (g_wifiReady && WiFi.status() == WL_CONNECTED);
#else
  return false;
#endif
}
// transport uzywalny = link up ORAZ (zdrowy LUB minal cooldown na re-probe)
static bool netTxUsable(const NetHealth& h, bool linkUp, uint32_t now) {
  if (!linkUp) return false;
  if (h.fails < NET_TX_FAIL_LIMIT) return true;
  return (now - h.lastTryMs >= NET_TX_RECHECK_MS);
}
// preferowany interfejs do nastepnej proby (kabel ma priorytet gdy zdrowy)
static NetIf netChoose() {
  uint32_t now = millis();
  bool e = netEthLinkUp(), w = netWifiLinkUp();
  // ═══ tryb wymuszony (1=TYLKO ETH "wole kabel", 2=TYLKO WiFi) ═══
  // W trybie wymuszonym NIE spadamy na drugi interfejs - jak lacze lezy, to
  if (g_netMode == 1) return e ? NET_ETH  : NET_NONE;
  if (g_netMode == 2) return w ? NET_WIFI : NET_NONE;
  // AUTO preferuje WiFi dla chmurowego TLS - handshake po W5500 potrafi
  // zakleszczyc loop() (patrz NET_MODE_DEFAULT). ETH tylko jako ostatnia deska.
  if (netTxUsable(g_wifiTx, w, now)) return NET_WIFI;
  if (netTxUsable(g_ethTx,  e, now)) return NET_ETH;
  if (w) return NET_WIFI;              // oba niezdrowe: probuj cokolwiek z linkiem
  if (e) return NET_ETH;
  return NET_NONE;
}
// drugi interfejs (do failoveru w tej samej probie), jesli ma link
static NetIf netOther(NetIf nif) {
  if (g_netMode != 0) return NET_NONE;   // bez failoveru w trybie wymuszonym
  if (nif == NET_ETH  && netWifiLinkUp()) return NET_WIFI;
  if (nif == NET_WIFI && netEthLinkUp())  return NET_ETH;
  return NET_NONE;
}
static void netReport(NetIf nif, bool ok) {
  NetHealth* h = (nif == NET_ETH) ? &g_ethTx : (nif == NET_WIFI) ? &g_wifiTx : nullptr;
  if (!h) return;
  h->lastTryMs = millis();
  if (ok) h->fails = 0;
  else if (h->fails < 250) h->fails++;
}
static const char* netName(NetIf nif) {
  return (nif == NET_ETH) ? "ETH" : (nif == NET_WIFI) ? "WiFi" : "---";
}

#endif // USE_TELEGRAM

#include "mqtt_runtime.h"

// ============== Telegram ==============
#if USE_TELEGRAM
// ═══ KOLEJKA ODROCZONYCH WIADOMOSCI TELEGRAM ══════════════════
// FIX KRYTYCZNY: dotychczasowa galaz "Odlozono (NINA aktywna)" NIC nie
// odkladala - wiadomosc PRZEPADALA bez sladu. A ze NINA odpytuje Alpaca co
// ~1 s (okno ninaIsActive = 3 s), to w trakcie CALEJ sesji przepadaly
// WSZYSTKIE wiadomosci nie-force: "Teleskop zaparkowany", ostrzezenia Park
// Guard, powroty SAFE... Stad wrazenie "Telegram raz dziala, raz nie".
// Teraz: mala kolejka FIFO. Wysylamy w okienku miedzy zapytaniami NINA
// (dither/slew/download daja przerwy), a jesli wiadomosc czeka dluzej niz
// TG_DEFER_MAX_MS - wysylamy MIMO NINA, ale JEDNA szybka proba (primaryOnly:
// bez retry i bez failoveru, max ~4-5 s blokady zamiast ~20 s).
#ifndef TG_QUEUE_LEN
  #define TG_QUEUE_LEN 6
#endif
#ifndef TG_DEFER_MAX_MS
  #define TG_DEFER_MAX_MS 180000UL     // po 3 min czekania wysylamy mimo NINA
#endif
static String        g_tgQueue[TG_QUEUE_LEN];
static unsigned long g_tgQueueAt[TG_QUEUE_LEN];
static uint8_t       g_tgQHead = 0, g_tgQCount = 0;

static void telegramSendNow(const String& msg, bool primaryOnly);

static void tgEnqueue(const String& msg) {
  for (uint8_t i = 0; i < g_tgQCount; i++)
    if (g_tgQueue[(uint8_t)((g_tgQHead + i) % TG_QUEUE_LEN)] == msg) return; // duplikat juz czeka
  if (g_tgQCount >= TG_QUEUE_LEN) {    // przepelnienie: wyrzuc najstarsza (jest najmniej aktualna)
    Serial.println(F("[TG] Kolejka pelna - usuwam najstarsza odlozona wiadomosc"));
    g_tgQueue[g_tgQHead] = "";
    g_tgQHead = (uint8_t)((g_tgQHead + 1) % TG_QUEUE_LEN);
    g_tgQCount--;
  }
  uint8_t idx = (uint8_t)((g_tgQHead + g_tgQCount) % TG_QUEUE_LEN);
  g_tgQueue[idx]   = msg;
  g_tgQueueAt[idx] = millis();
  g_tgQCount++;
  Serial.printf("[TG] Odlozono do kolejki (NINA aktywna), czeka: %u\n", (unsigned)g_tgQCount);
}

// Wolane z loop(): dosylanie odlozonych wiadomosci.
static void telegramQueueTick() {
  if (g_tgQCount == 0) return;
  unsigned long now = millis();
  if (now - g_tgLastSent < TG_MIN_INTERVAL_MS) return;   // szanuj globalny antyspam
  bool ninaBusy = ninaIsActive();
  unsigned long waited = now - g_tgQueueAt[g_tgQHead];
  if (ninaBusy && waited < TG_DEFER_MAX_MS) return;      // czekamy na okienko
  String msg = g_tgQueue[g_tgQHead];
  g_tgQueue[g_tgQHead] = "";                             // zwolnij bufor String
  g_tgQHead = (uint8_t)((g_tgQHead + 1) % TG_QUEUE_LEN);
  g_tgQCount--;
  // NINA wciaz aktywna po 3 min -> jedna szybka proba (patrz komentarz wyzej)
  telegramSendNow(msg, ninaBusy);
}

static void telegramSend(const String& msg, bool force) {
  if(!onlineTelegramEnabled())return;
  unsigned long now = millis();

  // anti-spam globalny + blokada duplikatow.
  // force=true przepuszcza krytyczne alarmy: start, FORCE CLOSE, STUCK, TIMEOUT.
  // odczyt pod mutexem - g_tgLastMsg (String) jest teraz tez pisany
  // z drugiego rdzenia (netTask), wiec surowy odczyt bez ochrony to ryzyko
  // wyscigu/crasha. Krotki timeout (20ms) - w razie czego wolimy przepuscic
  // wiadomosc niz zawiesic glowna petle.
  bool blocked = false;
  if (g_tgStateMutex && xSemaphoreTake(g_tgStateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    if (!force && now - g_tgLastSent < TG_MIN_INTERVAL_MS) blocked = true;
    if (!blocked && !force && msg == g_tgLastMsg && now - g_tgLastMsgAt < TG_DUPLICATE_SUPPRESS_MS) blocked = true;
    xSemaphoreGive(g_tgStateMutex);
  }
  if (blocked) return;

  // FIX KRYTYCZNY: nie blokuj loop() gdy NINA aktywna (wspolny straznik
  // ninaIsActive) - blokujacy TLS zrywal SafetyMonitor. zamiast DROPU
  // wiadomosc idzie do kolejki i zostanie doslana (telegramQueueTick w loop()).
  // telegramSendNow() jest juz i tak niebokujaca (patrz nizej), ale
  // ta kolejka aplikacyjna zostaje - dalej ma sens jako "poczekaj az NINA
  // skonczy sesje" zamiast zasypywac Telegram w trakcie obserwacji.
  if (!force && ninaIsActive()) { tgEnqueue(msg); return; }

  telegramSendNow(msg, false);
}

// CIENKI WRAPPER - odklada zlecenie do kolejki FreeRTOS i wraca
// natychmiast (<1ms). Prawdziwa, blokujaca wysylka TLS dzieje sie w netTask()
// na rdzeniu 0 - patrz telegramSendNow_blocking() nizej. Nazwa/sygnatura
// zostaje taka sama jak dawniej, wiec WSZYSTKIE dotychczasowe wywolania w
// kodzie (telegramSend, telegramQueueTick, wiadomosc startowa...) dzialaja
// bez zmian, po prostu przestaly juz blokowac serwer Alpaca.
static void telegramSendNow(const String& msg, bool primaryOnly) {
  if(!onlineTelegramEnabled())return;
  if (!g_netQueue) {
    Serial.println(F("[TG] netTask jeszcze nie wystartowal - wiadomosc pominieta"));
    return;
  }
  TgJob job;
  memset(&job, 0, sizeof(job));
  strncpy(job.text, msg.c_str(), sizeof(job.text) - 1);
  job.primaryOnly = primaryOnly;
  if (xQueueSend(g_netQueue, &job, 0) != pdTRUE) {
    Serial.println(F("[TG] Kolejka net-task pelna - wiadomosc pominieta (sprobuj pozniej)"));
  }
}

// Wlasciwa wysylka (TLS, blokujaca). primaryOnly=true -> tylko preferowany
// interfejs, bez powtorki i bez failoveru (fail-fast, na czas sesji NINA).
// wolana WYLACZNIE z netTask() na rdzeniu 0 - nigdy z loop().
static void telegramSendNow_blocking(const String& msg, bool primaryOnly) {
  unsigned long now = millis();

  // ═══ Telegram z AUTOMATYCZNYM failoverem ETH<->WiFi (health-aware) ═══
  // ═══ failover wylaczany trybem sieci (netChoose/netOther) ═══
  NetIf tgPrimary = netChoose();
  onlineTelegramStarted();
  struct TgResult {bool ok=false;~TgResult(){onlineTelegramResult(ok,ok?200:-1);}} tgResult;
  if (tgPrimary == NET_NONE) {
    // FIX: dawniej cichy return - w logu nie bylo sladu, ze wiadomosc w ogole
    // probowala wyjsc. Przy net_mode=2 (TYLKO WiFi) i niedzialajacym WiFi
    // wygladalo to tak, jakby telegramSendNow() nigdy nie bylo wywolane.
    Serial.printf("[TG] Brak transportu (net_mode=%u, ETH link=%d, WiFi link=%d) - wiadomosc NIE wyslana: %s\n",
                  (unsigned)g_netMode, (int)netEthLinkUp(), (int)netWifiLinkUp(), msg.c_str());
    return;
  }

  // zapis pod tym samym mutexem co odczyt w telegramSend() - to teraz
  // dzieje sie na rdzeniu 0 (netTask), a czyta rdzen 1 (loop). String bez
  // ochrony przy jednoczesnym dostepie z dwoch rdzeni to realne ryzyko crasha.
  if (g_tgStateMutex && xSemaphoreTake(g_tgStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    g_tgLastSent = now;
    g_tgLastMsg = msg;
    g_tgLastMsgAt = now;
    xSemaphoreGive(g_tgStateMutex);
  }
#if USE_TASK_WDT
  esp_task_wdt_reset();                 // swiezy watchdog przed blokujacym DNS/TLS
#endif

  // Transporty bazowe (oba w zasiegu do konca funkcji - tgClient owija jeden z nich)
#if USE_W5500
  EthernetClient ethBase;
#endif
  WiFiClient wifiBase;

  ESP_SSLClient tgClient;               // TLS owijajacy dowolny Client (Eth/WiFi)
  tgClient.setClient(&wifiBase);        // placeholder - wlasciwy transport ustawiamy w petli
  tgClient.setInsecure();               // bez weryfikacji certyfikatu (jak dotychczas)
  tgClient.setBufferSizes(16384, 2048); // wieksze bufory - handshake TLS sie miesci
  tgClient.setTimeout(3);               // BSSL_TCPClient: timeout w sekundach, nie milisekundach
  tgClient.setHandshakeTimeout(4);      // skrocony (bylo 8s)
#if DIAG_TLS_DEBUG
  tgClient.setDebugLevel(4);
#endif

  // Kolejnosc prob: preferowany interfejs, potem (jesli ma link) drugi - failover od razu
  NetIf tgOrder[2] = { tgPrimary, netOther(tgPrimary) };
  bool  conn  = false;
  NetIf tgUsed = NET_NONE;
  const int tgAttempts = primaryOnly ? 1 : 2;   // fail-fast przy aktywnej NINA
  for (int a = 0; a < tgAttempts && !conn; a++) {
    NetIf cand = tgOrder[a];
    if (cand == NET_NONE) continue;
#if USE_W5500
    if (cand == NET_ETH) tgClient.setClient(&ethBase);
    else                 tgClient.setClient(&wifiBase);
#else
    tgClient.setClient(&wifiBase);
#endif
    Serial.print(F("[TG] Wysylam (")); Serial.print(netName(cand));
    Serial.print(F("): ")); Serial.println(msg);

#if USE_W5500
    if (cand == NET_ETH) {              // diagnostyka DNS po kablu
      Serial.print(F("[TG] DNS serwer: ")); Serial.println(Ethernet.dnsServerIP());
      DNSClient dns; dns.begin(Ethernet.dnsServerIP());
      IPAddress tgIP;
      int dnsOk = dns.getHostByName("api.telegram.org", tgIP);
      if (dnsOk == 1) { Serial.print(F("[TG] api.telegram.org -> ")); Serial.println(tgIP); }
      else { Serial.print(F("[TG] BLAD DNS (kod ")); Serial.print(dnsOk); Serial.println(F(") - sprawdz bramę/DNS")); }
    }
#endif
#if USE_TASK_WDT
    esp_task_wdt_reset();
#endif
    bool c = tgClient.connect("api.telegram.org", 443);
    if (!c && !primaryOnly) {           // pierwszy TLS po bootcie lubi pasc - ponawiamy raz (nie w fail-fast)
      Serial.println(F("[TG] TLS 1. proba nieudana - ponawiam..."));
      tgClient.stop();
      delay(300);
#if USE_TASK_WDT
      esp_task_wdt_reset();
#endif
      c = tgClient.connect("api.telegram.org", 443);
    }
    netReport(cand, c);                 // aktualizuj health interfejsu
    if (c) { conn = true; tgUsed = cand; }
    else {
      Serial.print(F("[TG] ")); Serial.print(netName(cand));
      Serial.println(F(" nieudane - probuje drugi interfejs (jesli jest)"));
      tgClient.stop();
#if USE_W5500
      if (cand == NET_ETH) ethBase.stop(); else wifiBase.stop();
#else
      wifiBase.stop();
#endif
    }
  }
  if (!conn) { Serial.println(F("[TG] Brak polaczenia TLS (ETH i WiFi nieudane)")); return; }
  Serial.print(F("[TG] TLS OK przez ")); Serial.println(netName(tgUsed));
#if USE_TASK_WDT
  esp_task_wdt_reset();                 // swiezy watchdog przed odczytem odpowiedzi
#endif

  // URL-encode message (prosta wersja - tylko spacje i podstawowe znaki)
  const String branded = observatoryMessage(msg);
  String encoded = "";
  for (int i = 0; i < (int)branded.length(); i++) {
    char c = branded[i];
    if (c == ' ') encoded += '+';
    else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '-' || c == '_' ||
             c == '.' || c == '!' || c == '(' || c == ')' || c == ':') {
      encoded += c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }

  String req = String("GET /bot") + TG_TOKEN +
               "/sendMessage?chat_id=" + TG_CHAT_ID +
               "&text=" + encoded +
               "&parse_mode=HTML HTTP/1.0\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n";
  tgClient.print(req);

  unsigned long t0 = millis();
  // FIX: HTTP/1.0 + Connection: close -> serwer potrafi zamknac PO
  // wyslaniu odpowiedzi; connected()==false a bajty wciaz w buforze. Stary
  // warunek gubil "ok":true i log wygladal jak porazka mimo dostarczenia.
  while ((tgClient.available() || tgClient.connected()) && millis() - t0 < 5000) {
    if (tgClient.available()) {
      String line = tgClient.readStringUntil('\n');
      if (line.indexOf("\"ok\":true") >= 0) {
        Serial.println(F("[TG] Wyslano OK"));
        tgResult.ok=true;
        break;
      }
    }
    yield();
  }
  tgClient.stop();
}
#endif

#include "online_notifications.h"

// ============== Alert manager ==============
static void weatherResponseNotify(NotifyLevel level,const char* title,const String& message) {
  notifyEvent(level,title,message.c_str()); // queue SMS/alarm even when Telegram is disabled
#if USE_TELEGRAM
  // This installation intentionally leaves g_sendTelegram null.
  telegramSend(String(title)+": "+message,true);
#endif
}
static void weatherResponseAlertsTick() {
  static bool episode=false, watching=false, blockedSent=false;
  static uint8_t announcedHard=0;
  static uint32_t lastBlocked=0;
  const bool requested=weatherCloseRequested();
  const bool closed=roofState==RoofState::CLOSED && limitClosedActive() && !limitOpenActive();
  const bool moving=astro_roof::moving(roofState);
  const uint32_t now=millis();
  const uint8_t hardMask=((g_weatherPolicy.rain&&g_rain)?1:0)|((g_weatherPolicy.wind&&g_windUnsafeLatched)?2:0)|((g_weatherPolicy.clouds&&g_denseCloud.latched)?4:0);
  if(requested && (!episode || (hardMask & ~announcedHard))) {
    announcedHard|=hardMask;
    episode=true; watching=!closed; blockedSent=false;
    String message=String("Zla pogoda: ")+((g_weatherPolicy.rain&&g_rain)?String("DESZCZ"):(g_weatherPolicy.wind&&g_windUnsafeLatched)?String("WIATR"):g_reason)+". Dach: "+roofStateName(roofState)+
      (closed?". CLOSED potwierdzone krancowka.":". Zadanie CLOSE; wykonanie zalezy od geometrii i aktywnych blokad. To nie jest potwierdzenie zamkniecia. NINA powinna przerwac sesje i zaparkowac montaz.");
    weatherResponseNotify(closed?NOTIFY_WARN:NOTIFY_CRITICAL,"POGODA",message);
    return;
  }
  if(!requested) { episode=false; announcedHard=0; }
  // Track closure even if weather clears before the roof reaches CLOSED.
  if(watching && closed) {
    watching=false;
    weatherResponseNotify(NOTIFY_INFO,"DACH","Zamkniecie pogodowe: CLOSED potwierdzone krancowka.");
  } else if(watching && !moving && !closed &&
      (!blockedSent || uint32_t(now-lastBlocked)>=300000UL)) {
    blockedSent=true;lastBlocked=now;
    String message=String("NIE ZAMKNIETO dachu. Stan: ")+roofStateName(roofState)+
      ". Blokada: "+(g_lastRoofBlockReason.length()?g_lastRoofBlockReason:g_roofMotionFault)+
      ". Sprawdz instalacje; alarm nie potwierdza ruchu.";
    weatherResponseNotify(NOTIFY_CRITICAL,"DACH",message);
  }
}

static void checkAndSendAlerts(bool wasUnsafe) {
#if USE_TELEGRAM
  unsigned long now = millis();

  // Przejscie SAFE -> UNSAFE
  if (g_unsafe && !wasUnsafe) {
    String alertMsg = "OBSERWATORIUM UNSAFE!\nPowod: " + g_reason +
                      "\nDach: " + roofStateName(roofState);
#if USE_TELEGRAM
    // ANTYSPAM: cooldown obowiazuje ZAWSZE, takze gdy stan migocze
    // SAFE<->UNSAFE na granicy progu (wczesniej kazde migniecie = nowy alert).
    //
    // ANTYSPAM-2: alert UNSAFE ma sens tylko wtedy, gdy jest CO chronic.
    // Gdy dach jest ZAMKNIETY i nie ma sesji (NINA milczy), zachmurzenie w bialy dzien
    // niczym nie grozi -> nie zawracamy glowy. Alert leci gdy dach NIE jest zamkniety
    // (otwarty/w ruchu/ERROR) albo gdy trwa sesja (NINA odpytuje Alpaca).
    // Ustaw -D ALERT_ONLY_WHEN_ACTIVE=0 w secrets, by wrocic do alertow zawsze.
  #ifndef ALERT_ONLY_WHEN_ACTIVE
    #define ALERT_ONLY_WHEN_ACTIVE 1
  #endif
  #if ALERT_ONLY_WHEN_ACTIVE
    bool alertMatters = (roofState != RoofState::CLOSED) || ninaIsActive();
  #else
    bool alertMatters = true;
  #endif
    if (alertMatters && (g_lastAlertMs == 0 || now - g_lastAlertMs > ALERT_COOLDOWN_MS)) {
      telegramSend(alertMsg, true);
      g_alertUnsafeSent = true;
      g_lastAlertMs = now;
    }
#endif
  }
  if (!g_unsafe) g_alertUnsafeSent = false;

  // Deszcz
  // ANTY-SPAM: "DESZCZ WYKRYTY" raz na epizod. g_rain "mruga" przy kapiacym
  // deszczu (wet-hold 30 s), wiec re-uzbrajamy alert dopiero gdy deszcz USTAL na dobre
  // - sucho nieprzerwanie przez RAIN_REARM_MS. Dotyka WYLACZNIE powiadomien;
  // zamykanie i trzymanie dachu pozostaje bez zmian.
  static unsigned long g_rainDrySinceMs = 0;
  const unsigned long RAIN_REARM_MS = 30UL * 60UL * 1000UL;   // 30 min sucho = uzbroj ponownie
  if (g_weatherPolicy.rain && g_rain && !g_alertRainSent) {
    String msg = "DESZCZ WYKRYTY! Dach: " + String(roofStateName(roofState));
#if USE_TELEGRAM
    telegramSend(msg, true);  // FIX: force=true - deszcz nie moze zginac
#endif
    g_alertRainSent = true;
  }
  if (g_rain) {
    g_rainDrySinceMs = 0;                                   // pada - kasujemy licznik suchosci
  } else {
    if (g_rainDrySinceMs == 0) g_rainDrySinceMs = now;     // pierwszy moment bez deszczu
    if (g_alertRainSent && (now - g_rainDrySinceMs >= RAIN_REARM_MS))
      g_alertRainSent = false;                             // sucho na dobre -> uzbroj na nastepny epizod
  }

  // Silny wiatr (podmuch lub trwala srednia ponad limit)
  bool windAlert = g_weatherPolicy.wind && g_anemometerEnabled &&
                   ((g_windGust > WIND_GUST_LIMIT_MS) || (g_windSustained > WIND_SUSTAINED_LIMIT_MS));
  if (windAlert && !g_alertWindSent) {
    // ANTYSPAM:
    //  a) cooldown 15 min miedzy alertami wiatru (koniec bombardowania przy oscylacji)
    //  b) dedup: jesli przed chwila (<60s) poszedl UNSAFE z powodem WIATR, nie dubluj
    bool cooldownOk = (g_lastWindAlertMs == 0 || now - g_lastWindAlertMs > ALERT_COOLDOWN_MS);
    bool dupOfUnsafe = (g_unsafe && g_reason.indexOf("WIATR") >= 0 && (now - g_lastAlertMs) < 60000UL);
    if (cooldownOk && !dupOfUnsafe) {
      char buf[96];
      snprintf(buf, sizeof(buf), "SILNY WIATR: srednia %.1f m/s, podmuch %.1f m/s (limit %.0f/%.0f)!",
               g_windSustained, g_windGust, WIND_SUSTAINED_LIMIT_MS, WIND_GUST_LIMIT_MS);
#if USE_TELEGRAM
      telegramSend(String(buf), true);
#endif
      g_lastWindAlertMs = now;
    }
    g_alertWindSent = true;   // niezaleznie od wysylki: nie probuj w kolko w tym epizodzie
  }
  if (g_windSustained < WIND_SAFE_RESET_MS && g_windGust < WIND_SUSTAINED_LIMIT_MS) g_alertWindSent = false;

  // Awaria anemometru
  static bool s_windFaultAlertSent = false;
  if (g_weatherPolicy.wind && g_anemometerEnabled && g_windFault && !s_windFaultAlertSent) {
#if USE_TELEGRAM
    telegramSend("AWARIA ANEMOMETRU: brak wiarygodnego pomiaru wiatru (sprawdz czujnik/zasilanie/kabel).", true);
#endif
    s_windFaultAlertSent = true;
  }
  if (!g_windFault) s_windFaultAlertSent = false;
#endif
}

// FIX BUG1: flaga oczekujacego zamkniecia gdy UNSAFE zastanal dach w trakcie OPENING.
// Po roofStop() lastUnsafeEdge=true wiec blok SAFE->UNSAFE nigdy sie nie wyzwoli ponownie.
// g_pendingAutoClose gwarantuje ze roofClose() zostanie wywolany w kolejnym cyklu.


// ============== Auto-recovery czujnikow I2C () ==============
// Czujniki I2C (MLX90614, BMP280/180, BH1750, SI1145) potrafia "odpasc" przy
// starcie lub w trakcie (dlugie kable, slabe pull-upy, zaklocenia). Tu cyklicznie
// (co SENSOR_RETRY_MS) probujemy je reinicjalizowac, gdy sa oznaczone jako padniete.
//
// BEZPIECZENSTWO:
//  - RG11 (deszcz) i anemometr NIE sa na I2C -> reset magistrali ich nie dotyka,
//    czyli dwa najwazniejsze wejscia bezpieczenstwa dzialaja niezaleznie.
//    mogloby chwilowo szarpnac wyjsciami przekaznikow. Awaria I2C moze tez
//    przerwac dostep do lokalnego TCA9554, wiec jego wyjscie swiatla pozostaje OFF.
//  - Reset calej magistrali (Wire.end/begin) robimy TYLKO gdy padly >=2 czujniki
//    (objaw problemu na poziomie busa). Przy jednym - tylko jego pojedynczy begin().
//  - Piny I2C bez zmian (uzywamy istniejacych I2C_SDA_PIN / I2C_SCL_PIN).
#ifndef SENSOR_RETRY_MS
  #define SENSOR_RETRY_MS 30000UL
#endif
#ifndef MISSING_MODULE_RETRY_MS
  #define MISSING_MODULE_RETRY_MS 10000UL
#endif
unsigned long g_lastSensorRetry = 0;
unsigned long g_lastMissingModuleRetry = 0;
int g_mlxNanStreak  = 0;   // kolejne nieudane odczyty MLX1 (zanik w trakcie)
int g_mlx2NanStreak = 0;   // kolejne nieudane odczyty MLX2 (osobny licznik failovera)
int g_baroBadStreak = 0;   // kolejne nieudane/nierealne odczyty barometru

static inline bool pressurePlausible(float hPa) { return (hPa >= 300.0f && hPa <= 1100.0f); }

static void sensorRecoveryTick(unsigned long now) {
  if(astro_roof::moving(roofState)) return;
  if (now - g_lastSensorRetry < SENSOR_RETRY_MS) return;
  g_lastSensorRetry = now;

  // policz padniete czujniki I2C objete recovery
  int down = 0;
  if (ENABLE_MLX90614 && !mlxOK) down++;
  if (ENABLE_MLX90614_2 && !mlx2OK) down++;   // 
#if ENABLE_BME280 || ENABLE_BMP280
  if (g_baroType == BARO_NONE) down++;
#if ENABLE_BMP280
  else if (g_baroType == BARO_BMP280 && !bmpOK) down++;
#endif
#if ENABLE_BME280
  else if (g_baroType == BARO_BME280 && !bme280OK) down++;
#endif
#endif
#if ENABLE_BH1750
  if (!bhOK) down++;
#endif
#if ENABLE_SI1145
  if (!uvOK) down++;
#endif
#if ENABLE_TSL2591
  if (!tslOK) down++;
#endif
  if (down == 0) return;

  Serial.printf("[RECOVERY] Padnietych czujnikow I2C: %d - proba odzyskania...\n", down);

  // Reset peryferium I2C tylko przy problemie na poziomie busa (>=2 czujniki).
  if (down >= 2) {
    Wire.end();
    delay(3);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);
    Wire.setTimeOut(200);  // spojnie z setup() - patrz komentarz tam
    Serial.println(F("[RECOVERY] Zresetowano magistrale I2C (>=2 czujniki padniete)"));
  }

  if (!mlxOK) {
    if ((ENABLE_MLX90614 && mlx.begin())) { mlxOK = true; g_mlxNanStreak = 0; Serial.println(F("[RECOVERY] MLX90614 wrocil")); }
  }
  if (!mlx2OK) {   // 
    if ((ENABLE_MLX90614_2 && mlx2.begin(MLX2_I2C_ADDR))) { mlx2OK = true; g_mlx2NanStreak=0; Serial.println(F("[RECOVERY] MLX90614 #2 (0x5B) wrocil")); }
  }
  // Barometr: jesli znamy typ - reinit; jesli NONE (nie wykryl sie przy starcie) - sprobuj wykryc.
#if ENABLE_BMP280
  if (g_baroType == BARO_BMP280 && !bmpOK) {
    if (bmp.begin(0x76) || bmp.begin(0x77)) { bmpOK = true; g_baroBadStreak = 0; Serial.println(F("[RECOVERY] BMP280 wrocil")); }
  }
#endif
#if ENABLE_BME280
  if (g_baroType == BARO_BME280 && !bme280OK) {
    if (bme280.begin(0x76) || bme280.begin(0x77)) { bme280OK = true; g_baroBadStreak = 0; Serial.println(F("[RECOVERY] BME280 wrocil")); }
  } else if (g_baroType == BARO_NONE) {
    if (bme280.begin(0x76) || bme280.begin(0x77)) { bme280OK = true; g_baroType = BARO_BME280; g_baroBadStreak = 0; Serial.println(F("[RECOVERY] Wykryto BME280")); }
  }
#endif
#if ENABLE_BMP280
  if (g_baroType == BARO_NONE) {
    if (bmp.begin(0x76) || bmp.begin(0x77)) { bmpOK = true; g_baroType = BARO_BMP280; g_baroBadStreak = 0; Serial.println(F("[RECOVERY] Wykryto BMP280")); }
  }
#endif
#if ENABLE_BH1750
  if (!bhOK) {
    if (bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) { bhOK = true; Serial.println(F("[RECOVERY] BH1750 wrocil")); }
  }
#endif
#if ENABLE_SI1145
  if (!uvOK) {
    if (si1145.begin()) { uvOK = true; Serial.println(F("[RECOVERY] SI1145 wrocil")); }
  }
#endif
#if ENABLE_TSL2591
  if (!tslOK) {
    if (tsl591.begin()) {
      tslOK = true; g_tslGainIdx = 1;
      tsl591.setGain(TSL2591_GAIN_MED);
      tsl591.setTiming(TSL2591_INTEGRATIONTIME_200MS);
      Serial.println(F("[RECOVERY] TSL2591 wrocil"));
    }
  }
#endif
}

// Automatyka trzech grzalek: wylicza punkt rosy z BME280 i ustawia
// niezalezne wypelnienie PWM. Dane starsze niz 10 s lub brak Modbus = 0%.
static void heaterControlTick() {
  const uint32_t now = millis();
  const bool timeFresh = lastUpdate_Temperature != 0 && lastUpdate_Humidity != 0 &&
      lastUpdate_DewPoint != 0 && (uint32_t)(now - lastUpdate_Temperature) <= HEATER_PWM_MAX_SENSOR_AGE_MS &&
      (uint32_t)(now - lastUpdate_Humidity) <= HEATER_PWM_MAX_SENSOR_AGE_MS &&
      (uint32_t)(now - lastUpdate_DewPoint) <= HEATER_PWM_MAX_SENSOR_AGE_MS;
  g_heaterSensorValid = (g_baroType == BARO_BME280 && bme280OK) && timeFresh && isfinite(g_tAir) && isfinite(g_hAir) && isfinite(g_dewPoint) &&
      g_tAir > -40.0f && g_tAir < 60.0f && g_hAir >= 0.0f && g_hAir <= 100.0f &&
      g_dewPoint > -60.0f && g_dewPoint < 60.0f;
  g_heaterDewMarginC = g_heaterSensorValid ? (g_tAir - g_dewPoint) : NAN;

  float autoDuty = 0.0f;
  if (g_heaterSensorValid) {
    const float span = HEATER_PWM_MARGIN_ZERO_C - HEATER_PWM_MARGIN_FULL_C;
    if (span > 0.0f) {
      autoDuty = constrain((HEATER_PWM_MARGIN_ZERO_C - g_heaterDewMarginC) * 100.0f / span,
                           0.0f, 100.0f);
    }
  }

#if !PWM_MODULE_ENABLED
  for (uint8_t i = 0; i < 3; ++i) g_heaterDutyPct[i] = 0.0f;
  g_heaterPwmOnline = false;
  return;
#else
  if (!g_heaterPwmOnline) {
    for (uint8_t i = 0; i < 3; ++i) {
      g_heaterDutyPct[i] = 0.0f;
      g_heaterManual[i] = false;
      g_heaterManualPct[i] = 0.0f;
    }
    g_ledDutyPct = 0.0f;
    g_ledPwmOn = false;

    if (g_pwmOffChannel == 0) {
      if (g_heaterLastProbeMs != 0 && uint32_t(now - g_heaterLastProbeMs) < 10000UL) return;
      g_pwmOffChannel = 1;
      g_pwmOffConfirmed = true;
    }
    // Attempt each output and let HTTP and roof safety run between transactions.
    if (!heaterPwmWriteChannel(g_pwmOffChannel, 0.0f)) g_pwmOffConfirmed = false;
    if (++g_pwmOffChannel == 5) {
      g_pwmOffChannel = 0;
      g_heaterLastProbeMs = millis();
      g_heaterPwmOnline = g_pwmOffConfirmed;
      if (g_heaterPwmOnline) {
        for (auto& lastTx : g_heaterLastTxMs) lastTx = 0;
        eventLogEx("INFO", "HEATERS", "PWM module online @%u; all OFF writes acknowledged", (unsigned)HEATER_PWM_ADDR);
      }
    }
    return;
  }

  // Tryb reczny konczy sie sam: po HEATER_MANUAL_TIMEOUT_MIN albo gdy kanal wylaczono w ustawieniach.
  for (uint8_t i = 0; i < 3; ++i) {
    if (g_heaterManual[i] && (!g_heaterChannelEnabled[i] ||
        (uint32_t)(now - (uint32_t)g_heaterManualSinceMs[i]) >= (uint32_t)HEATER_MANUAL_TIMEOUT_MIN * 60000UL)) {
      g_heaterManual[i] = false; g_heaterManualPct[i] = 0.0f;
      g_heaterLastTxMs[i] = 0;
      eventLogEx("INFO", "HEATERS", "Grzalka %u: koniec trybu recznego, powrot do AUTO", (unsigned)(i + 1));
    }
  }

  bool sentAny = false;
  bool failed = false;
  static uint8_t nextChannel = 0;
  for (uint8_t offset = 0; offset < 3; ++offset) {
    const uint8_t i = (nextChannel + offset) % 3;
    float wanted = 0.0f;
    if (HEATER_PWM_ENABLED && g_heaterSensorValid && g_heaterChannelEnabled[i]) {
      if (g_heaterManual[i]) wanted = constrain(g_heaterManualPct[i], 0.0f, 100.0f);
      else if (g_heaterSensorValid) wanted = autoDuty;
    }
    if (g_heaterLastTxMs[i] == 0 || fabsf(wanted - g_heaterDutyPct[i]) >= 0.5f ||
        (uint32_t)(now - g_heaterLastTxMs[i]) >= 5000UL) {
      nextChannel = (i + 1) % 3;
      if (heaterPwmWriteChannel((uint8_t)(i + 1), wanted)) {
        g_heaterDutyPct[i] = wanted;
        g_heaterLastTxMs[i] = now;
        sentAny = true;
      } else {
        failed = true;
      }
      break;
    }
  }
#if LED_PWM_ENABLED
  {
    // co 5 s ponawiamy jasnosc LED (jak keep-alive grzalek) i jednoczesnie sprawdzamy lacze
    static uint32_t s_ledLastTxMs = 0;
    if (!failed && !sentAny && g_ledPwmOn &&
        (uint32_t)(now - s_ledLastTxMs) >= 5000UL) {
      s_ledLastTxMs = now;
      if (!ledPwmSet(g_ledBrightPct)) failed = true;
    }
  }
#endif
  if (failed) {
    g_heaterPwmOnline = false;
    for (uint8_t i = 0; i < 3; ++i) {
      g_heaterDutyPct[i] = 0.0f; g_heaterManual[i] = false; g_heaterManualPct[i] = 0.0f;
    }
#if LED_PWM_ENABLED
    g_ledPwmOn = false;
    g_ledDutyPct = 0.0f;
#endif
    eventLogEx("CRITICAL", "HEATERS", "PWM communication failed; automatic heating suspended; OFF pending");
  }
#endif
}

// Reczne ustawienie grzalki 1..3 (z WWW). manual=false -> powrot do AUTO.
// Wymaga: modul PWM online i kanal wlaczony w ustawieniach (checkbox "Automatyka grzalki N").
static bool heaterManualSet(int ch, bool manual, float pct) {
  if (!HEATER_PWM_ENABLED) { g_lastRoofBlockReason="HEATER_PWM_ENABLED=0"; return false; }
  if (ch < 1 || ch > 3) { g_lastRoofBlockReason = "Grzalka: numer kanalu musi byc 1..3"; return false; }
  const uint8_t i = (uint8_t)(ch - 1);
  if (!manual) {
    g_heaterManual[i] = false; g_heaterManualPct[i] = 0.0f;
    g_heaterLastTxMs[i] = 0;   // wymus natychmiastowy zapis do modulu
    return true;
  }
#if !PWM_MODULE_ENABLED
  (void)pct;
  g_lastRoofBlockReason = "Grzalki wylaczone w firmware (HEATER_PWM_ENABLED=0)";
  return false;
#else
  if (!g_heaterPwmOnline) { g_lastRoofBlockReason = "Modul PWM 4CH offline - grzalki zablokowane"; return false; }
  if (!g_heaterChannelEnabled[i]) {
    g_lastRoofBlockReason = String("Grzalka ") + ch + " jest wylaczona w ustawieniach (wlacz jej kanal i zapisz)";
    return false;
  }
  if (!isfinite(pct) || pct < 0.0f || pct > 100.0f) { g_lastRoofBlockReason = "Wypelnienie grzalki musi byc 0..100%"; return false; }
  g_heaterManual[i] = true;
  g_heaterManualPct[i] = pct;
  g_heaterManualSinceMs[i] = millis();
  g_heaterLastTxMs[i] = 0;     // wymus natychmiastowy zapis do modulu
  eventLogEx("INFO", "HEATERS", "Grzalka %d: tryb reczny %.0f%% (auto-powrot po %d min)", ch, pct, (int)HEATER_MANUAL_TIMEOUT_MIN);
  return true;
#endif
}

// ============== Safety + sensory ==============
static bool g_tslWaiting=false;
static uint32_t g_tslStarted=0, g_tslLastStart=0, g_tslLastValid=0;
static uint8_t g_tslBadReads=0;
#if ENABLE_TSL2591
static bool tslReadWord(uint8_t reg,uint16_t& value) {
  Wire.beginTransmission(TSL2591_ADDR);
  Wire.write(uint8_t(TSL2591_COMMAND_BIT | reg));
  if (Wire.endTransmission(false)!=0) return false;
  if (Wire.requestFrom(uint8_t(TSL2591_ADDR),uint8_t(2))!=2) return false;
  const uint8_t low=Wire.read(), high=Wire.read();
  value=uint16_t(low)|(uint16_t(high)<<8);
  return true;
}
#endif
static void tslTick() {
#if ENABLE_TSL2591
  const uint32_t now=millis();
  if(!tslOK) { g_tslWaiting=false; g_tslLux=NAN; return; }
  if(!g_tslWaiting) {
    if(uint32_t(now-g_tslLastStart)<1000UL || astro_roof::moving(roofState)) return;
    tsl591.enable();
    g_tslWaiting=true; g_tslStarted=g_tslLastStart=millis();
    return;
  }
  if(uint32_t(now-g_tslStarted)<240UL) return; // same wait as pinned library's 200 ms setting
  g_tslWaiting=false;
  uint16_t full=0,ir=0;
  const bool fullOK=tslReadWord(TSL2591_REGISTER_CHAN0_LOW,full);
  serviceRoofFast();
  const bool irOK=fullOK && tslReadWord(TSL2591_REGISTER_CHAN1_LOW,ir);
  tsl591.disable();
  if(!fullOK || !irOK) {
    g_tslLux=NAN;
    if(++g_tslBadReads>=3) { tslOK=false; eventLogEx("WARN","TSL","TSL2591 read failed; recovery scheduled"); }
    return;
  }
  g_tslBadReads=0;
  const bool saturated=full==0xffff || ir==0xffff;
  const float lux=saturated ? NAN : tsl591.calculateLux(full,ir);
  g_tslFull=full; g_tslIR=ir;
  g_tslLux=isfinite(lux) && lux>=0 ? lux : NAN;
  if(isfinite(g_tslLux)) g_tslLastValid=millis();
  static const tsl2591Gain_t gains[]={TSL2591_GAIN_LOW,TSL2591_GAIN_MED,TSL2591_GAIN_HIGH,TSL2591_GAIN_MAX};
  if(saturated && g_tslGainIdx>0) tsl591.setGain(gains[--g_tslGainIdx]);
  else if(!saturated && full<50 && g_tslGainIdx<3) tsl591.setGain(gains[++g_tslGainIdx]);
#endif
}

static void readSensorsAndSafety() {
  unsigned long now = millis();
  if (now - lastSensorsRead < 1000) return;  // FIX: 4000 -> 1000 ms dla szybszej detekcji deszczu (RG-11)
  lastSensorsRead = now;

  // RG11 (wet hold - zmieniono z 5 min na 30 sec dla szybszej detekcji opadów)
  // FIX BUG12: zaimplementowano RG11_RAIN_WHEN_LOW z platformio.ini (dotad martwy define)
  // RG11_RAIN_WHEN_LOW=1 (domyslnie): stan LOW na pinie = deszcz (tryb NO, zwarcie=deszcz)
  // RG11_RAIN_WHEN_LOW=0: stan HIGH na pinie = deszcz (tryb NC)
  {
    static unsigned long g_rainLastDetectedMs = 0;
    static uint8_t rainConfirm = 0;                        // anti-glitch: licznik potwierdzen
    const unsigned long RG11_WET_HOLD_MS = 30UL * 1000UL;  // FIX: 300000 -> 30000 ms (30 sec zamiast 5 min)
    // DESZCZ wymaga POTWIERDZENIA poziomem (2 kolejne odczyty, ~1-2 s), a nie golego
    // zbocza z ISR. W trybie "It's Raining" przekaznik ZATRZASKUJE sie na 30 s-5 min, wiec realny
    // deszcz i tak jest trwale LOW - nic nie tracimy. Za to pojedynczy glitch elektryczny (falszywy
    // impuls, np. przy podlaczaniu NINA/USB/przelaczaniu zasilania) juz NIE zamknie dachu.
#if RG11_RAIN_WHEN_LOW
    bool liveRain = ENABLE_RG11 && (digitalRead(RG11_PIN) == LOW);
#else
    bool liveRain = ENABLE_RG11 && (digitalRead(RG11_PIN) == HIGH);
#endif
    noInterrupts(); g_rainIsrFlag = false; interrupts();  // ISR zneutralizowana - tylko czyscimy flage

    if (liveRain) { if (rainConfirm < 2) rainConfirm++; }
    else          { rainConfirm = 0; }

    const bool rainBefore = g_rain;
    if (rainConfirm >= 2) { g_rainLastDetectedMs = now; g_rain = true; }
    else { g_rain = (g_rainLastDetectedMs > 0 && (now - g_rainLastDetectedMs) < RG11_WET_HOLD_MS); }
    if (!rainBefore && g_rain) eventLogEx("CRITICAL", "RAIN", "RG11 rain detected: pin=%u raw=%u confirmed=%u hold=30000ms", (unsigned)RG11_PIN,(unsigned)digitalRead(RG11_PIN),(unsigned)rainConfirm);
    if (rainBefore && !g_rain) eventLogEx("INFO", "RAIN", "RG11 rain cleared");
  }
  lastUpdate_RainRate = now;

  // Wiatr (chwilowy + okno kroczace: srednia trwala i podmuch + detekcja awarii)
  g_wind_ms = readWindSpeedMS();
  if(!g_windValidity.holding) windWindowPush(g_wind_ms);
  windFaultTick(now);
  if(!g_windValidity.holding) lastUpdate_WindSpeed = now;


  // Krytyczne GPIO/ADC odczytane przed pomocniczym I2C.
  // Najpierw sprobuj odzyskac padniete czujniki I2C (gated wewnatrz co 30s)
  sensorRecoveryTick(now);

#if ENABLE_BH1750
  // BH1750
  if (bhOK) {
    float lux = bh1750.readLightLevel();
    if (!isnan(lux) && lux >= 0.0f) g_skyLux = lux;
  }
#endif

  bool* available[2]={&mlxOK,&mlx2OK};
  Adafruit_MLX90614* devices[2]={&mlx,&mlx2};
  static bool failureNotified[2]={};
  for (int i=0;i<2;++i) {
    if (*available[i]) {
      const float object=devices[i]->readObjectTempC();
      serviceRoofFast();
      const float ambient=devices[i]->readAmbientTempC();
      const bool good=g_mlxSamples[i].update(object,ambient,millis());
      if (good) {
        if (failureNotified[i]) eventLogEx("INFO","MLX","MLX%d valid readings recovered",i+1);
        failureNotified[i]=false;
      } else if (g_mlxSamples[i].bad>=3) *available[i]=false;
    }
    if (!g_mlxSamples[i].usable(millis()) && !failureNotified[i]) {
      failureNotified[i]=true;
      const String message=String("MLX")+String(i+1)+": brak wiarygodnej swiezej pary temperatur";
      eventLogEx("ERROR","MLX","%s",message.c_str());
#if USE_TELEGRAM
      telegramSend(message,true);
#endif
    }
    serviceRoofFast();
  }
  const int previous=g_useMlx2AsFailover?1:0;
  const int selected=astro_roof::selectMlx(previous,g_mlxSamples[0],g_mlxSamples[1],millis());
  bool activeOk=selected>=0;
  if (activeOk) {
    g_useMlx2AsFailover=selected==1;
    const auto& sample=g_mlxSamples[selected];
    g_tempSky=sample.sky+g_mlxSkyOff; g_tempAmb=sample.ambient+g_mlxAmbOff;
    lastUpdate_SkyTemperature=sample.at; // never stamp an old sample as new
    if (selected!=previous) {
      g_acPrevRaw=NAN;
      const String message=String("MLX: przelaczenie MLX")+String(previous+1)+" -> MLX"+String(selected+1)+"; swieze dane, praca kontynuowana";
      eventLogEx("WARN","MLX","%s",message.c_str());
#if USE_TELEGRAM
      telegramSend(message,true);
#endif
    }
  } else { g_tempSky=NAN; g_tempAmb=NAN; }
  const auto& backup=g_mlxSamples[1];
  g_tempSky2=backup.usable(millis()) ? backup.sky+g_mlxSkyOff : NAN;
  g_tempAmb2=backup.usable(millis()) ? backup.ambient+g_mlxAmbOff : NAN;

  // ═══ delta z korekcja AAG + referencja POWIETRZA (BME280) ═══
  // delta_raw (diag) = stara definicja: T_amb(MLX) - T_sky. Do porownania w UI.
  g_cloudDeltaRaw = (!isnan(g_tempAmb) && !isnan(g_tempSky)) ? (g_tempAmb - g_tempSky) : NAN;
  {
    // Referencja otoczenia: T powietrza z BME280/DHT (g_tAir, wolnozmienna,
    // odczyt z poprzedniego cyklu ~1 s temu - OK). Fallback: puszka MLX.
    float ambRef = isfinite(g_tAir) && lastUpdate_Temperature &&
        uint32_t(millis()-lastUpdate_Temperature)<=astro_roof::MLX_MAX_AGE_MS ? g_tAir : g_tempAmb;
    float skyC   = g_tempSky;
    float rawAir = (!isnan(ambRef) && !isnan(skyC)) ? (ambRef - skyC) : NAN;  // surowa delta OD POWIETRZA

    if (g_autoCloud) {
      // ── AUTO: samoucząca linia czystego nieba (bez K, bez recznego strojenia) ──
      // RG11 jest juz odczytany w tym cyklu; nie ucz podczas deszczu.
      acDecayTick(now);
      acLearn(ambRef, rawAir, !g_rain, activeOk);
      g_cloudDelta = acNormalize(rawAir, ambRef, activeOk);   // ustawia g_acExpected/g_acGap/g_acTrusted
      g_skyTd = NAN;
      if (!isnan(g_cloudDelta)) lastUpdate_CloudCover = lastUpdate_SkyTemperature;
    } else if (g_skyCorrEnabled && !isnan(skyC) && !isnan(ambRef)) {
      // ── Legacy AAG (uzywane TYLKO gdy auto wylaczone) ──
      float Td = (g_skyK1 / 100.0f) * (ambRef - g_skyK2 / 10.0f)
               + (g_skyK3 / 100.0f) * powf(expf((g_skyK4 / 1000.0f) * ambRef), g_skyK5 / 100.0f);
      skyC -= Td; g_skyTd = Td;
      g_cloudDelta = ambRef - skyC;
      lastUpdate_CloudCover = lastUpdate_SkyTemperature;
      g_acTrusted=false; g_acExpected=NAN; g_acGap=NAN;
    } else {
      // ── Surowa delta od powietrza (auto OFF, AAG OFF) ──
      g_skyTd = NAN;
      g_cloudDelta = rawAir;
      if (!isnan(rawAir)) lastUpdate_CloudCover = lastUpdate_SkyTemperature;
      g_acTrusted=false; g_acExpected=NAN; g_acGap=NAN;
    }
    if (isnan(ambRef) || isnan(skyC)) g_cloudDelta = NAN;

    // autozapis nauki do NVS (rzadko, zeby nie meczyc flasha)
    if (g_autoCloud && g_acDirty && (g_acLastSaveMs==0 || (now - g_acLastSaveMs) >= AC_SAVE_MS)) {
      g_acLastSaveMs = now;
      if (g_prefs.begin("obscfg", false)) {
        g_prefs.putBytes("acClr",  g_acClr,  sizeof(g_acClr));
        g_prefs.putBytes("acHits", g_acHits, sizeof(g_acHits));
        g_prefs.end();
        g_acDirty = false;
      }
    }
  }

  // ── ODCZYT TEMPERATURY / WILGOTNOSCI / CISNIENIA ─────────────────
  // Priorytet: BME280 czyta wszystko (temp + hum + pressure) jednym odczytem.
  // DHT22 wskakuje jako backup temp/hum gdy BME280 niedostepny lub zwraca NaN.
  // BMP280 jako backup cisnienia gdy BME280 niedostepny.

  bool bmeGaveTemp = false, bmeGaveHum = false;

#if ENABLE_BME280
  if (g_baroType == BARO_BME280 && bme280OK) {
    float t   = bme280.readTemperature();
    float h   = bme280.readHumidity();
    float p   = bme280.readPressure();
    float hPa = isnan(p) ? NAN : p / 100.0f;

    // temperatura
    if (!isnan(t) && t > -40.0f && t < 85.0f) {
      g_tAir = t; lastUpdate_Temperature = now; bmeGaveTemp = true;
      focusBmeTemperature.sample(t, now, -40.0f, 85.0f);
    } else { focusBmeTemperature.valid = false; Serial.println(F("!! BME280 blad temp")); }

    // wilgotnosc
    if (!isnan(h) && h >= 0.0f && h <= 100.0f) {
      g_hAir = h; lastUpdate_Humidity = now; bmeGaveHum = true;
    } else { Serial.println(F("!! BME280 blad wilgotnosci")); }

    // cisnienie
    if (!isnan(hPa) && pressurePlausible(hPa)) {
      g_pressure_hPa = hPa; lastUpdate_Pressure = now; g_baroBadStreak = 0;
    } else if (++g_baroBadStreak >= 3) {
      bme280OK = false;
      Serial.println(F("!! BME280 zwraca nierealne cisnienie -> recovery"));
    }

    // punkt rosy (juz tutaj, bo mamy oba odczyty)
    if (bmeGaveTemp && bmeGaveHum) {
      g_dewPoint = calculateDewPoint(g_tAir, g_hAir);
      if (!isnan(g_dewPoint)) lastUpdate_DewPoint = now;
    }
  }
#endif

#if ENABLE_DHT22
  // DHT22 - backup temp/hum gdy BME280 nie dal wartosci (lub niedostepny)
  bool dhtNeeded = (g_baroType != BARO_BME280 || !bme280OK || !bmeGaveTemp || !bmeGaveHum);
  (void)dhtNeeded;
  if (!dhtOK) focusDhtTemperature.valid = false;
  { // Also sample DHT for the focuser when BME is healthy; weather priority below is unchanged.
    if (dhtOK) {
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      focusDhtTemperature.sample(t, now, -40.0f, 80.0f);
      if (!bmeGaveTemp) {
        if (!isnan(t)) { g_tAir = t; lastUpdate_Temperature = now; }
        else { dhtOK = false; Serial.println(F("??  DHT22 blad temp")); }
      }
      if (!bmeGaveHum) {
        if (!isnan(h)) { g_hAir = h; lastUpdate_Humidity = now; }
        else { dhtOK = false; }
      }
      // przelicz punkt rosy jesli BME280 go nie wyznaczyl
      if (!bmeGaveTemp || !bmeGaveHum) {
        g_dewPoint = calculateDewPoint(g_tAir, g_hAir);
        if (!isnan(g_dewPoint)) lastUpdate_DewPoint = now;
      }
    } else {
      dht.begin();
      float t = dht.readTemperature();
      if (!isnan(t)) { dhtOK = true; Serial.println(F("DHT22 przywrocony")); }
    }
  }
#endif



#if ENABLE_BMP280
  // Barometr BMP280 - tylko gdy BME280 niedostepny (cisnienie backup)
  if (g_baroType == BARO_BMP280 && bmpOK) {
    float p = bmp.readPressure();
    float hPa = isnan(p) ? NAN : p / 100.0f;
    if (!isnan(hPa) && pressurePlausible(hPa)) { g_pressure_hPa = hPa; lastUpdate_Pressure = now; g_baroBadStreak = 0; }
    else if (++g_baroBadStreak >= 3) { bmpOK = false; Serial.println(F("!! BMP280 zwraca nierealne cisnienie -> oznaczam do odzyskania")); }
  }
#endif

#if ENABLE_SI1145
  // UV (SI1145)
  if (uvOK) {
    uint16_t raw = si1145.readUV();
    g_uvIndex = (raw > 0) ? (raw / 100.0f) : 0.0f;
  }
#endif

#if ENABLE_DS18B20
  // DS18B20
  if (!dsOK) focusDsTemperature.valid = false;
  if (dsOK) {
    ds18b20.setWaitForConversion(false);
    ds18b20.requestTemperatures();
    // FIX BUG4: zastapiono blokujacy delay(10) petla yield() - nie blokuje loop()
    unsigned long t0ds = millis();
    while (!ds18b20.isConversionComplete() && millis() - t0ds < 800) { yield(); }
    float t = ds18b20.getTempCByIndex(0);
    focusDsTemperature.sample(ds18b20.isConversionComplete() ? t : NAN, now, -55.0f, 125.0f);
    if (t > -100.0f && t < 125.0f) g_tExt = t;
    else { dsOK = false; Serial.println(F("??  DS18B20 blad")); }
  } else {
    ds18b20.begin();
    if (ds18b20.getDeviceCount() > 0) {
      dsOK = true;
      ds18b20.setResolution(10);
      Serial.println(F("DS18B20 przywrocony"));
    }
  }
#endif

  // ============================================================
  // LOGIKA BEZPIECZENSTWA
  // Priorytety: brak modulu Modbus dachu i aktywnych wyjsc bezpieczenstwa,
  // nastepnie czujniki pogody, wiatr i Park Guard.
  // ============================================================
  bool wasUnsafe = g_unsafe;
  g_unsafe = false;
  g_reason = "";

  // Brakujace moduly odpytujemy co 10 s; proba Modbus czeka na odpowiedz RTU,
  // wiec retry co sekunde niepotrzebnie opoznialoby WWW na golym ESP.
  if (!modbusRelayOK &&
      now - g_lastMissingModuleRetry >= MISSING_MODULE_RETRY_MS) {
    g_lastMissingModuleRetry = now;
    // ═══ WAVESHARE 17658: auto-restart Modbus RTU relay ═══
    if (!modbusRelayOK && modbusRelayBegin()) {
      for (uint8_t ch = 0; ch <= 7; ch++) relayState[ch] = false;
#if MOUNT_POWER_RELAY_ENABLED
      relayState[MOUNT_POWER_RELAY_CH]=g_mountPowerRestoreBlocked;
#endif
    }

  }

  auto setUnsafe = [&](const String& reason) {
    g_unsafe = true;
    if (g_reason.length()) g_reason += "; ";
    g_reason += reason;
  };

  if (!ROOF_MOTION_ENABLED) setUnsafe("TRYB STOLOWY: ruch dachu wylaczony");
  if (!modbusRelayOK) setUnsafe("BRAK WAVESHARE 17658 - MODBUS RELAY offline");
  if (roofState==RoofState::ERROR) setUnsafe("BLAD DACHU: "+g_roofMotionFault);

#if MOUNT_LIMIT_ENABLED
  // E-STOP montazu -> UNSAFE. NINA (SafetyMonitor) zobaczy issafe=false i
  // sama przerwie/zaparkuje. Otwieranie/zamykanie blokowane osobno w roofOpen/roofClose.
  if (g_mountEstop) setUnsafe(g_mountEstopReason.length() ? ("E-STOP MONTAZU: " + g_mountEstopReason) : "E-STOP MONTAZU");
#endif

#if COMMS_WATCHDOG_ENABLED && COMMS_LOST_AUTOPARK
  // utrata laczności z NINA -> UNSAFE (uruchamia auto-zamykanie respektujace
  // Park Guard). Skuteczne auto-domkniecie wymaga, by ESP znal park z reed switcha
  // Lokalny czujnik parku pozostaje dostepny, gdy Alpaca jest nieosiagalna.
  if (g_commsLost) setUnsafe("Utrata laczności z NINA (autopark)");
#endif

  // ── CHMURY (MLX90614) — graded; male chmury NIE robia UNSAFE ─────────────
  // Klasyfikacja wizualna (dla UI/NINA) niezaleznie od decyzji safety:
  if (!activeOk || !isfinite(g_cloudDelta)) {
    g_skyCondition = SkyCondition::UNKNOWN;
#if MLX_FAIL_UNSAFE
    if(g_weatherPolicy.clouds) setUnsafe(activeOk ? "MLX: brak danych (NAN)" : "BRAK aktywnego MLX90614");
#endif
    // bez MLX_FAIL_UNSAFE: chmury sa nieznane, ale NIE blokujemy dachu
    g_cloudOvercastSinceMs = 0;
    g_cloudUnsafeLatched = false;
  } else {
    if      (g_cloudDelta >= CLOUD_DELTA_CLEAR)     g_skyCondition = SkyCondition::CLEAR;
    else if (g_cloudDelta >= CLOUD_DELTA_SCATTERED) g_skyCondition = SkyCondition::SCATTERED;
    else if (g_cloudDelta >= CLOUD_DELTA_OVERCAST)  g_skyCondition = SkyCondition::CLOUDY;
    else                                            g_skyCondition = SkyCondition::CRITICAL;

    if (g_cloudCanTriggerUnsafe) {
      // Chmury -> UNSAFE TYLKO przy trwalym, gestym zachmurzeniu (histereza):
      if (g_cloudDelta < CLOUD_DELTA_OVERCAST) {
        if (g_cloudOvercastSinceMs == 0) g_cloudOvercastSinceMs = now;
        if ((now - g_cloudOvercastSinceMs) >= uint32_t(g_weatherPolicy.cloudHoldSec)*1000UL) g_cloudUnsafeLatched = true;
      } else {
        g_cloudOvercastSinceMs = 0;  // delta wrocila powyzej OVERCAST -> zeruj licznik
        if (g_cloudDelta >= CLOUD_DELTA_OVERCAST_CLEAR) g_cloudUnsafeLatched = false; // histereza
      }
      if (g_cloudUnsafeLatched) {
        char buf[64];
        snprintf(buf, sizeof(buf), "GESTE CHMURY (trwale, delta=%.1f C)", g_cloudDelta);
        setUnsafe(String(buf));
      }
    } else {
      g_cloudOvercastSinceMs = 0;
      g_cloudUnsafeLatched = false;
    }
  }

  // ── DESZCZ (RG11) — priorytet bezwzgledny, bez tolerancji ────────────────
  if (g_weatherPolicy.rain && g_rain) setUnsafe("DESZCZ (RG11)");
  if (g_weatherPolicy.rain && !ENABLE_RG11) setUnsafe("WYBRANO RG11, ALE BRAK OBSLUGI W FIRMWARE");

  // ── WIATR — lekki/umiarkowany SAFE; liczy sie trwaly wiatr + podmuch ─────
  // Histereza: zatrzaskujemy UNSAFE i czyscimy dopiero gdy srednia spadnie
  // ponizej WIND_SAFE_RESET_MS (a podmuch ponizej limitu trwalego).
  if (!g_weatherPolicy.wind || !g_anemometerEnabled) {
    // anemometr wylaczony (np. commissioning) -> wiatr NIE wplywa na bezpieczenstwo
    // Retain an existing high-wind latch even if monitoring is disabled.
  } else if (!windMeasurementUsable()) {
    setUnsafe(String("ANEMOMETR: ")+windStatusText());
  } else if (g_windFault) {
#if WIND_FAULT_UNSAFE
    setUnsafe("AWARIA ANEMOMETRU - brak pomiaru wiatru");
#endif
    // przy awarii nie ufamy odczytom -> nie modyfikujemy zatrzasku wiatru
  } else {
    bool gustTrip      = (g_windGust > WIND_GUST_LIMIT_MS);
    bool sustainedTrip = (g_windSustained > WIND_SUSTAINED_LIMIT_MS);
    if (gustTrip || sustainedTrip) {
      g_windUnsafeLatched = true;
    } else if (g_windSustained < WIND_SAFE_RESET_MS && g_windGust < WIND_SUSTAINED_LIMIT_MS) {
      g_windUnsafeLatched = false;  // histereza: wyraznie spokojnie -> czyscimy
    }
    if (g_windUnsafeLatched) {
      char buf[72];
      snprintf(buf, sizeof(buf), "WIATR trwaly %.1f / podmuch %.1f m/s (limit %.0f/%.0f)",
               g_windSustained, g_windGust, WIND_SUSTAINED_LIMIT_MS, WIND_GUST_LIMIT_MS);
      setUnsafe(String(buf));
    }
  }

  if (g_weatherPolicy.wind && g_windUnsafeLatched) setUnsafe("WIATR: zatrzask ochrony aktywny");
  g_denseCloud.update(activeOk && isfinite(g_cloudDelta),g_cloudDelta,now);
  if (g_weatherPolicy.clouds && g_denseCloud.latched) setUnsafe("BARDZO GESTE ZACHMURZENIE");
  g_moisture.update(g_weatherPolicy,weatherThermoReady(),g_hAir,g_tAir-g_dewPoint,now);
  if(!policySkyReady())setUnsafe("BRAK WIARYGODNEGO MLX W OCHRONIE POGODY");
  if(!policyWindReady())setUnsafe("BRAK WIARYGODNEGO ANEMOMETRU W OCHRONIE POGODY");
  if(g_weatherPolicyFault)setUnsafe("BLAD KONFIGURACJI POLITYKI POGODY");
  if((g_weatherPolicy.humidity || g_weatherPolicy.dew) && !weatherThermoReady())setUnsafe("BRAK AKTUALNEJ TEMPERATURY/WILGOTNOSCI/PUNKTU ROSY");
  if(g_weatherPolicy.humidity && g_moisture.humidLatched)setUnsafe("TRWALA WYSOKA WILGOTNOSC");
  if(g_weatherPolicy.dew && g_moisture.dewLatched)setUnsafe("TRWALE RYZYKO ROSY");
  g_weatherOpenReady=g_weatherStable.update(!g_unsafe && safetyInputsUsable(),now,uint32_t(g_weatherPolicy.reopenSec)*1000UL);

  // Park: warunek OPEN i CLOSE bez przeswitu; brak parku w sesji z otwartym dachem jest normalny.
  // Logujemy stan dla UI
  if (!g_telescopeParked && g_parkSensorOK) {
    Serial.println(F("[PARK] UWAGA: Teleskop nie w parku!"));
  }

  // Wilgotnosc / rosa - ostrzezenia (BME280 lub DHT22 jako zrodlo)
  if ((bme280OK || dhtOK) && !isnan(g_tAir) && !isnan(g_dewPoint)) {
    float margin = g_tAir - g_dewPoint;
    if (margin < DEW_POINT_MARGIN) Serial.printf("??  ROSA: margines=%.1f degC\n", margin);
  }

  // Log co 10s
  static unsigned long lastLog = 0;
  if (now - lastLog > 10000) {
    lastLog = now;
    Serial.println(F("\n===== WEATHER / SAFETY ====="));
    Serial.printf("T_air: %.1f C | RH: %.0f %% | Dew: %.1f C\n", g_tAir, g_hAir, g_dewPoint);
    Serial.printf("T_sky: %.1f C | T_amb: %.1f C | Delta: %.1f C\n", g_tempSky, g_tempAmb, g_cloudDelta);
    Serial.printf("Press: %.0f hPa | Lux: %.0f | UV: %.1f\n", g_pressure_hPa, g_skyLux, g_uvIndex);
    Serial.printf("Wind: chwil %.1f | sredni %.1f | podmuch %.1f m/s | %.2fV%s | Rain: %s\n",
                  g_wind_ms, g_windSustained, g_windGust, g_windRawVolts,
                  g_windFault ? " [AWARIA]" : "", g_rain ? "TAK" : "NIE");
    Serial.printf("Roof: %s | Park: %s | windLatch=%d cloudLatch=%d\n",
                  roofStateName(roofState), g_telescopeParked ? "OK" : "!!",
                  (int)g_windUnsafeLatched, (int)g_cloudUnsafeLatched);
    Serial.printf("UNSAFE: %s\n", g_unsafe ? g_reason.c_str() : "SAFE");
  }

  // Zmiana stanu bezpieczeństwa — tylko przy przejściu, nie cyklicznie.
  if (g_unsafe != wasUnsafe) {
    eventLogEx(g_unsafe ? "CRITICAL" : "INFO", "SAFETY",
               g_unsafe ? "SAFE -> UNSAFE: %s" : "UNSAFE -> SAFE",
               g_unsafe ? g_reason.c_str() : "");
  }

  // Weather requests closure; physical faults only block movement.
  // Retry CLOSE through the ordinary interlocks after an echoed STOP and release.
  if (weatherCloseRequested()) {
    if (roofState==RoofState::OPENING) {
      g_pendingAutoClose=roofStop();
    } else if (roofState==RoofState::OPEN || roofState==RoofState::STOPPED || roofState==RoofState::UNKNOWN) {
      g_pendingAutoClose=true;
    }
    if (g_pendingAutoClose && roofState!=RoofState::ERROR &&
        roofState!=RoofState::OPENING && roofState!=RoofState::CLOSING &&
        roofState!=RoofState::CLOSED && !pulseActive[RELAY_ROOF_STOP] &&
        (!g_stopAttempted || (g_stopCommandConfirmed && g_stopReleaseConfirmed))) {
      if (roofClose()) g_pendingAutoClose=false;
    }
  } else g_pendingAutoClose=false;

  // Issue movement/STOP first; network notifications never precede the closure request.
  weatherResponseAlertsTick();
  // Alerty
  checkAndSendAlerts(wasUnsafe);

  // Historia
  historySample();
}

// ============== HTML ==============
static String currentIpString() {
  // ETH ma priorytet; aktualny adres wybiera konfiguracja W5500.
#if USE_W5500
  if (g_ethReady) return Ethernet.localIP().toString();
#endif
#if USE_WIFI
  if (g_wifiReady) return WiFi.localIP().toString();
#endif
  return String("0.0.0.0");
}

static String buildStateJson();

// ============================================================
// WEB UI — single page, roll-off roof animation
// ============================================================
#include "web_ui.h"

// ============== HTTP helpers ==============
static String urlDecode(const String& s) {
  String out; out.reserve(s.length());
  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];
    if (c == '+') out += ' ';
    else if (c == '%' && i + 2 < (int)s.length()) {
      auto hexVal = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
        if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
        return -1;
      };
      int v1 = hexVal(s[i + 1]), v2 = hexVal(s[i + 2]);
      if (v1 >= 0 && v2 >= 0) { out += (char)((v1 << 4) | v2); i += 2; }
      else out += c;
    } else out += c;
  }
  return out;
}

static String toLowerCopy(String s) { s.toLowerCase(); return s; }

static String getParamValue(const String& params, const String& key) {
  String want = toLowerCopy(key);
  int start = 0;
  while (start < (int)params.length()) {
    int amp = params.indexOf('&', start);
    if (amp < 0) amp = params.length();
    String part = params.substring(start, amp);
    int eq = part.indexOf('=');
    String k = (eq >= 0) ? part.substring(0, eq) : part;
    String v = (eq >= 0) ? part.substring(eq + 1) : "";
    k = toLowerCopy(urlDecode(k));
    if (k == want) return urlDecode(v);
    start = amp + 1;
  }
  return "";
}

static uint32_t getParamUInt(const String& params, const String& key, uint32_t defVal) {
  String v = getParamValue(params, key);
  if (!v.length()) return defVal;
  return (uint32_t)v.toInt();
}

static bool getParamBool(const String& params, const String& key, bool defVal, bool* found = nullptr) {
  String v = getParamValue(params, key);
  if (!v.length()) { if (found) *found = false; return defVal; }
  if (found) *found = true;
  String vl = toLowerCopy(v);
  if (vl == "true" || vl == "1" || vl == "yes" || vl == "on") return true;
  return false;
}

static float getParamFloat(const String& params, const String& key, float defVal, bool* found = nullptr) {
  String v = getParamValue(params, key);
  if (!v.length()) { if (found) *found = false; return defVal; }
  if (found) *found = true;
  return v.toFloat();
}

static String jsonEscape(const String& s) {
  String out; out.reserve(s.length() + 8);
  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)(uint8_t)c);
          out += buf;
        } else out += c;
    }
  }
  return out;
}

static String jsonQuote(const String& s) { return String('"') + jsonEscape(s) + '"'; }
static String jsonNumOrNull(float v, uint8_t dec = 1) {
  if (isnan(v) || isinf(v)) return "null";
  return String(v, (unsigned int)dec);
}

// ============== buildStateJson ==============
static String buildStateJson() {
  String j; j.reserve(2300);
  const bool safe = !g_unsafe && safetyInputsUsable();
  j += "{";
  j += "\"fw\":"; j += jsonQuote(FW_VERSION);
  j += ",\"build\":\"ALPACA-PWM-20261109\"";
  j += ",\"safe\":"; j += (safe ? "true" : "false");
  const String reasonNow=roofState==RoofState::ERROR ? ("BLAD DACHU: "+g_roofMotionFault) :
      g_mountEstop ? ("E-STOP MONTAZU: "+g_mountEstopReason) :
      !modbusRelayOK ? String("BRAK WAVESHARE 17658 - MODBUS offline") :
      !g_unsafe && !safetyInputsUsable() ? String("Pogoda: brak swiezych wiarygodnych danych") : g_reason;
  j += ",\"reason\":"; j += jsonQuote(reasonNow);
  j += ",\"features\":{";
  j += "\"bme280\":"; j += (ENABLE_BME280 ? "true" : "false");
  j += ",\"dht22\":"; j += (ENABLE_DHT22 ? "true" : "false");
  j += ",\"ds18b20\":"; j += (ENABLE_DS18B20 ? "true" : "false");
  j += ",\"bh1750\":"; j += (ENABLE_BH1750 ? "true" : "false");
  j += ",\"tsl2591\":"; j += (ENABLE_TSL2591 ? "true" : "false");
  j += ",\"si1145\":"; j += (ENABLE_SI1145 ? "true" : "false");
  j += ",\"bmp280\":"; j += (ENABLE_BMP280 ? "true" : "false");
  j += "}";

  j += ",\"net\":{";
  j += "\"http\":"; j += jsonQuote(String("http://") + currentIpString() + ":" + String(SERVER_PORT) + "/");
#if USE_WIFI
  j += ",\"wifi\":"; j += (g_wifiReady ? jsonQuote(WiFi.localIP().toString()) : "null");
#else
  j += ",\"wifi\":null";
#endif
#if USE_W5500
  j += ",\"eth\":"; j += (g_ethReady ? jsonQuote(Ethernet.localIP().toString()) : "null");
#else
  j += ",\"eth\":null";
#endif
  j += "}";

  j += ",\"roof\":{";
  j += "\"state\":"; j += jsonQuote(String(roofStateName(roofState)));
  j += ",\"limit_closed\":"; j += (limitClosedActive() ? "true" : "false");
  j += ",\"limit_open\":";   j += (limitOpenActive()   ? "true" : "false");
  j += ",\"motion_fault\":"; j += jsonQuote(g_roofMotionFault);
  j += ",\"fault_class\":"; j += jsonQuote(g_roofFaultKind==RoofFaultKind::COMMUNICATION?"COMMUNICATION":g_roofFaultKind==RoofFaultKind::PARK_LOST?"PARK_LOST":g_roofFaultKind==RoofFaultKind::PHYSICAL?"PHYSICAL":g_roofFaultKind==RoofFaultKind::STOP_UNCONFIRMED?"STOP_UNCONFIRMED":"NONE");
  j += ",\"stop_attempted\":"; j += g_stopAttempted?"true":"false";
  j += ",\"stop_command_confirmed\":"; j += g_stopCommandConfirmed?"true":"false";
  j += ",\"stop_release_confirmed\":"; j += g_stopReleaseConfirmed?"true":"false";
  j += ",\"stuck_checked\":"; j += (g_roofStuckChecked ? "true" : "false");
  j += "}";
  j += ",\"emergency_close_pressed\":";
  j += (digitalRead(EMERGENCY_CLOSE_PIN) == LOW ? "true" : "false");

  // rozszerzone bezpieczenstwo - E-STOP montazu, laczność z NINA, link
  j += ",\"safety_ext\":{";
  j += "\"mount_estop_enabled\":";
#if MOUNT_LIMIT_ENABLED
  j += "true";
#else
  j += "false";
#endif
  j += ",\"mount_estop\":";        j += (g_mountEstop ? "true" : "false");
  j += ",\"mount_estop_reason\":"; j += jsonQuote(g_mountEstopReason);
  j += ",\"mount_limit_raw\":";    j += (mountLimitRawTripped() ? "true" : "false");
  j += ",\"comms_wd_enabled\":";
#if COMMS_WATCHDOG_ENABLED
  j += "true";
#else
  j += "false";
#endif
  j += ",\"comms_lost\":";         j += (g_commsLost ? "true" : "false");
  j += ",\"alpaca_seen\":";        j += (g_lastAlpacaContactMs != 0 ? "true" : "false");
  j += ",\"safetymonitor_connected\":"; j += (g_smConnected ? "true" : "false");
  j += ",\"safetymonitor_seen\":"; j += (g_lastSafetyMonitorContactMs!=0 ? "true" : "false");
  j += ",\"safetymonitor_age_s\":"; j += String(g_lastSafetyMonitorContactMs ? (uint32_t)((millis() - g_lastSafetyMonitorContactMs) / 1000UL) : 0);
  j += ",\"nina_active\":";        j += (ninaIsActive() ? "true" : "false");
  j += ",\"alpaca_age_s\":";       j += String(g_lastAlpacaContactMs ? (uint32_t)((millis() - g_lastAlpacaContactMs) / 1000UL) : 0);
  j += ",\"wifi_link\":";          j += (g_wifiLinkUp ? "true" : "false");
  j += ",\"eth_link\":";           j += (g_ethLinkUp ? "true" : "false");
  j += ",\"link_up\":";            j += (g_netLinkUp ? "true" : "false");
  j += ",\"ota_enabled\":";
#if USE_OTA && USE_WIFI
  j += (g_otaReady ? "true" : "false");
#else
  j += "false";
#endif
  j += ",\"ota_active\":";
#if USE_OTA && USE_WIFI
  j += (g_otaActive ? "true" : "false");
#else
  j += "false";
#endif
  j += "}";

  j += ",\"clock_time\":"; j += jsonQuote(rtcClockText());
  j += ",\"rtc_present\":"; j += (g_rtcPresent ? "true" : "false");
  j += ",\"rtc_time_valid\":"; j += (g_rtcTimeValid ? "true" : "false");
  j += ",\"rtc_time_lost\":"; j += (g_rtcTimeLost ? "true" : "false");
  j += ",\"tca_ok\":"; j += (tcaOK ? "true" : "false");
  j += ",\"modbus_relay_ok\":"; j += (modbusRelayOK ? "true" : "false");
  j += ",\"modbus_relay_state_valid\":"; j += (modbusRelayStateValid ? "true" : "false");
  const uint32_t relayReadAge=uint32_t(millis()-g_lastModbusRelayPoll);
  const bool relayReadCurrent=modbusRelayStateValid && g_lastModbusRelayPoll!=0 && relayReadAge<=3000;
  j += ",\"modbus_relay_readback_current\":"; j += relayReadCurrent?"true":"false";
  j += ",\"modbus_relay_read_age_ms\":"; j += g_lastModbusRelayPoll?String(relayReadAge):String("null");
  j += ",\"modbus_relay_online\":"; j += ((modbusRelayOK && modbusRelayStateValid) ? "true" : "false");
  j += ",\"modbus_relay_commanded\":[";
  for (uint8_t ch = 0; ch < 8; ++ch) {
    if (ch) j += ",";
    j += relayState[ch] ? "true" : "false";
  }
  j += "]";
  j += ",\"modbus_relay_actual\":[";
  for (uint8_t ch = 0; ch < 8; ++ch) {
    if (ch) j += ",";
    j += modbusRelayStateValid ? (modbusRelayActualState[ch] ? "true" : "false") : "null";
  }
  j += "]";
  j += ",\"manual_mode\":"; j += (g_manualMode ? "true" : "false");
  j += ",\"manual_authorized\":"; j += g_manualAuthorized?"true":"false";
  j += ",\"manual_remaining_sec\":"; j += String(0UL);
  j += ",\"manual_auto_return\":false";
  j += ",\"hard_weather_block\":"; j += hardWeatherBlocksOpen()?"true":"false";
  j += ",\"open_weather_measurements_ready\":";
  j += astro_policy::weatherMeasurementsReady(uint32_t(millis()-lastSensorsRead)<=3000UL,
      policyWindReady(),
      policySkyReady())?"true":"false";
  j += ",\"weather_close_requested\":"; j += weatherCloseRequested()?"true":"false";
  j += ",\"dense_cloud_calibrated\":"; j += astro_roof::DenseCloud::enabled?"true":"false";
  j += ",\"manual_weather_allows_open\":"; j += hardWeatherBlocksOpen()?"false":"true";
  j += ",\"sky_condition\":"; j += jsonQuote(String(skyConditionName(g_skyCondition)));
  j += ",\"sky_icon\":"; j += jsonQuote(String(skyConditionIcon(g_skyCondition)));
  j += ",\"heater_needed\":"; j += ((!isnan(g_hAir) && g_hAir > HUMIDITY_LIMIT) ? "true" : "false");

  // Park Guard
  j += ",\"park_guard_enabled\":";
#if PARK_GUARD_ENABLED
  j += "true";
#else
  j += "false";
#endif
  j += ",\"telescope_parked\":"; j += (g_telescopeParked ? "true" : "false");
  j += ",\"park_sensor_ok\":";   j += (g_parkSensorOK   ? "true" : "false");
  const bool localDoKnown=localDoReadback();
  j += ",\"local_do\":{\"supported\":";j+=localDoSupported()?"true":"false";
  j += ",\"online\":";j+=localDoKnown?"true":"false";
  j += ",\"channels\":[";
  for(unsigned id=1;id<=7;++id){
    if(id>1)j+=",";
    j+="{\"id\":";j+=String(id);j+=",\"name\":";j+=jsonQuote(localDoNames[id-1]);
    j+=",\"enabled\":";j+=localDoAllowed(id)?"true":"false";
    j+=",\"on\":";j+=localDoKnown?(localDoState[id-1]?"true":"false"):"null";
    j+="}";
  }
  j+="]}";
  j += ",\"illum_requested\":";  j += (g_illumRequested ? "true" : "false");
  j += ",\"illum_enabled\":";
#if ILLUM_DO_ENABLED
  j += "true";
#else
  j += "false";
#endif
  j += ",\"illum_available\":";  j += (illumAvailable() ? "true" : "false");
  const bool parkReadyNow=parkGuardAllowsClose();
#if PARK_GUARD_ENABLED
  const bool parkRaNow=digitalRead(PARK_SENSOR_PIN)==LOW;
  const bool parkDecNow=digitalRead(PARK_SENSOR_DEC_PIN)==LOW;
  const bool parkHwStable=parkRaNow && parkDecNow && g_stablePark.ready(millis(),PARK_SETTLE_MS);
#else
  const bool parkRaNow=false, parkDecNow=false, parkHwStable=false;
#endif
  j += ",\"park_ra\":"; j += parkRaNow?"true":"false";
  j += ",\"park_dec\":"; j += parkDecNow?"true":"false";
  j += ",\"park_hw_stable\":"; j += parkHwStable?"true":"false";
  j += ",\"park_close_blocked\":"; j += !clearanceAllowsClose()?"true":"false";
  j += ",\"park_motion_ready\":"; j += parkReadyNow?"true":"false";
  j += ",\"close_clearance_confirmed\":"; j += CLOSE_CLEARANCE_CONFIRMED?"true":"false";
  j += ",\"mount_protection_blocks_close\":"; j += mountProtectionBlocksClose()?"true":"false";
  j += ",\"weather_closure_pending\":"; j += g_pendingAutoClose?"true":"false";
  const uint32_t parkDiagNow=millis();
  const bool parkAlpacaConfigured=strlen(PARK_ALPACA_HOST)>0;
  bool parkAlpacaTransport=false;
#if USE_WIFI
  parkAlpacaTransport=g_wifiReady && WiFi.status()==WL_CONNECTED;
#endif
  const bool parkAlpacaCurrent=parkAlpacaConfigured && parkAlpacaTransport &&
      g_parkAlpacaSuccessSeen && g_parkAlpacaLastAttemptOK &&
      uint32_t(parkDiagNow-g_parkAlpacaSuccessMs)<=PARK_ALPACA_DIAG_FRESH_MS;
  j += ",\"park_alpaca\":{\"configured\":"; j += parkAlpacaConfigured?"true":"false";
  j += ",\"transport_ready\":"; j += parkAlpacaTransport?"true":"false";
  j += ",\"attempted\":"; j += g_parkAlpacaAttempted?"true":"false";
  j += ",\"attempt_age_s\":"; j += g_parkAlpacaAttempted?String(uint32_t(parkDiagNow-g_parkAlpacaAttemptMs)/1000UL):String("null");
  j += ",\"success_seen\":"; j += g_parkAlpacaSuccessSeen?"true":"false";
  j += ",\"success_age_s\":"; j += g_parkAlpacaSuccessSeen?String(uint32_t(parkDiagNow-g_parkAlpacaSuccessMs)/1000UL):String("null");
  j += ",\"last_attempt_ok\":"; j += g_parkAlpacaLastAttemptOK?"true":"false";
  j += ",\"last_value\":"; j += g_parkAlpacaSuccessSeen?(g_parkAlpacaLastValue?"true":"false"):"null";
  j += ",\"current\":"; j += parkAlpacaCurrent?"true":"false";
  j += ",\"value\":"; j += parkAlpacaCurrent?(g_parkAlpacaLastValue?"true":"false"):"null";
  j += ",\"fresh_limit_s\":15}";
  j += ",\"park_show_anim\":";   j += (g_showParkAnimation ? "true" : "false");
  j += ",\"alpaca_fails\":";     j += String(g_alpacaFailCount);


  j += ",\"sensors\":{";
  j += "\"t_air\":";        j += jsonNumOrNull(g_tAir, 1);
  j += ",\"h_air\":";       j += jsonNumOrNull(g_hAir, 0);
  j += ",\"dew\":";         j += jsonNumOrNull(g_dewPoint, 1);
  j += ",\"t_sky\":";       j += jsonNumOrNull(g_tempSky, 1);
  j += ",\"t_amb\":";       j += jsonNumOrNull(g_tempAmb, 1);
  j += ",\"cloud_delta\":"; j += jsonNumOrNull(g_cloudDelta, 1);
  j += ",\"cloud_delta_raw\":"; j += jsonNumOrNull(g_cloudDeltaRaw, 1);   // 
  j += ",\"sky_td\":";       j += jsonNumOrNull(g_skyTd, 1);              // poprawka AAG
  j += ",\"ac_expected\":"; j += jsonNumOrNull(g_acExpected, 1);         // nauczone czyste niebo
  j += ",\"ac_gap\":";      j += jsonNumOrNull(g_acGap, 1);              // ile ponizej czystego
  j += ",\"ac_trusted\":";  j += (g_acTrusted ? "true" : "false");       // 
  j += ",\"ac_bins\":";     j += String(acLearnedBins());                // ile koszy nauczonych
  j += ",\"pressure\":";    j += jsonNumOrNull(g_pressure_hPa, 0);
  j += ",\"lux\":";         j += jsonNumOrNull(g_skyLux, 0);
  j += ",\"tsl_lux\":";     j += jsonNumOrNull(g_tslLastValid && uint32_t(millis()-g_tslLastValid)<=5000 ? g_tslLux : NAN, 4);   // wiecej miejsc po przecinku - male wartosci w ciemna noc licza sie
  j += ",\"tsl_full\":";    j += String((unsigned)g_tslFull);
  j += ",\"tsl_ir\":";      j += String((unsigned)g_tslIR);
  j += ",\"t_ext\":";       j += jsonNumOrNull(g_tExt, 1);
  j += ",\"uv\":";          j += jsonNumOrNull(g_uvIndex, 1);
  j += ",\"wind\":";        j += jsonNumOrNull(windMeasurementUsable()?g_wind_ms:NAN, 1);
  j += ",\"wind_avg\":";    j += jsonNumOrNull(windMeasurementUsable()?g_windSustained:NAN, 1);
  j += ",\"wind_gust\":";   j += jsonNumOrNull(windMeasurementUsable()?g_windGust:NAN, 1);
  j += ",\"wind_volts\":";  j += jsonNumOrNull(g_windRawVolts, 2);
  j += ",\"wind_fault\":";  j += (g_windFault ? "true" : "false");
  j += ",\"wind_status\":\""; j += windStatusText(); j += "\"";
  j += ",\"wind_valid\":"; j += windMeasurementUsable()?"true":"false";
  j += ",\"wind_provisional_ms\":"; j += jsonNumOrNull(g_wind_ms,2);
  j += ",\"wind_adc_raw\":"; j += jsonNumOrNull(g_windAdcRaw,1);
  j += ",\"wind_adc_min\":"; j += String(g_windAdcMin);
  j += ",\"wind_adc_max\":"; j += String(g_windAdcMax);
  j += ",\"wind_adc_rails\":"; j += String(g_windAdcRails);
  j += ",\"wind_rejected_frames\":"; j += String(g_windRejectedFrames);
  j += ",\"wind_held_frames\":"; j += String(g_windHeldFrames);
  j += ",\"wind_holding\":"; j += g_windValidity.holding?"true":"false";
  j += ",\"wind_adc_volts\":"; j += jsonNumOrNull(g_windAdcVolts,3);
  j += ",\"wind_sensor_volts\":"; j += jsonNumOrNull(g_windRawVolts,3);
  j += ",\"wind_age_ms\":"; j += g_windValidity.present?String(uint32_t(millis()-g_windValidity.sampledAt)):"null";
  j += ",\"wind_good_samples\":"; j += String(g_windValidity.goodCount);
  uint32_t windSpan=0; unsigned windCount=0;
  for(int i=0;i<g_windWindowCount;++i) {
    const uint32_t age=uint32_t(millis()-g_windWindowTimes[i]);
    if(age<64000) { ++windCount; if(age>windSpan) windSpan=age; }
  }
  j += ",\"wind_window_ms\":"; j += String(windSpan);
  j += ",\"wind_window_samples\":"; j += String(windCount);
  j += ",\"wind_stable_warning\":"; j += (g_windValidity.present && uint32_t(millis()-g_windStableSince)>300000)?"true":"false";
  j += ",\"wind_adc_calibrated\":"; j += g_windValidity.adcCalibrated?"true":"false";
  j += ",\"wind_cal\":{\"gain\":"; j += String(g_windCal.gain,6);
  j += ",\"offset\":"; j += String(g_windCal.offset,6);
  j += ",\"ratio\":"; j += String(g_windCal.ratio,6);
  j += ",\"zero\":"; j += String(g_windCal.zero,6);
  j += ",\"top\":"; j += String(g_windCal.top,6);
  j += ",\"maxMS\":"; j += String(g_windCal.maxMS,3);
  j += ",\"model\":"; j += jsonQuote(String(g_windCal.model));
  j += ",\"verified\":"; j += g_windCal.verified?"true}":"false}";

  j += ",\"mlx_ok\":";      j += (mlxOK ? "true" : "false");
  j += ",\"t_sky2\":";      j += jsonNumOrNull(g_tempSky2, 1);   // 
  j += ",\"t_amb2\":";      j += jsonNumOrNull(g_tempAmb2, 1);
  j += ",\"mlx2_ok\":";     j += (mlx2OK ? "true" : "false");
  j += ",\"mlx1_valid\":"; j += g_mlxSamples[0].usable(millis())?"true":"false";
  j += ",\"mlx2_valid\":"; j += g_mlxSamples[1].usable(millis())?"true":"false";
  j += ",\"mlx1_age_ms\":"; j += g_mlxSamples[0].present?String(uint32_t(millis()-g_mlxSamples[0].at)):"null";
  j += ",\"mlx2_age_ms\":"; j += g_mlxSamples[1].present?String(uint32_t(millis()-g_mlxSamples[1].at)):"null";
  j += ",\"mlx_active\":";   j += String(g_useMlx2AsFailover ? 2 : 1);
  j += ",\"mlx_failover\":"; j += (g_useMlx2AsFailover ? "true" : "false");
  j += ",\"rain\":";        j += (g_rain ? "true" : "false");
  j += "}";

  j += ",\"heaters\":{";
  j += "\"firmware_enabled\":"; j += (HEATER_PWM_ENABLED ? "true" : "false");
  j += ",\"module_online\":"; j += (g_heaterPwmOnline ? "true" : "false");
  j += ",\"sensor_valid\":"; j += (g_heaterSensorValid ? "true" : "false");
  j += ",\"t_air_c\":"; j += jsonNumOrNull(g_tAir, 1);
  j += ",\"humidity_pct\":"; j += jsonNumOrNull(g_hAir, 0);
  j += ",\"channels\":[";
  for (uint8_t i = 0; i < 3; ++i) {
    if (i) j += ",";
    j += "{\"enabled\":"; j += (g_heaterChannelEnabled[i] ? "true" : "false");
    j += ",\"duty_pct\":"; j += String(g_heaterDutyPct[i], 1);
    j += ",\"rated_w\":"; j += String(g_heaterRatedPowerW[i], 1);
    j += ",\"estimated_w\":"; j += String(g_heaterRatedPowerW[i] * g_heaterDutyPct[i] / 100.0f, 1);
    j += ",\"manual\":"; j += (g_heaterManual[i] ? "true" : "false");
    j += ",\"manual_pct\":"; j += String(g_heaterManualPct[i], 0);
    j += "}";
  }
  j += "]";
  j += ",\"manual_timeout_min\":"; j += String((int)HEATER_MANUAL_TIMEOUT_MIN);
  j += "}";
  j += ",\"led\":{";
  j += "\"firmware_enabled\":"; j += (LED_PWM_ENABLED ? "true" : "false");
  j += ",\"module_online\":"; j += (g_heaterPwmOnline ? "true" : "false");
  j += ",\"on\":"; j += (g_ledPwmOn ? "true" : "false");
  j += ",\"brightness_pct\":"; j += String(g_ledBrightPct, 0);
  j += ",\"duty_pct\":"; j += String(g_ledDutyPct, 1);
  j += "}";

  j += ",\"weather_policy\":{\"version\":1";
  j += ",\"rain\":";j += g_weatherPolicy.rain?"true":"false";
  j += ",\"wind\":";j += g_weatherPolicy.wind?"true":"false";
  j += ",\"clouds\":";j += g_weatherPolicy.clouds?"true":"false";
  j += ",\"humidity\":";j += g_weatherPolicy.humidity?"true":"false";
  j += ",\"dew\":";j += g_weatherPolicy.dew?"true":"false";
  j += ",\"humidity_limit\":";j += String(g_weatherPolicy.humidityLimit);
  j += ",\"dew_tenths\":";j += String(g_weatherPolicy.dewTenths);
  j += ",\"moisture_hold_sec\":";j += String(g_weatherPolicy.moistureHoldSec);
  j += ",\"cloud_hold_sec\":";j += String(g_weatherPolicy.cloudHoldSec);
  j += ",\"reopen_sec\":";j += String(g_weatherPolicy.reopenSec);
  j += ",\"open_ready\":";j += g_weatherOpenReady?"true":"false";
  j += ",\"config_fault\":";j += g_weatherPolicyFault?"true":"false";
  j += ",\"thermo_valid\":";j += weatherThermoReady()?"true":"false";
  j += "}";

  // Progi (dla UI - by suwaki/etykiety byly spojne z firmware)
  j += ",\"limits\":{";
  j += "\"wind_warn\":";      j += String(WIND_WARN_MS, 0);
  j += ",\"wind_sustained\":"; j += String(WIND_SUSTAINED_LIMIT_MS, 0);
  j += ",\"wind_gust\":";     j += String(WIND_GUST_LIMIT_MS, 0);
  j += ",\"wind_reset\":";    j += String(WIND_SAFE_RESET_MS, 0);
  j += ",\"cloud_clear\":";   j += String(CLOUD_DELTA_CLEAR, 0);
  j += ",\"cloud_scattered\":"; j += String(CLOUD_DELTA_SCATTERED, 0);
  j += ",\"cloud_overcast\":"; j += String(CLOUD_DELTA_OVERCAST, 0);
  j += ",\"cloud_trigger\":"; j += (g_cloudCanTriggerUnsafe ? "true" : "false");
  j += ",\"heater_firmware_enabled\":"; j += (HEATER_PWM_ENABLED ? "true" : "false");
  j += ",\"heater_enabled\":[";
  for (uint8_t i = 0; i < 3; ++i) { if (i) j += ","; j += (g_heaterChannelEnabled[i] ? "true" : "false"); }
  j += "],\"heater_watts\":[";
  for (uint8_t i = 0; i < 3; ++i) { if (i) j += ","; j += String(g_heaterRatedPowerW[i], 1); }
  j += "]";
  j += ",\"anemometer\":"; j += (g_anemometerEnabled ? "true" : "false");
  // ═══ kalibracja MLX + tryb sieci ═══
  j += ",\"sky_corr\":";  j += (g_skyCorrEnabled ? "true" : "false");
  j += ",\"sky_k1\":"; j += String(g_skyK1, 0);
  j += ",\"sky_k2\":"; j += String(g_skyK2, 0);
  j += ",\"sky_k3\":"; j += String(g_skyK3, 0);
  j += ",\"sky_k4\":"; j += String(g_skyK4, 0);
  j += ",\"sky_k5\":"; j += String(g_skyK5, 0);
  j += ",\"mlx_sky_off\":"; j += String(g_mlxSkyOff, 1);
  j += ",\"mlx_amb_off\":"; j += String(g_mlxAmbOff, 1);
  j += ",\"net_mode\":"; j += String((int)g_netMode);
  j += ",\"auto_cloud\":"; j += (g_autoCloud ? "true" : "false");   // 
  j += "}";

  // Diagnostyka czujnikow/systemu (dla panelu diagnostycznego w UI)
  j += ",\"diag\":{";
  j += "\"uptime_s\":";   j += String((uint32_t)(millis() / 1000UL));
#if USE_W5500
  j += ",\"ethernet_timeouts\":"; j += String(astroEthTimeoutCount());
#endif
  j += ",\"tca_addr\":";  j += String((int)g_tcaAddr);   // 
  j += ",\"modbus_addr\":"; j += String((int)MODBUS_RELAY_ADDR);
  j += ",\"baro\":";      j += jsonQuote(g_baroType == BARO_BMP280 ? "BMP280" : (g_baroType == BARO_BME280 ? "BME280" : "BRAK"));
  j += ",\"mlx_ok\":";    j += (mlxOK ? "true" : "false");
  j += ",\"mlx2_ok\":";   j += (mlx2OK ? "true" : "false");   // 
#if ENABLE_DHT22
  j += ",\"dht_ok\":";    j += (dhtOK ? "true" : "false");
#endif
#if ENABLE_DS18B20
  j += ",\"ds_ok\":";     j += (dsOK ? "true" : "false");
#endif
#if ENABLE_BH1750
  j += ",\"bh_ok\":";     j += (bhOK ? "true" : "false");
#endif
#if ENABLE_SI1145
  j += ",\"uv_ok\":";     j += (uvOK ? "true" : "false");
#endif
#if ENABLE_TSL2591
  j += ",\"tsl_ok\":";    j += (tslOK ? "true" : "false");
  j += ",\"tsl_gain\":";  j += String((int)g_tslGainIdx);
#endif
  j += ",\"baro_ok\":";   j += ((bmpOK || bme280OK) ? "true" : "false");
  j += ",\"collision_en\":";
#if COLLISION_SENSOR_ENABLED
  j += "true";
#else
  j += "false";
#endif
  j += ",\"collision_active\":"; j += (g_collisionSensorActive ? "true" : "false");
  j += ",\"th_src\":";   j += jsonQuote((g_baroType == BARO_BME280 && bme280OK) ? "BME280" : "BRAK");
  j += ",\"wind_fault\":"; j += (g_windFault ? "true" : "false");
  j += ",\"wind_volts\":"; j += jsonNumOrNull(g_windRawVolts, 2);
  j += ",\"wind_avg\":";   j += jsonNumOrNull(windMeasurementUsable()?g_windSustained:NAN, 1);
  j += ",\"wind_gust\":";  j += jsonNumOrNull(windMeasurementUsable()?g_windGust:NAN, 1);
  j += ",\"wind_latch\":"; j += (g_windUnsafeLatched ? "true" : "false");
  j += ",\"cloud_latch\":"; j += (g_cloudUnsafeLatched ? "true" : "false");
#if USE_TELEGRAM
  j += ",\"tg_q\":"; j += String((int)g_tgQCount);   // ile wiadomosci czeka w kolejce
#else
  j += ",\"tg_q\":0";
#endif
  j += ",\"motion_fault\":"; j += jsonQuote(g_roofMotionFault);
  j += ",\"fault_class\":"; j += jsonQuote(g_roofFaultKind==RoofFaultKind::COMMUNICATION?"COMMUNICATION":g_roofFaultKind==RoofFaultKind::PARK_LOST?"PARK_LOST":g_roofFaultKind==RoofFaultKind::PHYSICAL?"PHYSICAL":g_roofFaultKind==RoofFaultKind::STOP_UNCONFIRMED?"STOP_UNCONFIRMED":"NONE");
  j += ",\"anem_en\":";     j += (g_anemometerEnabled ? "true" : "false");
  j += "}";

  j += ",\"relay_names\":[";
  for (uint8_t ch=0; ch<8; ++ch) { if(ch) j+=","; j+=jsonQuote(astroRelayName(ch)); }
  j += "],\"aux_map\":[";
  bool firstAux=true;
  for(uint8_t id=1;id<=5;++id) if(astroAuxAvailable(id)) {
    if(!firstAux) j+=","; firstAux=false;
    j+="{\"id\":"+String(id)+",\"ch\":"+String(astroAuxChannel(id)+1)+"}";
  }
  j += "]";
  j += ",\"rg11_enabled\":"; j += (ENABLE_RG11 ? "true":"false");
  j += ",\"roof_motion_enabled\":"; j += (ROOF_MOTION_ENABLED ? "true":"false");
  j += ",\"relay_backend\":"; j += jsonQuote(OBS_BOARD_8RO ? "TCA9554 built-in relays" : "Waveshare Modbus V3");
  j += ",\"mqtt_ok\":";
#if USE_MQTT
  j += (g_mqttConnected ? "true" : "false");
#else
  j += "false";
#endif
  j += ",\"aux\":[";
  for (int i = 0; i < 5; i++) {
    if (i) j += ",";
    const uint8_t ch = astroAuxChannel(i + 1);
    j += (relayDeviceReady(ch) ? relayState[ch] : false) ? "true" : "false";
  }
  j += "]";

  j += ",\"illum\":"; j += (g_illumOutputOn ? "true" : "false");

  String dbg;
  dbg += "MLX=" + String(mlxOK ? "OK" : "BRAK");

  dbg += " BARO=";
  if (g_baroType == BARO_BMP280) dbg += "BMP280";
  else if (g_baroType == BARO_BME280) dbg += "BME280";
  else dbg += "NONE";
  j += ",\"debug\":"; j += jsonQuote(dbg);
  j += "}";
  return j;
}

// ============== buildHistoryJson ==============
static String buildHistoryJson() {
  String j; j.reserve(512 + g_historyCount * 20);
  j += "{\"wind\":[";
  for (int i = 0; i < g_historyCount; i++) {
    int idx = (g_historyHead - g_historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i) j += ",";
    j += String(g_history[idx].wind_ms, 1);
  }
  j += "],\"delta\":[";
  for (int i = 0; i < g_historyCount; i++) {
    int idx = (g_historyHead - g_historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i) j += ",";
    j += isnan(g_history[idx].cloud_delta) ? "null" : String(g_history[idx].cloud_delta, 1);
  }
  j += "],\"t_air\":[";
  for (int i = 0; i < g_historyCount; i++) {
    int idx = (g_historyHead - g_historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i) j += ",";
    j += isnan(g_history[idx].t_air) ? "null" : String(g_history[idx].t_air, 1);
  }
  j += "],\"rain\":[";
  for (int i = 0; i < g_historyCount; i++) {
    int idx = (g_historyHead - g_historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i) j += ",";
    j += g_history[idx].rain ? "1" : "0";
  }
  j += "],\"count\":"; j += g_historyCount;
  j += "}";
  return j;
}

// ============== writeAll ==============
static void writeAll(Client &c, const char* header, size_t headerLen, const char* body, size_t bodyLen) {
  const size_t CHUNK = 1024;
  const size_t len = headerLen + bodyLen;
  size_t pos = 0;
  uint8_t tmp[CHUNK];
  const uint32_t start = millis();
  uint32_t lastProgress = start;
  while (pos < len) {
    if (!c.connected()) break;
    size_t n = (len - pos > CHUNK) ? CHUNK : (len - pos);
    const size_t prefix = pos < headerLen ? min(n, headerLen - pos) : 0;
    if (prefix) memcpy(tmp, header + pos, prefix);
    if (prefix < n) memcpy(tmp + prefix, body + (pos + prefix - headerLen), n - prefix);
    size_t w = c.write(tmp, n);
    if (w > 0) { pos += w; lastProgress = millis(); } else delay(1);
    serviceRoofFast();
    httpUrgentTick(); // STOP/SafetyMonitor can use the other session during a response
    yield();
    // Bound a stalled peer separately from the complete 138 kB page.
    if (uint32_t(millis() - lastProgress) > 1000 ||
        uint32_t(millis() - start) > 10000) { c.stop(); break; }
  }
}

static void httpSendRaw(Client &c, int code, const char* ct, const char* ct2, const char* body, size_t bodyLen) {
  char header[256];
  const int headerLen = snprintf(header, sizeof(header),
    "HTTP/1.1 %d %s\r\nConnection: close\r\nCache-Control: no-store, max-age=0\r\n"
    "X-Content-Type-Options: nosniff\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n",
    code, ct, ct2, (unsigned)bodyLen);
  if (headerLen < 0 || (size_t)headerLen >= sizeof(header)) { c.stop(); return; }

  // Small Alpaca replies use one TCP send instead of a separate send per header field.
  writeAll(c, header, headerLen, body, bodyLen);
  c.flush(); delay(2);
}

static void httpSend(Client &c, int code, const char* ct, const String& ct2, const String& body) {
  httpSendRaw(c, code, ct, ct2.c_str(), body.c_str(), body.length());
}

static void alpacaSend(Client &c, uint32_t cid, int errNum, const String& errMsg, const String& valJson) {
  String body = "{";
  if (valJson.length()) body += "\"Value\":" + valJson + ",";
  body += "\"ClientTransactionID\":" + String(cid);
  body += ",\"ServerTransactionID\":" + String(serverTransactionID++);
  body += ",\"ErrorNumber\":" + String(errNum);
  body += ",\"ErrorMessage\":" + jsonQuote(errMsg) + "}";
  httpSend(c, 200, "OK", "application/json", body);
}
static void alpacaSendValue(Client &c, uint32_t cid, const String& v) { alpacaSend(c, cid, 0, "", v); }
static void alpacaSendError(Client &c, uint32_t cid, int n, const String& m) { alpacaSend(c, cid, n, m, "null"); }

static void alpacaSendMeasurement(Client &c, uint32_t cid, float value, int decimals) {
  if (isfinite(value)) alpacaSendValue(c, cid, jsonNumOrNull(value, decimals));
  else alpacaSend(c, cid, 0x500, "Sensor has no valid measurement", "");
}

static String stripAbsoluteUrlToPath(const String& target) {
  int scheme = target.indexOf("://");
  if (scheme < 0) return target;
  int pathPos = target.indexOf('/', scheme + 3);
  if (pathPos < 0) return "/";
  return target.substring(pathPos);
}

static float timeSinceMs(unsigned long lastMs) {
  if (lastMs == 0) return -1.0f;
  unsigned long now = millis();
  if (now < lastMs) return 0.0f;
  return (now - lastMs) / 1000.0f;
}

// ============== Basic Auth ==============
static String base64Decode(const String& in) {
  static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out; out.reserve((in.length() * 3) / 4 + 4);
  int val = 0, valb = -8;
  for (int i = 0; i < (int)in.length(); i++) {
    const char* p = strchr(b64, in[i]);
    if (!p) break;
    val = (val << 6) + (int)(p - b64);
    valb += 6;
    if (valb >= 0) { out += (char)((val >> valb) & 0xFF); valb -= 8; }
  }
  return out;
}

static bool checkAuth(Client& c, const String& authHeader, const String& queryParams = "") {
  // Fail closed: brak hasla w secrets.ini ma blokowac WWW, nie otwierac je dla sieci.
  if (strlen(AUTH_PASS) == 0) {
    String body = "Panel WWW zablokowany: ustaw UI_PASS w secrets.ini.";
    c.print(F("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: "));
    c.print(body.length()); c.print(F("\r\n\r\n")); c.print(body); c.flush();
    return false;
  }
  // Tryb 1: HTTP Basic Auth (desktop)
  if (authHeader.length() > 0) {
    String ah = authHeader; ah.trim();
    if (ah.startsWith("Basic ") || ah.startsWith("basic ")) {
      String decoded = base64Decode(ah.substring(6));
      decoded.trim();
      if (decoded == (String(AUTH_USER) + ":" + String(AUTH_PASS))) return true;
    }
  }
  // Passwords in query strings are no longer accepted.
  String body = "<html><body><h2>401 Unauthorized</h2></body></html>";
  c.print(F("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Obserwatorium\"\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: "));
  c.print(body.length()); c.print(F("\r\n\r\n")); c.print(body); c.flush();
  return false;
}

// ============================================================================
// ASTRO-ROOF SAFE RC5.2 TEMP3 - EVENT LOG
// ---------------------------------------------------------------------------
// "Czarna skrzynka" obserwatorium:
//   - RAM ring buffer: ostatnie 200 zdarzen
//   - LittleFS: trwaly zapis zdarzen, przetrwa restart ESP32
//   - poziom: INFO / WARN / ERROR / CRITICAL
//   - kategoria: BOOT / ROOF / SAFETY / NINA / MLX / RAIN / WIND / PARK / ...
//   - WWW: filtrowanie, auto-refresh, licznik zdarzen, eksport JSON
//
// Logger NIE steruje bezpieczenstwem. Awaria LittleFS nie blokuje dachu.
// Zapis do Flash jest wykonywany tylko przy zdarzeniach, nigdy cyklicznie.
// ============================================================================
#ifndef EVENT_LOG_SIZE
  #define EVENT_LOG_SIZE 200
#endif
#ifndef EVENT_LOG_FILE
  #define EVENT_LOG_FILE "/astro_event.log"
#endif
#ifndef EVENT_LOG_MAX_BYTES
  #define EVENT_LOG_MAX_BYTES 98304UL
#endif
#ifndef EVENT_LOG_LOAD_BYTES
  #define EVENT_LOG_LOAD_BYTES 65536UL
#endif
#ifndef EVENT_LOG_PERSISTENT
  #define EVENT_LOG_PERSISTENT 1
#endif

struct EventLogEntry {
  uint32_t ms;
  uint32_t epoch;
  char level[10];
  char category[16];
  char text[160 + sizeof(OBSERVATORY_PREFIX)];
};

static EventLogEntry g_eventLog[EVENT_LOG_SIZE];
static uint16_t g_eventLogHead = 0;
static uint16_t g_eventLogCount = 0;
static bool g_eventLogFsOK = false;
static bool g_eventLogLoading = false;

static const char* eventLevelName(const char* level) {
  return level ? level : "INFO";
}

static void eventLogSanitize(char* s) {
  if (!s) return;
  for (char* p = s; *p; ++p) {
    if (*p == '\n' || *p == '\r' || *p == '\t' || *p == '|') *p = ' ';
  }
}

static void eventLogPush(const char* level, const char* category, const char* text) {
  EventLogEntry &e = g_eventLog[g_eventLogHead];
  e.ms = millis();
  time_t nowEpoch = time(nullptr);
  e.epoch = (nowEpoch > 1700000000) ? (uint32_t)nowEpoch : 0;
  strncpy(e.level, eventLevelName(level), sizeof(e.level)-1); e.level[sizeof(e.level)-1] = 0;
  strncpy(e.category, category ? category : "SYSTEM", sizeof(e.category)-1); e.category[sizeof(e.category)-1] = 0;
  observatoryCopyMessage(e.text, sizeof(e.text), text);
  eventLogSanitize(e.level);
  eventLogSanitize(e.category);
  eventLogSanitize(e.text);
  g_eventLogHead = (g_eventLogHead + 1) % EVENT_LOG_SIZE;
  if (g_eventLogCount < EVENT_LOG_SIZE) g_eventLogCount++;
}

static void eventLogPersist(const EventLogEntry& e) {
#if EVENT_LOG_PERSISTENT
  if (!g_eventLogFsOK || g_eventLogLoading) return;
  File f = LittleFS.open(EVENT_LOG_FILE, FILE_APPEND);
  if (!f) return;
  // Prosty, odporny format: ms|epoch|level|category|text\n
  f.print(e.ms); f.print('|');
  f.print(e.epoch); f.print('|');
  f.print(e.level); f.print('|');
  f.print(e.category); f.print('|');
  f.print(e.text); f.print('\n');
  f.close();

  File chk = LittleFS.open(EVENT_LOG_FILE, FILE_READ);
  if (chk) {
    size_t sz = chk.size();
    chk.close();
    if (sz > EVENT_LOG_MAX_BYTES) {
      // Zachowujemy jeden poprzedni plik jako dodatkowa historia.
      if (LittleFS.exists("/astro_event.old")) LittleFS.remove("/astro_event.old");
      LittleFS.rename(EVENT_LOG_FILE, "/astro_event.old");
    }
  }
#endif
}

static void eventLogV(const char* level, const char* category, const char* fmt, va_list ap) {
  char msg[160];
  vsnprintf(msg, sizeof(msg), fmt, ap);
  msg[sizeof(msg)-1] = 0;
  eventLogPush(level, category, msg);
  EventLogEntry &e = g_eventLog[(g_eventLogHead + EVENT_LOG_SIZE - 1) % EVENT_LOG_SIZE];
  Serial.printf("[EVENT][%s][%s] %s\n", e.level, e.category, e.text);
  eventLogPersist(e);
}

static void eventLogEx(const char* level, const char* category, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  eventLogV(level, category, fmt, ap);
  va_end(ap);
}

// Zachowanie starego API eventLog() - domyslnie INFO/SYSTEM.
static void eventLog(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  eventLogV("INFO", "SYSTEM", fmt, ap);
  va_end(ap);
}

static void eventLogLoadFile(const char* path, size_t maxBytes) {
#if EVENT_LOG_PERSISTENT
  if (!g_eventLogFsOK) return;
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return;
  size_t sz = f.size();
  if (sz > maxBytes) f.seek(sz - maxBytes, SeekSet);
  // Jeżeli zaczęliśmy w środku linii, pomiń jej resztę.
  if (sz > maxBytes) f.readStringUntil('\n');

  g_eventLogLoading = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    int p1=line.indexOf('|');
    int p2=(p1>=0)?line.indexOf('|',p1+1):-1;
    int p3=(p2>=0)?line.indexOf('|',p2+1):-1;
    int p4=(p3>=0)?line.indexOf('|',p3+1):-1;
    if (p1<0 || p2<0 || p3<0 || p4<0) continue;
    uint32_t ms=(uint32_t)line.substring(0,p1).toInt();
    uint32_t epoch=(uint32_t)line.substring(p1+1,p2).toInt();
    String level=line.substring(p2+1,p3);
    String cat=line.substring(p3+1,p4);
    String msg=line.substring(p4+1);
    eventLogPush(level.c_str(),cat.c_str(),msg.c_str());
    EventLogEntry &loaded = g_eventLog[(g_eventLogHead + EVENT_LOG_SIZE - 1) % EVENT_LOG_SIZE];
    loaded.ms = ms;
    loaded.epoch = epoch;
  }
  g_eventLogLoading = false;
  f.close();
#endif
}

static void eventLogInit() {
#if EVENT_LOG_PERSISTENT
  g_eventLogFsOK = LittleFS.begin(false);
  if (!g_eventLogFsOK) {
    Serial.println(F("[EVENT][WARN][LOG] LittleFS niedostepny - log pozostaje tylko w RAM"));
    return;
  }
  eventLogLoadFile("/astro_event.old", EVENT_LOG_LOAD_BYTES);
  eventLogLoadFile(EVENT_LOG_FILE, EVENT_LOG_LOAD_BYTES);
  Serial.println(F("[EVENT][INFO][LOG] LittleFS EVENT LOG aktywny"));
#else
  g_eventLogFsOK = false;
#endif
}

static String eventLogJsonEscape(const char* s) {
  String out;
  if (!s) return out;
  for (const char* p=s; *p; ++p) {
    if (*p == '\\' || *p == '"') { out += '\\'; out += *p; }
    else if (*p == '\n' || *p == '\r') out += ' ';
    else out += *p;
  }
  return out;
}

static String buildEventLogJson() {
  String out;
  out.reserve(42000);
  out += "[";
  uint16_t start = (g_eventLogCount < EVENT_LOG_SIZE) ? 0 : g_eventLogHead;
  for (uint16_t i=0; i<g_eventLogCount; ++i) {
    uint16_t idx=(start+i)%EVENT_LOG_SIZE;
    if(i) out += ',';
    out += "{\"ms\":"; out += String(g_eventLog[idx].ms);
    out += ",\"epoch\":"; out += String(g_eventLog[idx].epoch);
    out += ",\"level\":\""; out += eventLogJsonEscape(g_eventLog[idx].level); out += "\"";
    out += ",\"category\":\""; out += eventLogJsonEscape(g_eventLog[idx].category); out += "\"";
    out += ",\"event\":\""; out += eventLogJsonEscape(g_eventLog[idx].text); out += "\"}";
  }
  out += "]";
  return out;
}

static String eventLogDisplayTime(const EventLogEntry& e) {
  if (e.epoch > 1700000000) {
    struct tm tmv;
    time_t t=(time_t)e.epoch;
    localtime_r(&t,&tmv);
    char b[24];
    strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",&tmv);
    return String(b);
  }
  unsigned long sec=e.ms/1000UL;
  unsigned long h=sec/3600UL; sec%=3600UL;
  unsigned long m=sec/60UL; sec%=60UL;
  char b[24]; snprintf(b,sizeof(b),"+%02lu:%02lu:%02lu",h,m,sec);
  return String(b);
}

static String buildEventLogHtml() {
  String out; out.reserve(36000);
  out += F("<!doctype html><html lang='pl'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>" OBSERVATORY_NAME " - ASTRO-ROOF SAFE - PRO EVENT LOG</title><style>"
            "body{margin:0;background:#0b0e12;color:#dfe7ef;font-family:Arial,sans-serif}"
            ".top{padding:18px 22px;background:#11161d;border-bottom:1px solid #27313c;position:sticky;top:0;z-index:2}"
            "h1{margin:0 0 8px;font-size:22px}.sub{color:#8795a5;font-size:12px}"
            ".bar{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}.btn,.sel{background:#18212b;color:#dfe7ef;border:1px solid #344352;border-radius:7px;padding:8px 11px;cursor:pointer}"
            ".stats{display:flex;gap:16px;flex-wrap:wrap;margin-top:12px;font-size:12px;color:#aebbc8}.stat b{color:#fff}"
            ".wrap{padding:12px 22px}.row{display:grid;grid-template-columns:155px 80px 100px 1fr;gap:10px;padding:8px 10px;border-bottom:1px solid #18212a;font:13px Consolas,monospace}"
            ".head{color:#8190a0;background:#10151b;position:sticky;top:119px}.INFO{color:#cbd5df}.WARN{color:#ffd166;background:#2b2411}.ERROR{color:#ff9b8d;background:#2c1513}.CRITICAL{color:#fff;background:#4a1111;font-weight:700}"
            ".lvl{font-weight:700}.empty{padding:30px;color:#7d8996}.foot{padding:16px 22px;color:#657381;font-size:11px}"
            "@media(max-width:800px){.row{grid-template-columns:120px 65px 80px 1fr;font-size:11px}.head{top:145px}}"
            "</style></head><body><div class='top'><h1>" OBSERVATORY_NAME " — ASTRO-ROOF SAFE · LOG</h1>"
            "<div class='sub'>Czarna skrzynka obserwatorium • RAM + LittleFS • log nie steruje bezpieczenstwem</div>"
            "<div class='bar'><button class='btn' onclick='loadLog()'>↻ Odśwież</button><button class='btn' onclick='toggleAuto()' id='auto'>⏸ Auto: ON</button><button class='btn' onclick=\"location.href='/api/log'\">JSON</button>"
            "<select class='sel' id='filter' onchange='render()'><option value='ALL'>Wszystkie</option><option>INFO</option><option>WARN</option><option>ERROR</option><option>CRITICAL</option></select>"
            "<select class='sel' id='cat' onchange='render()'><option value='ALL'>Wszystkie kategorie</option></select></div>"
            "<div class='stats'><span class='stat'>RAM wpisów: <b id='count'>0</b></span><span class='stat'>Ostatni: <b id='last'>—</b></span><span class='stat'>FS: <b id='fs'>LittleFS</b></span></div></div>"
            "<div class='wrap'><div class='row head'><div>CZAS</div><div>POZIOM</div><div>KATEGORIA</div><div>ZDARZENIE</div></div><div id='log'><div class='empty'>Ładowanie...</div></div></div>"
            "<div class='foot'>" OBSERVATORY_NAME " — ASTRO-ROOF SAFE · LOG</div>"
            "<script>let data=[],auto=true,timer=null;const $=id=>document.getElementById(id);"
            "async function loadLog(){try{const r=await fetch('/api/log?ts='+Date.now(),{cache:'no-store'});if(!r.ok)throw Error(r.status);data=await r.json();buildCats();render();}catch(e){$('log').innerHTML='<div class=empty>Brak odpowiedzi /api/log: '+e+'</div>';}}"
            "function buildCats(){let s=$('cat'),old=s.value,cs=[...new Set(data.map(x=>x.category))].sort();s.innerHTML='<option value=\"ALL\">Wszystkie kategorie</option>'+cs.map(c=>'<option>'+esc(c)+'</option>').join('');if(cs.includes(old))s.value=old;}"
            "function render(){let f=$('filter').value,c=$('cat').value,a=data.filter(x=>(f==='ALL'||x.level===f)&&(c==='ALL'||x.category===c));$('count').textContent=data.length; $('last').textContent=data.length?fmt(data[data.length-1]):'—';$('log').innerHTML=a.length?a.map(x=>'<div class=\"row '+x.level+'\"><div>'+esc(fmt(x))+'</div><div class=\"lvl\">'+esc(x.level)+'</div><div>'+esc(x.category)+'</div><div>'+esc(x.event)+'</div></div>').join(''):'<div class=empty>Brak zdarzeń dla wybranego filtra.</div>';window.scrollTo(0,document.body.scrollHeight);}"
            "function fmt(x){if(x.epoch&&x.epoch>1700000000){let d=new Date(x.epoch*1000);return d.toLocaleString();}let s=Math.floor(x.ms/1000),h=Math.floor(s/3600);s%=3600;let m=Math.floor(s/60);s%=60;return '+'+String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');}"
            "function esc(s){return String(s).replace(/[&<>\"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[m]));}"
            "function toggleAuto(){auto=!auto;$('auto').textContent=auto?'⏸ Auto: ON':'▶ Auto: OFF';if(timer)clearInterval(timer);if(auto)timer=setInterval(loadLog,3000);}"
            "loadLog();timer=setInterval(loadLog,3000);</script></body></html>");
  return out;
}


// ============== HTTP handler ==============
static String webCommandToken() {
  static const String token=String(esp_random(),HEX)+String(esp_random(),HEX)+String(esp_random(),HEX)+String(esp_random(),HEX);
  return token;
}
static void handleClientImpl(Client &c,const astro_http::Request& request,const IPAddress& peer) {
  const String reqLine(request.request);
  const String authHeader(request.auth);
  int sp1=reqLine.indexOf(' '), sp2=sp1>=0 ? reqLine.indexOf(' ',sp1+1) : -1;
  if(sp1<0 || sp2<0) { httpSend(c,400,"Bad Request","text/plain","Bad Request"); c.stop(); return; }
  const String method=reqLine.substring(0,sp1);
  const String target=stripAbsoluteUrlToPath(reqLine.substring(sp1+1,sp2));
  String path=target, params;
  const int qpos=target.indexOf('?');
  if(qpos>=0) { path=target.substring(0,qpos); params=target.substring(qpos+1); }
  if(request.bodyLength) { if(params.length()) params+='&'; params+=request.body; }

  const uint32_t cid = getParamUInt(params, "ClientTransactionID", 0);
  if (method == "OPTIONS") { httpSend(c,403,"Forbidden","text/plain","Cross-origin control disabled");c.stop();return; }

  String pl = toLowerCopy(path);

  if(request.origin[0]) {
    const String origin(request.origin),host(request.host);
    if(!host.length() || (origin!="http://"+host && origin!="https://"+host)) {
      httpSend(c,403,"Forbidden","text/plain","Foreign browser origin rejected");c.stop();return;
    }
  }
  if(pl.startsWith("/api/v1/")) {
    // Browser traffic never gets the trusted-host exception. All other clients
    // need either the exact configured IPv4 host or valid HTTP Basic credentials.
    if(request.origin[0] || (!astro_policy::trustedIPv4(peer.toString().c_str(),ALPACA_TRUSTED_IPS) && !checkAuth(c,authHeader))) {
      if(request.origin[0])httpSend(c,403,"Forbidden","text/plain","Alpaca control is not a browser interface");
      c.stop();return;
    }
  }
  diagnosticStage(30,pl.startsWith("/api/v1/dome/")?1:pl.startsWith("/api/v1/safetymonitor/")?2:pl=="/cmd"?3:4);
  const bool webMutation=pl=="/cmd" || pl=="/relay" || pl=="/relaytest" || pl=="/api/wind/calibration" || pl=="/api/weather/policy" || (pl=="/api/notifications" && method!="GET");
  if (webMutation) {
    if(method!="POST" || qpos>=0 || String(request.csrf)!=webCommandToken()) {
      httpSend(c,403,"Forbidden","application/json","{\"ok\":false,\"error\":\"POST body and command token required\"}"); c.stop(); return;
    }
    if(!checkAuth(c,authHeader)) { c.stop(); return; }
  }
  if(method=="GET" && pl=="/api/command-token") {
    if(!checkAuth(c,authHeader)) { c.stop(); return; }
    httpSend(c,200,"OK","application/json",String("{\"token\":\"")+webCommandToken()+"\"}"); c.stop(); return;
  }
  // ── Web UI ────────────────────────────────────────────────────────────
  if (method == "GET" && (pl == "/" || pl == "/index.html")) {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    httpSendRaw(c, 200, "OK", "text/html; charset=utf-8", INDEX_HTML, INDEX_HTML_LEN);
    c.stop(); return;
  }

  if(method=="POST" && pl=="/api/weather/policy") {
    if(!checkAuth(c,authHeader,params)){c.stop();return;}
    if(roofState!=RoofState::CLOSED || !limitClosedActive() || limitOpenActive() || !g_manualMode) {
      httpSend(c,409,"Conflict","application/json","{\"ok\":false,\"error\":\"Zmiana polityki wymaga zamknietego dachu i MANUAL\"}");c.stop();return;
    }
    astro_weather::Policy candidate;bool ok=true;
    auto integer=[&](const char* key,long lo,long hi)->long {
      String text=getParamValue(params,key);char* end=nullptr;long value=strtol(text.c_str(),&end,10);
      if(!text.length() || end==text.c_str() || *end || value<lo || value>hi)ok=false;
      return value;
    };
    candidate.rain=integer("rain",0,1);candidate.wind=integer("wind",0,1);candidate.clouds=integer("clouds",0,1);
    candidate.humidity=integer("humidity",0,1);candidate.dew=integer("dew",0,1);
    candidate.humidityLimit=integer("humidity_limit",50,100);candidate.dewTenths=integer("dew_tenths",0,100);
    candidate.moistureHoldSec=integer("moisture_hold_sec",10,1800);candidate.cloudHoldSec=integer("cloud_hold_sec",30,1800);
    candidate.reopenSec=integer("reopen_sec",60,3600);
    if(!ok || !candidate.valid())httpSend(c,400,"Bad Request","application/json","{\"ok\":false,\"error\":\"Brak lub niepoprawne parametry polityki\"}");
    else {
      const bool saved=weatherPolicyStore(candidate);
      eventLogEx(saved?"WARN":"ERROR","POLICY","Save %s mask=%u RH=%u dew=%u cloud=%us moisture=%us reopen=%us",saved?"OK":"FAILED",candidate.mask(),candidate.humidityLimit,candidate.dewTenths,candidate.cloudHoldSec,candidate.moistureHoldSec,candidate.reopenSec);
      // A successful policy change never sends roof or mount commands.
      g_cloudOvercastSinceMs=0;g_cloudUnsafeLatched=false;g_moisture=astro_weather::Moisture{};
      g_unsafe=true;g_reason=saved?"POLITYKA: oczekiwanie na nowy odczyt":"BLAD ZAPISU POLITYKI POGODY";
      httpSend(c,saved?200:500,saved?"OK":"Internal Server Error","application/json",saved?"{\"ok\":true}":"{\"ok\":false,\"error\":\"Blad zapisu NVS; ochrona zablokowana\"}");
    }
    c.stop();return;
  }

  if(method=="POST" && pl=="/api/wind/calibration") {
    if(!checkAuth(c,authHeader,params)) { c.stop(); return; }
    if(roofState!=RoofState::CLOSED || !g_manualMode) {
      httpSend(c,409,"Conflict","application/json","{\"ok\":false,\"error\":\"Zamknij dach i wlacz MANUAL przed kalibracja\"}"); c.stop(); return;
    }
    WindCalibration candidate;
    bool ok=true;
    auto number=[&](const char* key,float& value) {
      String text=getParamValue(params,key); char* end=nullptr;
      const float parsed=strtof(text.c_str(),&end);
      if(!text.length() || end==text.c_str() || *end || !isfinite(parsed)) ok=false;
      else value=parsed;
    };
    number("gain",candidate.gain); number("offset",candidate.offset); number("ratio",candidate.ratio);
    number("zero",candidate.zero); number("top",candidate.top); number("maxMS",candidate.maxMS);
    String model=getParamValue(params,"model"); model.trim();
    if(model.length()<3 || model.length()>=sizeof(candidate.model)) ok=false;
    for(size_t i=0;i<model.length();++i) if(!((model[i]>='a'&&model[i]<='z')||
      (model[i]>='A'&&model[i]<='Z')||(model[i]>='0'&&model[i]<='9')||strchr(" ._-",model[i]))) ok=false;
    model.toCharArray(candidate.model,sizeof(candidate.model));
    candidate.verified=(getParamValue(params,"verified")=="1");
    if(!windCalibrationValid(candidate) || (candidate.verified &&
      (!g_windValidity.adcCalibrated || candidate.maxMS<WIND_GUST_LIMIT_MS || candidate.maxMS<WIND_SUSTAINED_LIMIT_MS))) ok=false;
    if(!ok) { httpSend(c,400,"Bad Request","application/json","{\"ok\":false,\"error\":\"Niepoprawne parametry, model lub kalibracja ADC\"}"); }
    else {
      const bool saved=windCalibrationStore(candidate);
      windResetAfterCalibration(); g_unsafe=true; g_reason="ANEMOMETR: ponowna weryfikacja po kalibracji";
      eventLogEx(saved?"INFO":"ERROR","WIND","Calibration save %s; model=%s; verified=%u",saved?"OK":"FAILED",candidate.model,candidate.verified);
      httpSend(c,saved?200:500,saved?"OK":"Internal Server Error","application/json",saved?"{\"ok\":true}":"{\"ok\":false,\"error\":\"Blad zapisu NVS - pomiar niewiarygodny\"}");
    }
    c.stop(); return;
  }

  // ── API state ─────────────────────────────────────────────────────────
  if (method == "GET" && pl == "/api/state") {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    httpSend(c, 200, "OK", "application/json", buildStateJson());
    c.stop(); return;
  }

  if(pl=="/api/notifications") {
    if(!checkAuth(c,authHeader)) { c.stop(); return; }
    if(method=="GET")httpSend(c,200,"OK","application/json",onlineStatusJson());
    else if(method=="POST") {
      const String channelText=getParamValue(params,"channel"), action=getParamValue(params,"action");
      const bool validChannel=channelText=="0"||channelText=="1"||channelText=="2";
      const uint8_t channel=(uint8_t)channelText.toInt();
      const String enabled=getParamValue(params,"enabled");
      const bool ok=validChannel && (action=="test"?onlineTest(channel):
        action=="enable"&&(enabled=="0"||enabled=="1")?onlineSetEnabled(channel,enabled=="1"):false);
      eventLogEx(ok?"INFO":"WARN","NOTIFY","Web action accepted=%u channel=%u",ok,channel);
      httpSend(c,ok?200:409,ok?"OK":"Conflict","application/json",ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"Channel unavailable, not configured, or queue full\"}");
    }else httpSend(c,405,"Method Not Allowed","application/json","{\"ok\":false}");
    c.stop(); return;
  }

  // ── API history ───────────────────────────────────────────────────────
  if (method == "GET" && pl == "/api/history") {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    httpSend(c, 200, "OK", "application/json", buildHistoryJson());
    c.stop(); return;
  }

  // ── PRO EVENT LOG ─────────────────────────────────────────────────────
  if (method == "GET" && pl == "/api/log") {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    httpSend(c, 200, "OK", "application/json", buildEventLogJson());
    c.stop(); return;
  }

  if (method == "GET" && (pl == "/log" || pl == "/eventlog")) {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    httpSend(c, 200, "OK", "text/html; charset=utf-8", buildEventLogHtml());
    c.stop(); return;
  }

  // ── CMD ───────────────────────────────────────────────────────────────
  if (method == "POST" && pl=="/cmd") {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    String doCmd = toLowerCopy(getParamValue(params, "do"));

    bool cmdOk = true;
    bool restartAfterResponse = false;
    if (doCmd == "restart") {
      const bool roofMoving = (roofState == RoofState::OPENING || roofState == RoofState::CLOSING);
      const bool roofRelayActive = relayState[RELAY_ROOF_OPEN] || relayState[RELAY_ROOF_CLOSE] || relayState[RELAY_ROOF_STOP];
      if (roofMoving || roofRelayActive) {
        cmdOk = false;
        g_lastRoofBlockReason = "Restart odrzucony: najpierw zatrzymaj dach.";
      } else {
        roofRelaysOff();
        if(pulseActive[RELAY_ROOF_OPEN] || pulseActive[RELAY_ROOF_CLOSE] || pulseActive[RELAY_ROOF_STOP]) {
          cmdOk=false;
          g_lastRoofBlockReason="Restart odrzucony: OFF przekaznikow dachu niepotwierdzone";
        } else restartAfterResponse = true;
      }
    }
    const bool wantsOpen=(doCmd=="open" || doCmd=="force_open");
    const bool wantsClose=(doCmd=="close" || doCmd=="force_close");
    if (wantsOpen || wantsClose) {
      if (g_manualMode && (!g_manualAuthorized || getParamValue(params,"manual_pass")!=String(AUTH_PASS) || getParamValue(params,"confirm")!="MANUAL_MOVE")) {
        cmdOk=false;
        g_lastRoofBlockReason="MANUAL: wymagana ponowna autoryzacja i haslo przy kazdym OPEN/CLOSE; deszcz/wiatr i blokady pozostaja aktywne";
      } else if ((doCmd=="force_open" || doCmd=="force_close") && !g_manualMode) {
        cmdOk=false; g_lastRoofBlockReason="Polecenie reczne wymaga MANUAL";
      } else cmdOk=wantsOpen ? roofOpen() : roofClose();
    }
    if (doCmd == "stop") cmdOk = roofStop();
    // reczny E-STOP montazu (panic) i jego reset
    if (doCmd == "estop") cmdOk=mountEstopManual();
    if (doCmd == "estop_reset") {
      cmdOk=g_manualMode && g_manualAuthorized && getParamValue(params,"manual_pass")==String(AUTH_PASS) && getParamValue(params,"confirm")=="INSPECTED_MOUNT";
      if(cmdOk)cmdOk=mountEstopReset();
      else g_lastRoofBlockReason="Przywrocenie montazu wymaga MANUAL, hasla i potwierdzenia inspekcji";
      if (!cmdOk && g_lastRoofBlockReason.length() == 0)
        g_lastRoofBlockReason = "Reset E-STOP nieudany";
    }
    // MANUAL: persistent ownership; RAM authorization and per-move password.
    if (doCmd == "manual_on") {
      String manualPass = getParamValue(params, "manual_pass");
      if (manualPass.length() == 0 || manualPass != String(AUTH_PASS)) {
        g_lastRoofBlockReason = "MANUAL zablokowany: wymagane ponowne potwierdzenie hasłem.";
        cmdOk = false;
      } else {
        cmdOk=manualModeOn();
        if(!cmdOk)g_lastRoofBlockReason="Nie potwierdzono zapisu MANUAL w pamieci";
      }
    }
    if (doCmd=="fault_ack") {
      cmdOk=g_manualMode && g_manualAuthorized && getParamValue(params,"manual_pass")==String(AUTH_PASS) && getParamValue(params,"confirm")=="INSPECTED_STATIONARY";
      if(cmdOk) cmdOk=g_roofFaultKind==RoofFaultKind::PARK_LOST ? acknowledgeParkLossFault() : acknowledgeCommunicationFault();
      else g_lastRoofBlockReason="ACK: wymagany autoryzowany MANUAL, haslo i inspekcja nieruchomego dachu";
    }
    if (doCmd == "manual_off") { cmdOk=manualModeOff();if(!cmdOk)g_lastRoofBlockReason="Nie potwierdzono zapisu AUTO"; }
    // ── Iluminator obserwatorium (TCA9554 DO8 -> driver MOSFET) ──
    //     /cmd?do=light&st=1         -> zapal recznie (niezaleznie od stanu dachu)
    //     /cmd?do=light&st=0         -> zgas
    if (doCmd == "light") {
      bool on = getParamBool(params, "st", true);
      cmdOk = illumRequest(on);                           // false = TCA9554 offline / blad wyjscia
    }
    // Reczne wlaczanie/wylaczanie czerwonej tasmy COB na PWM CH4, niezaleznie od TCA.
    if (doCmd == "led") {
      const bool on = getParamBool(params, "st", false);
#if LED_PWM_ENABLED
      cmdOk = ledPwmRequest(on);
      if (!cmdOk) g_lastRoofBlockReason = "Modul PWM offline lub blad zapisu LED CH4";
#else
      (void)on;
      cmdOk = false;
      g_lastRoofBlockReason = "Sterowanie PWM LED wylaczone w firmware (LED_PWM_ENABLED=0)";
#endif
    }
    // ── Jasnosc tasmy LED (PWM CH4):  /cmd?do=ledbright&pct=1..100
    if (doCmd == "ledbright") {
      const float lpct = getParamFloat(params, "pct", g_ledBrightPct);
#if LED_PWM_ENABLED
      if (!isfinite(lpct) || lpct < 1.0f || lpct > 100.0f) {
        g_lastRoofBlockReason = "Jasnosc swiatla musi byc w zakresie 1..100%";
        cmdOk = false;
      } else {
        const bool changed = fabsf(lpct - g_ledBrightPct) >= 0.5f;
        g_ledBrightPct = lpct;
        if (changed && g_prefs.begin("obscfg", false)) { g_prefs.putFloat("ledBr", g_ledBrightPct); g_prefs.end(); }
        if (g_ledPwmOn && !ledPwmSet(g_ledBrightPct)) {
          g_lastRoofBlockReason = "Blad zapisu PWM - czerwona tasma wylaczona";
          g_ledPwmOn = false;
          cmdOk = false;
        }
      }
#else
      (void)lpct;
      g_lastRoofBlockReason = "Regulacja jasnosci wylaczona w firmware (LED_PWM_ENABLED=0)";
      cmdOk = false;
#endif
    }
    // ── Reczne sterowanie grzalka: /cmd?do=heater&ch=1..3&mode=manual|auto&pct=0..100
    if (doCmd == "heater") {
      const int hch = (int)getParamFloat(params, "ch", 0.0f);
      const bool wantManual = (toLowerCopy(getParamValue(params, "mode")) == "manual");
      const float hpct = getParamFloat(params, "pct", 0.0f);
      cmdOk = heaterManualSet(hch, wantManual, hpct);
    }
    // ═══ budzenie komputera w obserwatorium (Wake-on-LAN) ═══
    //     wywolanie:  http://<IP-ESP>/cmd?do=pcwake
    if (doCmd == "pcwake") cmdOk = wakePC();
    // ═══ /═══

    // zapis edytowalnych progow (chmury/wiatr). Parametry opcjonalne -
    // brak parametru = zostaw biezaca wartosc. Walidacja chroni przed wpisaniem
    // progow wylaczajacych ochrone. Po sukcesie zapis do NVS.
    if (doCmd == "setcfg") {
      float ccClr = getParamFloat(params, "cloud_clear",     CLOUD_DELTA_CLEAR);
      float ccSca = getParamFloat(params, "cloud_scattered", CLOUD_DELTA_SCATTERED);
      float ccOvc = getParamFloat(params, "cloud_overcast",  CLOUD_DELTA_OVERCAST);
      float wWarn = getParamFloat(params, "wind_warn",       WIND_WARN_MS);
      float wSus  = getParamFloat(params, "wind_sustained",  WIND_SUSTAINED_LIMIT_MS);
      float wGust = getParamFloat(params, "wind_gust",       WIND_GUST_LIMIT_MS);
      float wRst  = getParamFloat(params, "wind_reset",      WIND_SAFE_RESET_MS);
      bool ccTrig = g_cloudCanTriggerUnsafe; // decision participation changes only via /api/weather/policy
          bool heatEn1 = getParamBool(params, "heater1", g_heaterChannelEnabled[0]);
      bool heatEn2 = getParamBool(params, "heater2", g_heaterChannelEnabled[1]);
      bool heatEn3 = getParamBool(params, "heater3", g_heaterChannelEnabled[2]);
      float heatW1 = getParamFloat(params, "heater_w1", g_heaterRatedPowerW[0]);
      float heatW2 = getParamFloat(params, "heater_w2", g_heaterRatedPowerW[1]);
      float heatW3 = getParamFloat(params, "heater_w3", g_heaterRatedPowerW[2]);
      bool anemEn = (ANEMOMETER_ENABLED != 0);
      // ═══ kalibracja MLX + tryb sieci (parametry opcjonalne) ═══
      bool  skyEn = getParamBool (params, "sky_corr",    g_skyCorrEnabled);
      float sk1   = getParamFloat(params, "sky_k1",      g_skyK1);
      float sk2   = getParamFloat(params, "sky_k2",      g_skyK2);
      float sk3   = getParamFloat(params, "sky_k3",      g_skyK3);
      float sk4   = getParamFloat(params, "sky_k4",      g_skyK4);
      float sk5   = getParamFloat(params, "sky_k5",      g_skyK5);
      float mso   = getParamFloat(params, "mlx_sky_off", g_mlxSkyOff);
      float mao   = getParamFloat(params, "mlx_amb_off", g_mlxAmbOff);
      float nmF   = getParamFloat(params, "net_mode",    (float)g_netMode);
      bool  acOn  = getParamBool (params, "auto_cloud",  g_autoCloud);   // 
      String err = cfgValidate(ccClr, ccSca, ccOvc, wWarn, wSus, wGust, wRst);
      if (!err.length()) {
        if      (sk1 < 0.0f || sk1 > 100.0f)   err = "sky_k1 poza 0..100";
        else if (sk2 < -100.0f || sk2 > 100.0f) err = "sky_k2 poza -100..100";
        else if (sk3 < 0.0f || sk3 > 100.0f)   err = "sky_k3 poza 0..100";
        else if (sk4 < 0.0f || sk4 > 1000.0f)  err = "sky_k4 poza 0..1000";
        else if (sk5 < 0.0f || sk5 > 200.0f)   err = "sky_k5 poza 0..200";
        else if (mso < -10.0f || mso > 10.0f)  err = "mlx_sky_off poza -10..10";
        else if (mao < -10.0f || mao > 10.0f)  err = "mlx_amb_off poza -10..10";
        else if (nmF < 0.0f || nmF > 2.0f)     err = "net_mode poza 0..2";
        else if (!isfinite(heatW1) || heatW1 < 1.0f || heatW1 > 60.0f ||
                 !isfinite(heatW2) || heatW2 < 1.0f || heatW2 > 60.0f ||
                 !isfinite(heatW3) || heatW3 < 1.0f || heatW3 > 60.0f) err = "Moc grzalek musi byc w zakresie 1..60 W";
        else if (((heatEn1 && !g_heaterChannelEnabled[0]) || (heatEn2 && !g_heaterChannelEnabled[1]) ||
                  (heatEn3 && !g_heaterChannelEnabled[2])) && !HEATER_PWM_ENABLED)
          err = "Najpierw wlacz HEATER_PWM_ENABLED i wgraj firmware";
        else if (((heatEn1 && !g_heaterChannelEnabled[0]) || (heatEn2 && !g_heaterChannelEnabled[1]) ||
                  (heatEn3 && !g_heaterChannelEnabled[2])) && !g_heaterPwmOnline)
          err = "Nie wykryto modulu PWM 4CH pod adresem 2; grzalki pozostaja OFF";
      }
      if (err.length()) {
        g_lastRoofBlockReason = "Progi odrzucone: " + err;
        cmdOk = false;
      } else {
        CLOUD_DELTA_CLEAR = ccClr; CLOUD_DELTA_SCATTERED = ccSca; CLOUD_DELTA_OVERCAST = ccOvc;
        WIND_WARN_MS = wWarn; WIND_SUSTAINED_LIMIT_MS = wSus;
        WIND_GUST_LIMIT_MS = wGust; WIND_SAFE_RESET_MS = wRst;
        g_cloudCanTriggerUnsafe = ccTrig;
        if (!g_cloudCanTriggerUnsafe) { g_cloudUnsafeLatched = false; g_cloudOvercastSinceMs = 0; }
        g_heaterChannelEnabled[0] = heatEn1; g_heaterChannelEnabled[1] = heatEn2; g_heaterChannelEnabled[2] = heatEn3;
        g_heaterRatedPowerW[0] = heatW1; g_heaterRatedPowerW[1] = heatW2; g_heaterRatedPowerW[2] = heatW3;
        g_anemometerEnabled = anemEn;
        // Disabling acquisition never clears a recorded high-wind event.
        // ═══ kalibracja MLX + tryb sieci ═══
        g_skyCorrEnabled = skyEn;
        g_skyK1 = sk1; g_skyK2 = sk2; g_skyK3 = sk3; g_skyK4 = sk4; g_skyK5 = sk5;
        g_mlxSkyOff = mso; g_mlxAmbOff = mao;
        g_autoCloud = acOn;   // auto-kalibracja chmur
        if ((uint8_t)nmF != g_netMode) {
          g_netMode = (uint8_t)nmF;
          Serial.printf("[NET] Tryb sieci: %s\n", g_netMode == 1 ? "TYLKO ETH" :
                        g_netMode == 2 ? "TYLKO WiFi" : "AUTO");
        }
        cfgApplyHysteresis();
        cfgSaveToNVS();
        Serial.println(F("[CFG] Zapisano nowe progi (NVS)"));
        cmdOk = true;
      }
    }
    if (doCmd == "cfgdefault") { cfgResetDefaults(); Serial.println(F("[CFG] Przywrocono domyslne progi")); cmdOk = true; }

    // ═══ akcje auto-kalibracji chmur ═══
    if (doCmd == "cloudseed") {
      float ambRef = isfinite(g_tAir) && lastUpdate_Temperature &&
        uint32_t(millis()-lastUpdate_Temperature)<=astro_roof::MLX_MAX_AGE_MS ? g_tAir : g_tempAmb;
      float rawAir = (!isnan(ambRef) && !isnan(g_tempSky)) ? (ambRef - g_tempSky) : NAN;
      if (acSeedClearNow(ambRef, rawAir, (g_useMlx2AsFailover ? mlx2OK : mlxOK))) {
        cfgSaveToNVS();
        Serial.printf("[CLOUD] Nauczono: czyste niebo @ %.1f C -> delta %.1f C\n", ambRef, rawAir);
        cmdOk = true;
      } else {
        g_lastRoofBlockReason = "Nie moge nauczyc: brak MLX albo delta zbyt niska (to nie czyste niebo)";
        cmdOk = false;
      }
    }
    if (doCmd == "cloudforget") {
      acResetLearning(); cfgSaveToNVS();
      Serial.println(F("[CLOUD] Skasowano nauke auto-kalibracji"));
      cmdOk = true;
    }

    // MLX FAILOVER: przelacznik na MLX2 gdy MLX1 pada
    if (doCmd == "mlx_switchover") {
      String useStr = getParamValue(params, "use");
      if (useStr.length() > 0) {
        bool switchToMlx2 = (useStr.toInt() != 0);
        bool targetOk = g_mlxSamples[switchToMlx2?1:0].usable(millis());
        if (!targetOk) {
          g_lastRoofBlockReason = switchToMlx2 ? "MLX2 niedostepny - nie mozna przelaczyc" : "MLX1 niedostepny - nie mozna przelaczyc";
          Serial.printf("[MLX] Odrzucono przelaczenie na MLX%d - czujnik niedostepny\n",
                        switchToMlx2 ? 2 : 1);
          cmdOk = false;
        } else {
          g_useMlx2AsFailover = switchToMlx2;
          Serial.printf("[MLX] Recznie przelaczono na: MLX%d (mlx1=%d, mlx2=%d)\n",
                        switchToMlx2 ? 2 : 1, mlxOK, mlx2OK);
          cmdOk = true;
        }
      }
    }

    if(doCmd=="localdo"){
      const String idText=getParamValue(params,"n");
      const String stateText=getParamValue(params,"st");
      const int id=idText.toInt();
      if(idText!=String(id) || id<1 || id>7 || !localDoAllowed(id) || (stateText!="0" && stateText!="1")){
        cmdOk=false;g_lastRoofBlockReason="DO niedostepne lub bledny numer/stan";
      }else{
        cmdOk=localDoWrite(id,stateText=="1");
        g_lastRoofBlockReason=cmdOk?"":"DO: brak potwierdzenia TCA; stan obciazenia nieznany";
        eventLogEx(cmdOk?"INFO":"ERROR","DO","DO%u -> %s, %s",id,stateText=="1"?"ON":"OFF",cmdOk?"OK":"FAILED");
      }
    }

    // AUX1 is physical CH7 (zero-based relay index 6). CH4-CH6 remain system channels.
    String auxN = getParamValue(params, "aux");
    String stN  = getParamValue(params, "st");
    bool auxCommand = auxN.length() > 0;
    if (auxCommand) {
      const int n = auxN.toInt();
      const int on = stN.toInt();
      if (auxN != String(n) || n < 1 || n > 5 || !astroAuxAvailable(n) || (stN != "0" && stN != "1")) {
        cmdOk = false;
        g_lastRoofBlockReason = "AUX niedostepne lub bledny numer/st; sprawdz mape kanalow";
      } else if (!relayWrite(astroAuxChannel(n), on != 0)) {
        cmdOk = false;
        g_lastRoofBlockReason = "Sterownik nie potwierdzil zapisu AUX";
      } else {
        g_lastRoofBlockReason = "";
      }
    }

    bool ajaxFound = false;
    bool ajax = getParamBool(params, "ajax", false, &ajaxFound);
    if (ajaxFound && ajax) {
      String resp = String("{\"ok\":") + (cmdOk ? "true" : "false") +
                    ",\"error\":" + jsonQuote(g_lastRoofBlockReason) +
                    ",\"state\":" + buildStateJson() + "}";
      httpSend(c, cmdOk ? 200 : (auxCommand ? 502 : 409), cmdOk ? "OK" : (auxCommand ? "Bad Gateway" : "Conflict"), "application/json", resp);
    } else {
      c.println(F("HTTP/1.1 302 Found\r\nLocation: /\r\nConnection: close\r\n"));
    }
    c.stop();
    if (restartAfterResponse) {
      Serial.println(F("[WEB] Restart programowy z panelu WWW"));
      delay(500);
      ESP.restart();
    }
    return;
  }

  // ── Relay test ────────────────────────────────────────────────────────
  if (method == "POST" && pl == "/relaytest") {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    if (!g_manualMode || astro_roof::moving(roofState)) {
      httpSend(c, 403, "Forbidden", "text/plain; charset=utf-8", "AUX test requires MANUAL and idle roof\n"); c.stop(); return;
    }
    bool all = getParamBool(params, "all", false);
    if (all) { httpSend(c, 403, "Forbidden", "text/plain; charset=utf-8", "Roof relay test disabled; use normal OPEN/CLOSE/STOP commands with safety gates\n"); c.stop(); return; }
    relayTestRun(all);
    httpSend(c, 200, "OK", "text/plain; charset=utf-8", all ? "relaytest: ALL\n" : "relaytest: AUX\n");
    c.stop(); return;
  }

  // ── Manual relay ──────────────────────────────────────────────────────
  if (method == "POST" && pl == "/relay") {
    if (!checkAuth(c, authHeader, params)) { c.stop(); return; }
    int ch = (int)getParamUInt(params, "ch", 255);
    int on = (int)getParamUInt(params, "on", 0);
    if (ch < 3 || ch > 7 || !astroAuxAvailable(astroAuxIdForChannel(ch))) {
      httpSend(c, 403, "Forbidden", "application/json", "{\"ok\":false,\"error\":\"Use roof commands; system channels are protected\"}"); c.stop(); return;
    }
    if (on == 0 || on == 1) {
      const bool ok = relayWrite((uint8_t)ch, on != 0);
      httpSend(c, ok ? 200 : 502, ok ? "OK" : "Bad Gateway", "application/json",
               String("{\"ok\":") + (ok ? "true" : "false") + ",\"error\":" + jsonQuote(ok ? "" : "Modbus write failed") + ",\"ch\":" + ch + ",\"on\":" + (on ? "true" : "false") + "}");
    } else {
      httpSend(c, 400, "Bad Request", "application/json", "{\"ok\":false}");
    }
    c.stop(); return;
  }

  // ── Management ────────────────────────────────────────────────────────
  if (method == "GET" && pl == "/management/apiversions") {
    alpacaSendValue(c, cid, "[1]"); c.stop(); return;
  }
  if (method == "GET" && pl == "/management/v1/configureddevices") {
    alpacaSendValue(c, cid,
      "[{\"DeviceName\":\"" OBSERVATORY_NAME " - Bezpieczenstwo\",\"DeviceType\":\"SafetyMonitor\",\"DeviceNumber\":0,"
      "\"UniqueID\":\"ESP32-SM-0\",\"DriverVersion\":\"1.0\",\"Description\":\"" OBSERVATORY_NAME " - Bezpieczenstwo pogodowe\",\"SupportedActions\":[]},"
      "{\"DeviceName\":\"" OBSERVATORY_NAME " - Pogoda\",\"DeviceType\":\"ObservingConditions\",\"DeviceNumber\":0,"
      "\"UniqueID\":\"ESP32-OC-0\",\"DriverVersion\":\"1.0\",\"Description\":\"" OBSERVATORY_NAME " - Warunki pogodowe\",\"SupportedActions\":[]},"
      "{\"DeviceName\":\"" OBSERVATORY_NAME " - Dach\",\"DeviceType\":\"Dome\",\"DeviceNumber\":0,"
      "\"UniqueID\":\"ESP32-DOME-0\",\"DriverVersion\":\"1.0\",\"Description\":\"" OBSERVATORY_NAME " - Dach odsuwany\",\"SupportedActions\":[\"RoofPosition\",\"RoofState\"]}]");
    c.stop(); return;
  }

  // ── Alpaca ────────────────────────────────────────────────────────────
  // Legacy unprefixed paths must not bypass the authenticated /api/v1 gate.
  if(!pl.startsWith("/api/v1/")) {
    httpSend(c,404,"Not Found","text/plain","Unknown endpoint");c.stop();return;
  }
  String base = pl;
  if (base.startsWith("/api/v1/")) base = base.substring(8);
  if (base.startsWith("/"))       base = base.substring(1);

  // Generic Alpaca traffic is diagnostic; only SafetyMonitor proves NINA activity.
  if (base.startsWith("safetymonitor/") || base.startsWith("observingconditions/") ||
      base.startsWith("dome/") || base.startsWith("telescope/")) {
    g_lastAlpacaContactMs = millis();

  }
  // Tylko SafetyMonitor potwierdza warunek do fizycznego OPEN z HTTP.
  if (base.startsWith("safetymonitor/0/")) {
    g_lastSafetyMonitorContactMs = millis();
    g_commsNoContactSinceMs = 0;
    if (g_commsLost) {
      g_commsLost=false;
      eventLogEx("INFO","NINA","SafetyMonitor contact recovered");
    }
  }

  // SafetyMonitor
  if (base.startsWith("safetymonitor/0/")) {
    String member = base.substring(16);
    if (member == "name" && method == "GET")        { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Bezpieczenstwo")); }
    else if (member == "description" && method == "GET") { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Bezpieczenstwo pogodowe")); }
    else if (member == "driverinfo" && method == "GET")  { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Bezpieczenstwo")); }
    else if (member == "driverversion" && method == "GET") { alpacaSendValue(c, cid, jsonQuote("1.0")); }
    else if (member == "interfaceversion" && method == "GET") { alpacaSendValue(c, cid, "3"); }
    else if (member == "supportedactions" && method == "GET") { alpacaSendValue(c, cid, "[]"); }
    else if (member == "hasconnectanddevicestate" && method == "GET") { alpacaSendValue(c, cid, "true"); }
    else if (member == "connecting" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "connect" && method == "PUT")    { g_smConnected = true;  alpacaSendValue(c, cid, "null"); }
    else if (member == "disconnect" && method == "PUT") { g_smConnected = false; alpacaSendValue(c, cid, "null"); }
    else if (member == "connected") {
      if (method == "GET") { alpacaSendValue(c, cid, g_smConnected ? "true" : "false"); }
      else if (method == "PUT") {
        bool found = false;
        bool v = getParamBool(params, "Connected", g_smConnected, &found);
        if (!found) alpacaSendError(c, cid, 1025, "Missing Connected");
        else { g_smConnected = v; alpacaSendValue(c, cid, "null"); }
      } else alpacaSendError(c, cid, 1036, "Method not implemented");
    }
    else if (member == "issafe" && method == "GET") {
      // UWAGA: w MANUAL Alpaca zawsze widzi UNSAFE (ochrona automatyki)
      bool alpacaSafe = !g_manualMode && !g_unsafe && safetyInputsUsable();
      alpacaSendValue(c, cid, alpacaSafe ? "true" : "false");
    }
    else if (member == "devicestate" && method == "GET") {
      bool alpacaSafe = !g_manualMode && !g_unsafe && safetyInputsUsable();
      String val = "[{\"Name\":\"IsSafe\",\"Value\":";
      val += alpacaSafe ? "true" : "false";
      val += "}]";
      alpacaSendValue(c, cid, val);
    }
    else alpacaSendError(c, cid, 1036, "Not implemented");
    c.stop(); return;
  }

  // ObservingConditions
  if (base.startsWith("observingconditions/0/")) {
    String member = base.substring(22);
    if (member == "name" && method == "GET")        { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Pogoda")); }
    else if (member == "description" && method == "GET") { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Warunki pogodowe")); }
    else if (member == "driverinfo" && method == "GET")  { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Pogoda")); }
    else if (member == "driverversion" && method == "GET") { alpacaSendValue(c, cid, jsonQuote("1.0")); }
    else if (member == "interfaceversion" && method == "GET") { alpacaSendValue(c, cid, "3"); }
    else if (member == "supportedactions" && method == "GET") { alpacaSendValue(c, cid, "[]"); }
    else if (member == "hasconnectanddevicestate" && method == "GET") { alpacaSendValue(c, cid, "true"); }
    else if (member == "connecting" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "connect" && method == "PUT")    { g_ocConnected = true;  alpacaSendValue(c, cid, "null"); }
    else if (member == "disconnect" && method == "PUT") { g_ocConnected = false; alpacaSendValue(c, cid, "null"); }
    else if (member == "connected") {
      if (method == "GET") { alpacaSendValue(c, cid, g_ocConnected ? "true" : "false"); }
      else if (method == "PUT") {
        bool found = false; bool v = getParamBool(params, "Connected", g_ocConnected, &found);
        if (!found) alpacaSendError(c, cid, 1025, "Missing Connected");
        else { g_ocConnected = v; alpacaSendValue(c, cid, "null"); }
      } else alpacaSendError(c, cid, 1036, "Method not implemented");
    }
    else if (member == "averageperiod") {
      if (method == "GET") { alpacaSendValue(c, cid, String(g_averagePeriod, 1)); }
      else if (method == "PUT") {
        bool found = false; float ap = getParamFloat(params, "AveragePeriod", g_averagePeriod, &found);
        if (!found) alpacaSendError(c, cid, 1025, "Missing AveragePeriod");
        else { g_averagePeriod = ap; alpacaSendValue(c, cid, "null"); }
      } else alpacaSendError(c, cid, 1036, "Method not implemented");
    }
    else if (member == "sensornames" && method == "GET") {
      alpacaSendValue(c, cid, R"(["Temperature","Humidity","DewPoint","SkyTemperature","CloudCover","Pressure","WindSpeed","RainRate","AveragePeriod"])");
    }
    else if (member == "refresh" && method == "PUT") { alpacaSendValue(c, cid, "null"); }
    else if (member == "focustemperatures" && method == "GET") { alpacaSendValue(c, cid, focuserTemperaturesJson()); }
    else if (member == "temperature" && method == "GET")    { alpacaSendValue(c, cid, jsonNumOrNull(g_tAir, 2)); }
    else if (member == "humidity" && method == "GET")       { alpacaSendValue(c, cid, jsonNumOrNull(g_hAir, 1)); }
    else if (member == "dewpoint" && method == "GET")       { alpacaSendValue(c, cid, jsonNumOrNull(g_dewPoint, 2)); }
    else if (member == "skytemperature" && method == "GET") { alpacaSendValue(c, cid, jsonNumOrNull(g_tempSky, 2)); }
    else if (member == "cloudcover" && method == "GET") {
      float cc = isnan(g_cloudDelta) ? NAN :
                 (g_cloudDelta >= CLOUD_DELTA_CLEAR ? 0.0f :
                  g_cloudDelta <= 0.0f ? 100.0f :
                  (1.0f - (g_cloudDelta / CLOUD_DELTA_CLEAR)) * 100.0f);
      alpacaSendValue(c, cid, jsonNumOrNull(cc, 1));
    }
    else if (member == "pressure" && method == "GET")  { alpacaSendValue(c, cid, jsonNumOrNull(g_pressure_hPa, 1)); }
    else if (member == "windspeed" && method == "GET") { alpacaSendMeasurement(c, cid, windMeasurementUsable()?g_wind_ms:NAN, 2); }
    else if (member == "winddirection" && method == "GET") { alpacaSendValue(c, cid, "0"); }
    else if (member == "windgust" && method == "GET")  { alpacaSendMeasurement(c, cid, windMeasurementUsable()?g_windGust:NAN, 2); }
    else if (member == "rainrate" && method == "GET")  { alpacaSendValue(c, cid, g_rain ? "1.0" : "0.0"); }
    else if (member == "skybrightness" && method == "GET") { alpacaSendValue(c, cid, jsonNumOrNull(g_skyLux, 1)); }
    else if (member == "starfwhm" && method == "GET")  { alpacaSendValue(c, cid, "0"); }
    else if (member == "skymagnitudeperarcsecondsquared" && method == "GET") { alpacaSendValue(c, cid, "0"); }
    else if (member == "skyquality" && method == "GET") { alpacaSendValue(c, cid, "0"); }
    else if (member.startsWith("timelastsampledfor")) {
      String sn = member.substring(18); sn.trim();
      float ts = -1.0f;
      if (sn == "temperature")    ts = timeSinceMs(lastUpdate_Temperature);
      else if (sn == "humidity")  ts = timeSinceMs(lastUpdate_Humidity);
      else if (sn == "dewpoint")  ts = timeSinceMs(lastUpdate_DewPoint);
      else if (sn == "skytemperature") ts = timeSinceMs(lastUpdate_SkyTemperature);
      else if (sn == "cloudcover") ts = timeSinceMs(lastUpdate_CloudCover);
      else if (sn == "pressure")  ts = timeSinceMs(lastUpdate_Pressure);
      else if (sn == "windspeed") ts = timeSinceMs(lastUpdate_WindSpeed);
      else if (sn == "rainrate")  ts = timeSinceMs(lastUpdate_RainRate);
      alpacaSendValue(c, cid, String(ts, 1));
    }
    else if (member == "sensordescription") {
      String sn = toLowerCopy(getParamValue(params, "SensorName"));
      if (sn == "temperature") alpacaSendValue(c, cid, jsonQuote("Air temperature (BME280)"));
      else if (sn == "humidity") alpacaSendValue(c, cid, jsonQuote("Relative humidity (BME280)"));
      else if (sn == "dewpoint") alpacaSendValue(c, cid, jsonQuote("Dew point (computed)"));
      else if (sn == "skytemperature") alpacaSendValue(c, cid, jsonQuote("Sky temp (MLX90614)"));
      else if (sn == "cloudcover") alpacaSendValue(c, cid, jsonQuote("Cloud cover (MLX delta)"));
      else if (sn == "pressure") alpacaSendValue(c, cid, jsonQuote("Pressure (BMP280/BME280)"));
      else if (sn == "windspeed") alpacaSendValue(c, cid, jsonQuote("Analog wind sensor; model in calibration record"));
      else if (sn == "rainrate") alpacaSendValue(c, cid, jsonQuote("Rain (RG11)"));
      else alpacaSendError(c, cid, 1036, "Unknown sensor");
    }
    else if (member == "devicestate" && method == "GET") {
      String val = "[]";
      alpacaSendValue(c, cid, val);
    }
    else alpacaSendError(c, cid, 1036, "Not implemented");
    c.stop(); return;
  }

  // Dome
  if (base.startsWith("dome/0/")) {
    String member = base.substring(7);
    if (member == "name" && method == "GET")        { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Dach")); }
    else if (member == "description" && method == "GET") { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Dach odsuwany")); }
    else if (member == "driverinfo" && method == "GET")  { alpacaSendValue(c, cid, jsonQuote(OBSERVATORY_NAME " - Dach")); }
    else if (member == "driverversion" && method == "GET") { alpacaSendValue(c, cid, jsonQuote("1.0")); }
    else if (member == "interfaceversion" && method == "GET") { alpacaSendValue(c, cid, "2"); }
    else if (member == "supportedactions" && method == "GET") { alpacaSendValue(c, cid, "[\"RoofPosition\",\"RoofState\"]"); }
    else if (member == "hasconnectanddevicestate" && method == "GET") { alpacaSendValue(c, cid, "true"); }
    else if (member == "connecting" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "connect" && method == "PUT")    { g_domeConnected = true;  alpacaSendValue(c, cid, "null"); }
    else if (member == "disconnect" && method == "PUT") { g_domeConnected = false; alpacaSendValue(c, cid, "null"); }
    else if (member == "connected") {
      if (method == "GET") { alpacaSendValue(c, cid, g_domeConnected ? "true" : "false"); }
      else if (method == "PUT") {
        bool found = false; bool v = getParamBool(params, "Connected", g_domeConnected, &found);
        if (!found) alpacaSendError(c, cid, 1025, "Missing Connected");
        else { g_domeConnected = v; alpacaSendValue(c, cid, "null"); }
      } else alpacaSendError(c, cid, 1036, "Method not implemented");
    }
    else if (member == "shutterstatus" && method == "GET") { alpacaSendValue(c, cid, String(domeShutterStatusFromRoof())); }
    else if (member == "slewing" && method == "GET") { alpacaSendValue(c, cid, domeIsSlewing() ? "true" : "false"); }
    else if (member == "altitude" && method == "GET") { alpacaSendValue(c, cid, "0"); }
    else if (member == "azimuth" && method == "GET")  { alpacaSendValue(c, cid, "0"); }
    else if (member == "athome" && method == "GET")   { alpacaSendValue(c, cid, "false"); }
    else if (member == "atpark" && method == "GET")   { alpacaSendValue(c, cid, (roofState == RoofState::CLOSED) ? "true" : "false"); }
    else if (member == "canfindhome" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "canpark" && method == "GET")  { alpacaSendValue(c, cid, "false"); }
    else if (member == "cansetaltitude" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "cansetazimuth" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "cansetpark" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "cansetshutter" && method == "GET") { alpacaSendValue(c, cid, "true"); }
    else if (member == "canslave" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "cansyncazimuth" && method == "GET") { alpacaSendValue(c, cid, "false"); }
    else if (member == "openshutter" && method == "PUT") {
      if (g_manualMode) alpacaSendError(c,cid,1036,"MANUAL: ruch przez WWW po potwierdzeniu; STOP nadal dostepny");
      else if (g_unsafe) alpacaSendError(c, cid, 1036, "Safety UNSAFE - cannot open");
      else if (roofOpen()) alpacaSendValue(c, cid, "null");
      else alpacaSendError(c, cid, 1036, g_lastRoofBlockReason);
    }
    else if (member == "closeshutter" && method == "PUT") {
      if (roofClose()) alpacaSendValue(c, cid, "null");
      else alpacaSendError(c, cid, 1036, g_lastRoofBlockReason);
    }
    else if (member == "abortslew" && method == "PUT") {
      if (roofStop()) alpacaSendValue(c, cid, "null");
      else alpacaSendError(c, cid, 1036, g_lastRoofBlockReason);
    }
    else if (member == "findhome" && method == "PUT")     { alpacaSendError(c, cid, 1036, "Not implemented"); }
    else if (member == "park" && method == "PUT")         { alpacaSendError(c, cid, 1036, "Not implemented"); }
    else if (member == "slewtoaltitude" && method == "PUT") { alpacaSendError(c, cid, 1036, "Fixed roof"); }
    else if (member == "slewtoazimuth" && method == "PUT")  { alpacaSendError(c, cid, 1036, "Fixed roof"); }
    else if (member == "synctoazimuth" && method == "PUT")  { alpacaSendError(c, cid, 1036, "Fixed roof"); }
    else if (member == "action" && method == "PUT") {
      String action = toLowerCopy(getParamValue(params, "Action"));
      if (action == "roofposition") {
        int pos = (roofState == RoofState::CLOSED) ? 0 : (roofState == RoofState::OPEN) ? 100 : 50;
        alpacaSendValue(c, cid, String(pos));
      } else if (action == "roofstate") {
        String s = "unknown";
        switch (roofState) {
          case RoofState::OPEN:    s = "open";    break;
          case RoofState::CLOSED:  s = "closed";  break;
          case RoofState::OPENING: s = "opening"; break;
          case RoofState::CLOSING: s = "closing"; break;
          case RoofState::STOPPED: s = "stopped"; break;
          case RoofState::ERROR:   s = "error";   break;
          default: break;
        }
        alpacaSendValue(c, cid, jsonQuote(s));
      } else alpacaSendError(c, cid, 1036, "Action not implemented");
    }
    else if (member == "devicestate" && method == "GET") { alpacaSendValue(c, cid, "[]"); }
    else alpacaSendError(c, cid, 1036, "Not implemented");
    c.stop(); return;
  }

  // Fallback
  if (pl.startsWith("/api/") || pl.startsWith("/management") || pl.startsWith("/safetymonitor") || pl.startsWith("/observingconditions")) {
    alpacaSendError(c, cid, 1036, "Not implemented");
  } else {
    httpSendRaw(c, 200, "OK", "text/html; charset=utf-8", INDEX_HTML, INDEX_HTML_LEN);
  }
  c.stop();
}

static void otaBeginIfConfigured() {
#if USE_OTA && USE_WIFI
  if (!g_wifiReady || g_otaReady) return;
  // bezpieczne OTA. Haslo WYMAGANE - bez niego OTA nie startuje.
  if (strlen(OTA_PASSWORD) == 0) {
    Serial.println(F("[OTA] WYLACZONE: brak hasla. Ustaw -D OTA_PASSWORD=... w platformio.ini."));
    return;
  }
  ArduinoOTA.setHostname(OBSERVATORY_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    g_otaActive = true;
    if (modbusRelayOK) {
      roofRelaysOff();
      Serial.println(F("[OTA] START wgrywania - relaye dachu OFF"));
    } else {
      Serial.println(F("[OTA] START wgrywania (MODBUS RELAY offline - przekazniki dachu juz nieaktywne)"));
    }
#if USE_TASK_WDT
    esp_task_wdt_reset();
#endif
  });
  ArduinoOTA.onEnd([]() { Serial.println(F("[OTA] KONIEC - restart")); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
#if USE_TASK_WDT
    esp_task_wdt_reset();
#endif
    static unsigned int lastPct = 999;
    unsigned int pct = (t ? (p * 100u / t) : 0);
    if (pct != lastPct && (pct % 10 == 0)) { lastPct = pct; Serial.printf("[OTA] %u%%\n", pct); }
  });
  ArduinoOTA.onError([](ota_error_t e) {
    g_otaActive = false;
    Serial.printf("[OTA] BLAD %u\n", (unsigned)e);
  });
  ArduinoOTA.begin();
  g_otaReady = true;
  Serial.printf("[OTA] Bezpieczne OTA WLACZONE | host=%s (haslo wymagane)\n", OBSERVATORY_HOSTNAME);
#endif
}

// ============== Setup ==============
void setup() {
  onlineBegin(); // independent of Telegram compile flag; no outbound work until queued
Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println(F("=== START ESP32-S3 Obserwatorium :: " FW_VERSION " ==="));
  Serial.printf("[CONFIG] board=%s roof_motion=%d PWM=%d heaters=%d LED=%d\n", OBS_BOARD_8RO?"8RO SPARE":"POE MAIN", ROOF_MOTION_ENABLED,PWM_MODULE_ENABLED,HEATER_PWM_ENABLED,LED_PWM_ENABLED);
  eventLogInit();
  g_previousBreadcrumb={g_resetBreadcrumb.magic,g_resetBreadcrumb.stage,g_resetBreadcrumb.at,g_resetBreadcrumb.endpoint,g_resetBreadcrumb.check};
  diagnosticStage(1);
  operatorLoad();
#if MOUNT_LIMIT_ENABLED
  pinMode(MOUNT_LIMIT_PIN,INPUT_PULLUP);
  if(mountLimitRawTripped()) {g_mountPowerRestoreBlocked=true;g_mountEstop=true;g_mountEstopReason="Wejscie ochrony aktywne przy starcie";operatorStore("mountTrip",true);}
#endif
  eventLogEx("INFO", "BOOT", "Firmware start: %s", FW_VERSION);

  // net-task (rdzen 0) MUSI wystartowac zanim cokolwiek w setup()
  // moze probowac wyslac Telegram - inaczej telegramSendNow() zobaczy
  // g_netQueue==nullptr i pierwsza wiadomosc (np. startowa) przepadnie.
#if USE_TELEGRAM
  netTaskInit();
#endif

  // DIAGNOSTYKA: dlaczego ESP32 sie zrestartowal? KLUCZOWE do znalezienia
  // przyczyny nocnych restartow (crash/brownout/WDT/panic).
  esp_reset_reason_t rr = esp_reset_reason();
  const char* rrTxt = "NIEZNANY";
  switch (rr) {
    case ESP_RST_POWERON:   rrTxt = "Power-on (normalne wlaczenie zasilania)"; break;
    case ESP_RST_EXT:       rrTxt = "Reset zewnetrzny (przycisk RESET)"; break;
    case ESP_RST_SW:        rrTxt = "Reset programowy (ESP.restart())"; break;
    case ESP_RST_PANIC:     rrTxt = "!!! PANIC - crash/exception w kodzie !!!"; break;
    case ESP_RST_INT_WDT:   rrTxt = "!!! Interrupt Watchdog - kod blokowal przerwania !!!"; break;
    case ESP_RST_TASK_WDT:  rrTxt = "Task watchdog: zadanie lub przyczyna wymagaja diagnozy"; break;
    case ESP_RST_WDT:       rrTxt = "!!! Inny watchdog (hw/rtc) !!!"; break;
    case ESP_RST_BROWNOUT:  rrTxt = "!!! BROWNOUT - spadek napiecia zasilania !!!"; break;
    case ESP_RST_DEEPSLEEP: rrTxt = "Wybudzenie z deep sleep"; break;
    default: break;
  }
  if(breadcrumbValid(g_previousBreadcrumb) && rr!=ESP_RST_POWERON && rr!=ESP_RST_BROWNOUT) {
    eventLogEx("WARN","BOOT","Previous execution: stage=%lu endpoint=%lu millis=%lu (not a backtrace)",
      (unsigned long)g_previousBreadcrumb.stage,(unsigned long)g_previousBreadcrumb.endpoint,(unsigned long)g_previousBreadcrumb.at);
  }
  Serial.print(F(">>> POWOD RESTARTU: ")); Serial.println(rrTxt);
  eventLogEx((rr == ESP_RST_POWERON || rr == ESP_RST_EXT) ? "INFO" : "CRITICAL",
             "BOOT", "RESET: %s", rrTxt);
  Serial.printf(">>> Wolny heap na starcie: %u bytes (min ever: %u)\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());

  Serial.printf("Pins: I2C SDA=%d SCL=%d | RG11=%d | WIND=%d | LIM_C=%d | LIM_O=%d | PARK=%d\n",
                I2C_SDA_PIN, I2C_SCL_PIN, RG11_PIN, WIND_PIN,
                LIMIT_CLOSED_PIN, LIMIT_OPEN_PIN, PARK_SENSOR_PIN);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000); // 100kHz (szybciej niz , sensory to wytrzymaja)
  // FIX KRYTYCZNY: bez tego, zawieszony/przegrzany czujnik I2C (clock-stretch)
  // moze zablokowac Wire.* W NIESKONCZONOSC -> loop() wisi -> Task Watchdog po 30s
  // (dokladnie to zdarzylo sie 30.06 o 14:35: "Task Watchdog - loop() zawiesil sie >30s").
  // Timeout 200ms: po tylu ms bez odpowiedzi Wire zglasza blad zamiast wisiec wiecznie.
  Wire.setTimeOut(200);

  // wczytaj edytowalne progi (chmury/wiatr) z NVS (lub domyslne)
  cfgLoadFromNVS();
  delay(100);
  i2cScanOnce();
  if (RTC_ENABLED) rtcBegin();

  // ═══ WAVESHARE 17658: MODBUS RTU relay przez izolowany RS485 ═══
  if (modbusRelayBegin()) {
    eventLogEx("INFO", "MODBUS", "Waveshare 17658 OK addr=%u baud=%lu", (unsigned)MODBUS_RELAY_ADDR, (unsigned long)MODBUS_RELAY_BAUD);
  } else {
    eventLogEx("CRITICAL", "MODBUS", "Waveshare 17658 OFFLINE addr=%u", (unsigned)MODBUS_RELAY_ADDR);
  }
#if MOUNT_POWER_RELAY_ENABLED
  relayState[MOUNT_POWER_RELAY_CH]=g_mountPowerRestoreBlocked;
#endif
  modbusRelayPollNow(); // fresh readback after safe initialization

#if PWM_MODULE_ENABLED
  // Osobny slave PWM (adres 2) na tej samej magistrali A/B co 17658 (adres 1).
  // Pierwsza czynnoscia jest ustawienie wszystkich 4 kanalow na 0%.
  g_heaterLastProbeMs = millis();
  g_heaterPwmOnline = heaterPwmAllOff();
  if (g_heaterPwmOnline) {
    Serial.printf("[HEATERS] Waveshare PWM 4CH OK @%u, wszystkie wyjscia OFF\n", (unsigned)HEATER_PWM_ADDR);
    eventLogEx("INFO", "HEATERS", "PWM 4CH online @%u; all outputs initialized OFF", (unsigned)HEATER_PWM_ADDR);
  } else {
    Serial.printf("[HEATERS] Brak PWM 4CH @%u; grzalki zablokowane\n", (unsigned)HEATER_PWM_ADDR);
    eventLogEx("WARN", "HEATERS", "PWM module offline @%u; heaters locked OFF", (unsigned)HEATER_PWM_ADDR);
  }
#endif

  // TCA9554: DO1..DO7 pozostaja dostepne lokalnie, DO8 steruje oswietleniem.
#if !OBS_BOARD_8RO
  const uint8_t illumOffBit = (ILLUM_DO_ACTIVE_HIGH == 0)
      ? (uint8_t)(1U << ILLUM_DO_CHANNEL) : 0x00;
  tca.setInitialShadow(illumOffBit);
  if (tca.begin(g_tcaAddr, &Wire)) {
    tcaOK = true;
    Serial.printf("TCA9554 OK @0x%02X (DO1..DO7 lokalne; DO%u=LIGHT)\n",
                  g_tcaAddr, (unsigned)ILLUM_DO_CHANNEL + 1);
  } else {
    Serial.printf("??  BRAK TCA9554 @0x%02X (DO8/LIGHT offline)\n", g_tcaAddr);
  }


#endif

  // Krancowki
  pinMode(LIMIT_CLOSED_PIN, INPUT_PULLUP);
  pinMode(LIMIT_OPEN_PIN,   INPUT_PULLUP);
  updateRoofStateFromLimits();
  Serial.print(F("ROOF INIT: ")); Serial.println(roofStateName(roofState));
  eventLogEx("INFO", "ROOF", "INIT state=%s", roofStateName(roofState));

  // Oswietlenie jest inicjalizowane dopiero po TCA9554 i startuje OFF.
  illumSetup();

  // Emergency close button
  pinMode(EMERGENCY_CLOSE_PIN, INPUT_PULLUP);
  Serial.printf("Emergency CLOSE: GPIO%d\n", EMERGENCY_CLOSE_PIN);

  // czujnik kolizji NPN NC z opcjonalnym przekaznikiem zasilania
#if COLLISION_SENSOR_ENABLED
  pinMode(COLLISION_SENSOR_PIN, INPUT_PULLUP);
#if COLLISION_POWER_RELAY_ENABLED
  // Czujnik zasilany przez przekaznik ch7 - domyslnie wylamany (laser OFF)
  // Stan poczatkowy: zasilanie OFF, flagi false (NPN bez zasil. = HIGH na pinie = "clear")
  collisionPowerOff();
  g_collisionSensorActive  = false;
  g_collisionSensorRawLast = false;
  g_collisionLastChangeMs  = millis();
  Serial.printf("Collision sensor NPN NC: GPIO%d | PWR relay ch%d | ACTIVE_LOW=%d | zasilanie OFF (wlaczane tylko przy ruchu dachu)\n",
                COLLISION_SENSOR_PIN, COLLISION_POWER_RELAY_CH, COLLISION_SENSOR_ACTIVE_LOW);
#else
  // Bez przekaznika zasilania: czujnik ciagle wlaczony, odczytaj stan poczatkowy
  g_collisionSensorRawLast = readCollisionRawActive();
  g_collisionLastChangeMs  = millis();
  g_collisionSensorActive  = g_collisionSensorRawLast;
  Serial.printf("Collision sensor NPN NC: GPIO%d | ACTIVE_LOW=%d | initial=%d (bez PWR relay)\n",
                COLLISION_SENSOR_PIN, COLLISION_SENSOR_ACTIVE_LOW, (int)g_collisionSensorActive);
#endif
#else
  Serial.println(F("Collision sensor: OFF (wlacz -D COLLISION_SENSOR_ENABLED=1)"));
#endif

  // Park guard GPIO (dwa reedy - RA + DEC)
  pinMode(PARK_SENSOR_PIN,     INPUT_PULLUP);   // os RA
  pinMode(PARK_SENSOR_DEC_PIN, INPUT_PULLUP);   // os DEC
  Serial.printf("Park guard: RA=GPIO%d DEC=GPIO%d | GUARD_ENABLED=%d (park = OBIE osie)\n",
                PARK_SENSOR_PIN, PARK_SENSOR_DEC_PIN, PARK_GUARD_ENABLED);

  // krancowka overtravel montazu (E-STOP)
#if MOUNT_LIMIT_ENABLED
  pinMode(MOUNT_LIMIT_PIN, INPUT_PULLUP);
  mountPowerCut(g_mountPowerRestoreBlocked); // never auto-acknowledge a persisted/raw trip
  g_mountLimitRawLast  = mountLimitRawTripped();
  g_mountLimitChangeMs = millis();
  Serial.printf("Mount E-STOP: GPIO%d | NC(trip@HIGH)=%d | pwr_relay_ch%d=%d | start_trip=%d\n",
                MOUNT_LIMIT_PIN, MOUNT_LIMIT_TRIP_WHEN_HIGH, MOUNT_POWER_RELAY_CH,
                MOUNT_POWER_RELAY_ENABLED, (int)g_mountLimitRawLast);
  if (g_mountLimitRawLast) {
    // Po starcie juz w stanie trip - zatrzasnij od razu (bezpieczniej)
    mountEstopTrigger("Krancowka montazu w stanie TRIP juz przy starcie");
  }
#else
  Serial.println(F("Mount E-STOP: OFF (wlacz -D MOUNT_LIMIT_ENABLED=1)"));
#endif

#if ENABLE_DHT22
  // DHT22 (opcjonalny; domyslnie OFF - GPIO47, wspoldzielony z linia TF)
  dht.begin();
  dhtOK = true;
#else
  dhtOK = false;
#endif

  // MLX
  mlxOK = (ENABLE_MLX90614 && mlx.begin());
  Serial.println(mlxOK ? F("MLX90614 OK") : F("??  MLX90614 brak"));
  eventLogEx(mlxOK ? "INFO" : "CRITICAL", "MLX",
             mlxOK ? "MLX1 OK @0x5A" : "MLX1 OFFLINE @0x5A");

  // drugi MLX90614 (0x5B, nowa glowica) - tylko zbieranie danych
  mlx2OK = (ENABLE_MLX90614_2 && mlx2.begin(MLX2_I2C_ADDR));
  Serial.println(mlx2OK ? F("MLX90614 #2 (0x5B) OK") : F("??  MLX90614 #2 (0x5B) brak"));
  eventLogEx(mlx2OK ? "INFO" : "WARN", "MLX",
             mlx2OK ? "MLX2 OK @0x5B" : "MLX2 OFFLINE @0x5B");

  // BME280 jest podstawowym czujnikiem temperatury/wilgotnosci/cisnienia.
#if ENABLE_BME280
  bme280OK = bme280.begin(0x76) || bme280.begin(0x77);
  if (bme280OK) { g_baroType = BARO_BME280; Serial.println(F("BME280 OK (cisnienie + temp + wilgotnosc)")); }
#endif
#if ENABLE_BMP280
  if (!bme280OK) {
    bmpOK = bmp.begin(0x76) || bmp.begin(0x77);
    if (bmpOK) { g_baroType = BARO_BMP280; Serial.println(F("BMP280 OK")); }
  }
#endif
  if (g_baroType == BARO_NONE) Serial.println(F("??  BRAK aktywnego barometru BME280/BMP280"));

#if ENABLE_SI1145
  uvOK = si1145.begin();
  Serial.println(uvOK ? F("SI1145 OK") : F("??  SI1145 brak"));
#else
  uvOK = false;
#endif

#if ENABLE_DS18B20
  ds18b20.begin();
  dsOK = (ds18b20.getDeviceCount() > 0);
  if (dsOK) { Serial.printf("DS18B20 OK, devices: %d\n", ds18b20.getDeviceCount()); ds18b20.setResolution(10); }
  else Serial.println(F("??  DS18B20 brak"));
#else
  dsOK = false;
#endif

#if ENABLE_BH1750
  bhOK = bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(bhOK ? F("BH1750 OK") : F("??  BH1750 brak"));
#else
  bhOK = false;
#endif

#if ENABLE_TSL2591
  tslOK = tsl591.begin();
  if (tslOK) {
    tsl591.setGain(TSL2591_GAIN_MED);
    tsl591.setTiming(TSL2591_INTEGRATIONTIME_200MS);
    g_tslGainIdx = 1;
    Serial.println(F("TSL2591 OK"));
  } else { Serial.println(F("??  TSL2591 brak")); }
#else
  tslOK = false;
#endif

  // GPIO
#if ENABLE_RG11
  pinMode(RG11_PIN, INPUT_PULLUP);
  // RG-11 na przerwaniu - onset deszczu lapiemy natychmiast, niezaleznie od loop()
#if RG11_RAIN_WHEN_LOW
  attachInterrupt(digitalPinToInterrupt(RG11_PIN), rg11Isr, FALLING);  // LOW = mokro
#else
  attachInterrupt(digitalPinToInterrupt(RG11_PIN), rg11Isr, RISING);
#endif
#endif
  pinMode(WIND_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(WIND_PIN, ADC_11db);
  const esp_adc_cal_value_t windCalType=esp_adc_cal_characterize(ADC_UNIT_1,ADC_ATTEN_DB_11,
    ADC_WIDTH_BIT_12,1100,&g_windAdcChars);
  g_windValidity.adcCalibrated=(windCalType==ESP_ADC_CAL_VAL_EFUSE_TP_FIT ||
    windCalType==ESP_ADC_CAL_VAL_EFUSE_TP || windCalType==ESP_ADC_CAL_VAL_EFUSE_VREF);
  windCalibrationLoad();

  // WiFi
#if USE_WIFI
  Serial.println(F("Init WiFi..."));
  WiFi.setHostname(OBSERVATORY_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // modem-sleep OFF - stabilniejszy zapas (TLS/UI po WiFi bez czkawki)
  WiFi.begin(WIFI_SSID_C, WIFI_PASS_C);
  g_wifiLastConnectAttemptMs = millis();
  if (WiFi.status() == WL_CONNECTED) {
    g_wifiReady = true;
    Serial.print(F("WiFi IP: ")); Serial.println(WiFi.localIP());
    wifiServer.begin();
    Serial.printf("WiFi HTTP/Alpaca server on port %d\n", SERVER_PORT);
  } else {
    Serial.println(F("WiFi laczy sie w tle; setup nie czeka na punkt dostepowy"));
  }
#endif

  rtcNtpStart();
  otaBeginIfConfigured(); // jesli WiFi polaczy sie pozniej, loop() uruchomi OTA po polaczeniu

#if USE_W5500
  Serial.println(F("Init W5500..."));
  SPI.begin(ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  Ethernet.init(ETH_CS_PIN);
#if DIAG_ETH_DHCP
  // DIAG: test hipotezy "router wpuszcza do netu tylko adresy ze swojego DHCP".
  // Prosimy o adres przez DHCP zamiast wpisywac go na sztywno. UWAGA: domyslnie
  // ta funkcja moze blokowac sie do 60s BEZ ZADNEGO komunikatu na Serialu, jesli
  // router odpowie z opoznieniem - to wyglada jak zawieszka, ale nia nie jest.
  // Diagnostycznie ograniczamy oczekiwanie do 3s / 1s na probe.
  Serial.println(F("[DIAG] Probuje DHCP (test: czy router wymaga wlasnego DHCP-lease)..."));
  Serial.println(F("[DIAG] Limit oczekiwania DHCP: ok. 3s; potem statyczny IP."));
  bool dhcpOk = Ethernet.begin(macAddr, 3000, 1000);
  if (!dhcpOk) {
    Serial.println(F("[DIAG] DHCP nie dostalo adresu - wracam do statycznego IP"));
    Ethernet.begin(macAddr, ethIP, ethDNS, ethGW, ethSN);
  }
#else
  Ethernet.begin(macAddr, ethIP, ethDNS, ethGW, ethSN);  // DNS 8.8.8.8
#endif
  delay(200);
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(F("??  W5500 nie wykryty"));
  } else {
    g_ethReady = true;
    Serial.print(F("ETH IP: ")); Serial.println(Ethernet.localIP());
    // DIAG: stan fizyczny linku (kabel/PHY) - niezalezne od tego czy DHCP/DNS/TCP dziala.
    // LinkON = kabel podlaczony i PHY widzi sygnal z drugiej strony (switch/router).
    // LinkOFF = kabel odlaczony/uszkodzony/zly port - wtedy DHCP/TCP NIE MAJA SZANS zadzialac,
    // niezaleznie od routera. To rozstrzyga "kabel/sprzet" vs "router/siec".
    EthernetLinkStatus ls = Ethernet.linkStatus();
    Serial.print(F("[DIAG] Link PHY: "));
    Serial.println(ls == LinkON ? F("ON (kabel OK, widzi sygnal)") :
                    ls == LinkOFF ? F("OFF (!!) - kabel odlaczony/uszkodzony/zly port") :
                                    F("UNKNOWN (sterownik nie wspiera odczytu)"));
    ethServer.begin();

    // Nie wykonujemy testowego TCP w setup(): zdrowie interfejsu ustala wynik
    // rzeczywistej wysylki Telegrama, bez blokowania startu obslugi WWW.

#if DIAG_NET_TEST
    // DIAG dwa testy ZWYKLEGO (bez TLS) polaczenia po porcie 80, z licznikami
    // bajtow i stanem connected() - zeby zobaczyc CO SIE DZIEJE po udanym connect().
    // Usun caly blok #if DIAG_NET_TEST po diagnozie.
    {
      // TEST A: polaczenie PO SAMYM IP (Cloudflare 1.1.1.1) - bez DNS, eliminuje
      // DNS jako zmienna. Jesli to rowniez "connect=OK ale 0 bajtow", problem
      // jest gdzies za samym handshake (firewall/middlebox/NAT), nie w DNS.
      Serial.println(F("[DIAG-A] TCP:80 -> 1.1.1.1 (po IP, bez DNS) ..."));
      EthernetClient diagA;
      unsigned long dtA0 = millis();
      bool okA = diagA.connect(IPAddress(1, 1, 1, 1), 80);
      Serial.print(F("[DIAG-A] connect() = ")); Serial.print(okA ? F("OK") : F("FAIL"));
      Serial.print(F(" (")); Serial.print(millis() - dtA0); Serial.println(F(" ms)"));
      if (okA) {
        size_t wroteA = diagA.print(F("GET / HTTP/1.0\r\nHost: 1.1.1.1\r\nConnection: close\r\n\r\n"));
        Serial.print(F("[DIAG-A] wyslano bajtow zadania: ")); Serial.println((unsigned)wroteA);
        unsigned long dtA1 = millis();
        unsigned long gotBytesA = 0;
        while (millis() - dtA1 < 4000) {
          if (diagA.available()) { gotBytesA++; diagA.read(); }
          else if (!diagA.connected()) break;
          yield();
        }
        Serial.print(F("[DIAG-A] connected() po tescie = ")); Serial.println(diagA.connected() ? F("tak") : F("nie"));
        Serial.print(F("[DIAG-A] odebrano bajtow: ")); Serial.println(gotBytesA);
      }
      diagA.stop();
      delay(200);

      // TEST B: polaczenie PO HOSTNAME (jak w ), teraz z tymi samymi licznikami.
      Serial.println(F("[DIAG-B] TCP:80 -> example.com (z DNS) ..."));
      EthernetClient diagB;
      unsigned long dtB0 = millis();
      bool okB = diagB.connect("example.com", 80);
      Serial.print(F("[DIAG-B] connect() = ")); Serial.print(okB ? F("OK") : F("FAIL"));
      Serial.print(F(" (")); Serial.print(millis() - dtB0); Serial.println(F(" ms)"));
      if (okB) {
        size_t wroteB = diagB.print(F("GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n"));
        Serial.print(F("[DIAG-B] wyslano bajtow zadania: ")); Serial.println((unsigned)wroteB);
        unsigned long dtB1 = millis();
        unsigned long gotBytesB = 0;
        while (millis() - dtB1 < 4000) {
          if (diagB.available()) { gotBytesB++; Serial.write(diagB.read()); }
          else if (!diagB.connected()) break;
          yield();
        }
        Serial.println();
        Serial.print(F("[DIAG-B] connected() po tescie = ")); Serial.println(diagB.connected() ? F("tak") : F("nie"));
        Serial.print(F("[DIAG-B] odebrano bajtow: ")); Serial.println(gotBytesB);
      }
      diagB.stop();
    }
#endif
  }
#endif


#if USE_TELEGRAM
  Serial.println(F("[TG] Telegram alerting enabled"));
  // NIE wysylamy synchronicznie w setup() (blokujacy TLS PRZED uzbrojeniem
  // WDT i startem serwera = jesli handshake zawisnie, mamy boot-loop i martwe www).
  // Wrzucamy do kolejki - poleci z loop() przez telegramQueueTick(), gdy WDT juz
  // czuwa, serwery Alpaca/HTTP juz odpowiadaja, a transport = WiFi (net_mode).
  {
    String startMsg = "Obserwatorium  - system uruchomiony | RESTART: ";
    startMsg += rrTxt;
#if USE_W5500
    if (g_ethReady) { startMsg += " | ETH IP: "; startMsg += Ethernet.localIP().toString(); }
#endif
#if USE_WIFI
    if (g_wifiReady) { startMsg += " | WiFi IP: "; startMsg += WiFi.localIP().toString(); }
#endif
    tgEnqueue(startMsg);
  }
#endif

  // ═══ start modulu powiadomien (SMS SIM900 + syrena) ═══
  notifyBegin();
  if(g_mountEstop)notifyEvent(NOTIFY_CRITICAL,"MONTAZ","Zatrzask ochrony montazu aktywny po starcie. Przywrocenie wymaga inspekcji i hasla; sprawdz stan wyjscia odciecia.");
  if(rr==ESP_RST_TASK_WDT || rr==ESP_RST_INT_WDT || rr==ESP_RST_PANIC) {
    String text=String("Restart: ")+rrTxt;
    if(breadcrumbValid(g_previousBreadcrumb))text+="; stage="+String(g_previousBreadcrumb.stage)+" endpoint="+String(g_previousBreadcrumb.endpoint)+" millis="+String(g_previousBreadcrumb.at);
    else text+="; breadcrumb niewazny lub niedostepny";
    notifyEvent(NOTIFY_CRITICAL,"RESTART",text.c_str());
  }
  // g_sendTelegram zostaje nullptr: Telegram obsluguje Twoj istniejacy telegramSend().
  // Modul dokłada tylko NIEZALEZNY kanal: SMS + syrena na zdarzenia CRITICAL.
  // ═══ /═══

#if USE_TASK_WDT
  #if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdtConfig = {};
  wdtConfig.timeout_ms = (uint32_t)WDT_TIMEOUT_SEC * 1000UL;
  wdtConfig.idle_core_mask = 0;
  #if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  wdtConfig.idle_core_mask |= (1U << 0);
  #endif
  #if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  wdtConfig.idle_core_mask |= (1U << 1);
  #endif
  wdtConfig.trigger_panic = true;
  esp_task_wdt_init(&wdtConfig);
  #else
  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
  #endif
  esp_task_wdt_add(NULL);
  Serial.printf("Task WDT ON (%ds)\n", WDT_TIMEOUT_SEC);
#endif

  Serial.println(F("\nGotowe! URL:"));
#if USE_WIFI
  if (g_wifiReady) {
    Serial.print(F("  http://")); Serial.print(WiFi.localIP()); Serial.print(':'); Serial.print(SERVER_PORT); Serial.println('/');
  }
#endif
}

template<class Socket> struct HttpSession {
  AstroHttpClient<Socket> client;
  astro_http::Request request;
  bool used=false;
  bool dispatching=false;
  uint32_t acceptedStopEpoch=0;
  void start(Socket incoming) {
    acceptedStopEpoch=g_stopEpoch;
    client.attach(incoming); request.reset(millis()); used=true;
  }
  void close() { client.stop(); used=false; }
  void receive() {
    if(!used || request.complete()) return;
    if(request.expired(millis())) { close(); return; }
    size_t bytes=0;
    while(bytes++<astro_http::kInputBudget && client.available() && !request.complete() && !request.failed()) {
      const int value=client.read(); if(value<0) break;
      request.feed(char(value),millis());
    }
    if(request.failed() || (!client.connected() && !client.available() && !request.complete())) { close(); return; }
    if(request.expectContinue && request.phase==2 && !request.continueSent) {
      client.print(F("HTTP/1.1 100 Continue\r\n\r\n")); request.continueSent=true;
    }
  }
  bool urgentRequest() const {
    const String line(request.request);
    const int first=line.indexOf(' '), second=first>=0 ? line.indexOf(' ',first+1) : -1;
    if(first<0 || second<0) return false;
    const String method=line.substring(0,first);
    const String target=stripAbsoluteUrlToPath(line.substring(first+1,second));
    const int query=target.indexOf('?');
    const String path=toLowerCopy(query>=0 ? target.substring(0,query) : target);
    if(method=="GET" && path.startsWith("/api/v1/safetymonitor/0/")) return true;
    if(method=="PUT" && path=="/api/v1/dome/0/abortslew") return true;
    if(method!="POST" || path!="/cmd") return false;
    String params=query>=0 ? target.substring(query+1) : "";
    if(request.bodyLength) { if(params.length()) params+='&'; params+=request.body; }
    return toLowerCopy(getParamValue(params,"do"))=="stop";
  }
  void dispatch(bool urgentOnly) {
    if(!used || dispatching || !request.complete() || (urgentOnly && !urgentRequest())) return;
    // Never execute an older queued movement after servicing a priority STOP.
    if(!urgentRequest() && (acceptedStopEpoch!=g_stopEpoch || request.expired(millis()))) { close(); return; }
    dispatching=true;
    handleClientImpl(client,request,client.remoteIP());
    close();
    dispatching=false;
  }
};
#if USE_WIFI
static HttpSession<WiFiClient> g_wifiHttp[2];
#endif
#if USE_W5500
static HttpSession<EthernetClient> g_ethHttp[2];
#endif
static void httpReceiveTick() {
#if USE_WIFI
  if(g_wifiReady) {
    for(auto& session:g_wifiHttp) if(!session.used) {
      diagnosticStage(201); WiFiClient incoming=wifiServer.accept(); if(incoming) session.start(incoming); break;
    }
    for(auto& session:g_wifiHttp) { diagnosticStage(202); session.receive(); }
  }
#endif
#if USE_W5500
  if(g_ethReady) {
    for(auto& session:g_ethHttp) if(!session.used) {
      diagnosticStage(203); EthernetClient incoming=ethServer.accept(); if(incoming) session.start(incoming); break;
    }
    for(auto& session:g_ethHttp) { diagnosticStage(204); session.receive(); }
  }
#endif
}
static void httpDispatchTick() {
  // Receive all interfaces first; STOP/SafetyMonitor precede ordinary requests.
  for(int pass=0;pass<2;++pass) {
#if USE_WIFI
    for(auto& session:g_wifiHttp) { session.dispatch(pass==0); serviceRoofFast(); }
#endif
#if USE_W5500
    for(auto& session:g_ethHttp) { session.dispatch(pass==0); serviceRoofFast(); }
#endif
  }
}
static void httpUrgentTick() {
  static bool busy=false;
  if(busy) return; // never recursively dispatch a response to itself
  busy=true;
  httpReceiveTick();
#if USE_WIFI
  for(auto& session:g_wifiHttp) session.dispatch(true);
#endif
#if USE_W5500
  for(auto& session:g_ethHttp) session.dispatch(true);
#endif
  busy=false;
}

// ============== Loop ==============
void loop() {
  diagnosticStage(10);
  manualModeTick();
  serviceRoofFast();
  diagnosticStage(20);
  httpReceiveTick();
  diagnosticStage(21);
  httpDispatchTick();
  serviceRoofFast();
  if(ANEMOMETER_ENABLED) {
    static String previousWindStatus;
    const String currentWindStatus=windStatusText();
    if(previousWindStatus!=currentWindStatus) {
      eventLogEx(windMeasurementUsable()?"INFO":"ERROR","WIND","Status %s",currentWindStatus.c_str());
      static bool windStatusAlertSent=false;static uint32_t windStatusAlertAt=0;
      if(g_weatherPolicy.wind && previousWindStatus=="OK" && !windMeasurementUsable() &&
         (!windStatusAlertSent || uint32_t(millis()-windStatusAlertAt)>=900000UL)) {
        notifyEvent(NOTIFY_CRITICAL,"ANEMOMETR",currentWindStatus.c_str());
        windStatusAlertSent=true;windStatusAlertAt=millis();
      }
      previousWindStatus=currentWindStatus;
    }
  }
  if(g_weatherPolicy.wind && ANEMOMETER_ENABLED && !windMeasurementUsable()) {
    g_unsafe=true;
    g_reason=String("ANEMOMETR: ")+windStatusText();
  }
#if USE_MQTT
  // Do not start a blocking network connection while the roof is moving.
  if (roofState != RoofState::OPENING && roofState != RoofState::CLOSING) mqttTick();
#endif
  if (RTC_ENABLED) rtcTick();
#if USE_TASK_WDT
  esp_task_wdt_reset();
#endif

#if USE_OTA && USE_WIFI
  // nie przyjmuj NOWYCH zadan OTA gdy dach jedzie albo E-STOP montazu.
  // (Push wtedy nie polaczy sie i poczeka, az dom bedzie bezpieczny.)
  if (g_otaReady) {
    if (g_otaActive) {
      ArduinoOTA.handle();   // wgrywanie juz trwa - dokoncz
    } else {
      bool roofMoving = (roofState == RoofState::OPENING || roofState == RoofState::CLOSING);
      if (!roofMoving && !g_mountEstop) ArduinoOTA.handle();
    }
  }
#endif

  // zywy stan linku (wykrywa wypiety kabel W5500 / utracone WiFi) + alarm SMS
  diagnosticStage(22);
  linkStatusTick();
  // watchdog laczności z NINA (brak Alpaca przy otwartym dachu -> SMS/syrena)
  commsWatchdogTick(millis());
  // lagodny re-init nasluchu przy krotszej ciszy Alpaca (ratunek przed alarmem)
  commsSoftReinitTick(millis());

  // ═══ obsluga powiadomien (wysylka SMS, syrena) + status lacza ═══
  diagnosticStage(23);
  notifyTick();
  notifySetLinkUp(g_netLinkUp);   // realny stan linku zamiast stalych g_*Ready
  // ═══ /═══

  // Emergency close: ta sama niezalezna sciezka CLOSE co w masterze.
  {
    static bool lastBtn = HIGH, handled = false;
    static unsigned long debounceMs = 0;
    bool btnNow = digitalRead(EMERGENCY_CLOSE_PIN);
    if (btnNow != lastBtn) { debounceMs = millis(); lastBtn = btnNow; }
    if ((millis() - debounceMs) > 50) {
      if (lastBtn == LOW && !handled) {
        handled = true;
        Serial.println(F("!!! EMERGENCY CLOSE !!!"));
        // CLOSE nie wymaga NINA; wymaga przeswitu lub PARK i fizycznych blokad.
        // Pozostaja blokady fizyczne/interlockowe obslugiwane przez roofClose().
        if (modbusRelayOK) roofClose();
        // Emergency CLOSE does not silently surrender MANUAL ownership.
#if USE_TELEGRAM
        telegramSend("EMERGENCY CLOSE: zadanie zamkniecia; przeswit lub PARK oraz fizyczne blokady nadal obowiazuja");
#endif
      }
      if (lastBtn == HIGH) handled = false;
    }
  }
  sampleHardwarePark();
  diagnosticStage(24);
  updateParkStatus();    // Alpaca status; hardware park sampled every loop.
  diagnosticStage(25);
  readSensorsAndSafety();
  serviceRoofFast();
  diagnosticStage(26);
  tslTick();
  diagnosticStage(27);
  heaterControlTick();
  collisionSensorTick();
  mountEstopTick();      // szybka reakcja krancowki overtravel montazu
  roofTick();
  updatePulseOutputs();
  illumTick();             // czerwone swiatlo: brak auto-off; OFF wymuszany przy ruchu/STOP dachu
  if(!astro_roof::moving(roofState)) modbusRelayPollTick();   // FC01 readback okresowo, nigdy w kazdej iteracji loop()


#if USE_TELEGRAM
  telegramQueueTick();   // dosylanie odlozonych wiadomosci (kolejka TG)
#endif

  // WiFi reconnect
#if USE_WIFI
  // WiFi laczy sie asynchronicznie. Jesli poczatkowa proba sie nie powiedzie,
  // g_wifiReady pozostaje FALSE, ale ponawiamy laczenie okresowo.
  // Dawniej poczatkowe polaczenie (15s timeout w setup()) moglo opoznic start.
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - g_wifiLastConnectAttemptMs > 30000UL) {
      g_wifiLastConnectAttemptMs = now;
      Serial.println(F("??  WiFi nie polaczone - probuje (re)polaczyc..."));
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID_C, WIFI_PASS_C);
    }
  } else if (!g_wifiReady) {
    // Polaczylo sie dopiero teraz (po nieudanym starcie w setup()) - dokoncz
    // to, co setup() by zrobil: uruchom serwer WWW/Alpaca po WiFi.
    g_wifiReady = true;
    Serial.print(F("WiFi polaczone (odzyskane po nieudanym starcie): "));
    Serial.println(WiFi.localIP());
    wifiServer.begin();
    otaBeginIfConfigured();
  }
#endif
  diagnosticStage(20);
  httpReceiveTick();
  diagnosticStage(21);
  httpDispatchTick();
  serviceRoofFast();
  // Publish progress ONLY after this full control-loop iteration completed.
  const bool healthOK=modbusRelayOK && roofState!=RoofState::ERROR && !g_mountEstop &&
    policyWindReady() && policySkyReady() && !policyMoistureBlocked() && !g_weatherPolicyFault;
  onlineLoopProgress(lastSensorsRead,healthOK);
}




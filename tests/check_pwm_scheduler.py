"""Compile-time checks for the production PWM scheduler with injected Modbus failures."""
from pathlib import Path
import subprocess
import sys

from check_netfix import block

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
body = block(main, "heaterControlTick")
body = body.replace("static uint8_t nextChannel = 0;", "")
body = body.replace("static uint32_t s_ledLastTxMs = 0;", "")
source = r"""
#include <stdint.h>
#define PWM_MODULE_ENABLED 1
#define HEATER_PWM_ENABLED 1
#define LED_PWM_ENABLED 1
#define HEATER_PWM_ADDR 2
#define HEATER_PWM_MAX_SENSOR_AGE_MS 10000UL
#define HEATER_PWM_MARGIN_ZERO_C 4.0f
#define HEATER_PWM_MARGIN_FULL_C 1.0f
#define HEATER_MANUAL_TIMEOUT_MIN 120
#define NAN __builtin_nanf("")
struct Harness {
  uint32_t now=100, lastUpdate_Temperature=1, lastUpdate_Humidity=1, lastUpdate_DewPoint=1;
  static constexpr int BARO_BME280=1;
  int g_baroType=BARO_BME280;
  float g_tAir=12, g_hAir=90, g_dewPoint=10, g_heaterDewMarginC=0;
  bool bme280OK=true, g_heaterSensorValid=false;
  bool g_heaterPwmOnline=true, g_heaterChannelEnabled[3]={true,true,true};
  bool g_heaterManual[3]={true,false,false};
  uint32_t g_heaterManualSinceMs[3]={1,1,1}, g_heaterLastTxMs[3]={};
  float g_heaterManualPct[3]={20}, g_heaterDutyPct[3]={};
  bool g_ledPwmOn=true, g_pwmOffConfirmed=false;
  float g_ledDutyPct=50, g_ledBrightPct=50;
  uint8_t nextChannel=0, g_pwmOffChannel=0, failedChannel=0;
  uint32_t s_ledLastTxMs=0, g_heaterLastProbeMs=0;
  unsigned writes=0;
  uint8_t lastChannel=0;
  float lastDuty=0;
  constexpr uint32_t millis() const { return now; }
  constexpr bool isfinite(float v) const { return __builtin_isfinite(v); }
  constexpr float fabsf(float v) const { return v<0 ? -v : v; }
  constexpr float constrain(float v,float lo,float hi) const { return v<lo?lo:v>hi?hi:v; }
  template<class... Args> constexpr void eventLogEx(Args...) {}
  constexpr bool heaterPwmWriteChannel(uint8_t ch,float duty) {
    ++writes; lastChannel=ch; lastDuty=duty;
    if (ch==failedChannel && g_heaterPwmOnline) g_heaterLastProbeMs=0;
    return ch!=failedChannel;
  }
  constexpr bool ledPwmSet(float duty) { return heaterPwmWriteChannel(4,duty); }
  constexpr void tick() BODY
};
constexpr bool recoverEveryChannel() {
  Harness h;
  h.failedChannel=1;
  h.tick();
  if (h.writes!=1 || h.g_heaterPwmOnline || h.g_heaterManual[0] || h.g_ledPwmOn) return false;
  for (uint8_t ch=1; ch<=4; ++ch) {
    h.tick();
    if (h.lastChannel!=ch || h.lastDuty!=0 || h.writes!=1U+ch || h.g_heaterPwmOnline) return false;
  }
  h.failedChannel=0;
  h.now+=9999;
  h.tick();
  if (h.writes!=5 || h.g_heaterPwmOnline) return false;
  ++h.now;
  for (uint8_t ch=1; ch<=4; ++ch) {
    h.tick();
    if (h.lastChannel!=ch || h.lastDuty!=0 || h.writes!=5U+ch || h.g_heaterPwmOnline!=(ch==4)) return false;
  }
  return true;
}
constexpr bool normalRoundRobin() {
  Harness h;
  for (uint8_t ch=1; ch<=3; ++ch) {
    h.tick();
    if (h.writes!=ch || h.lastChannel!=ch || h.lastDuty<=0 || !h.g_heaterPwmOnline) return false;
  }
  h.tick();
  return h.writes==3;
}
static_assert(recoverEveryChannel(),"Failed PWM channels cannot skip the other OFF writes or restore heating prematurely");
static_assert(normalRoundRobin(),"One PWM write per tick with individual channel refresh");
""".replace("BODY", body)

out = root / ".pio/offline/pwm_scheduler.cpp"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(source, encoding="utf-8")
subprocess.run([sys.argv[1], "-std=gnu++17", "-fsyntax-only", str(out)], check=True)
print("PASS PWM round-robin, OFF attempts for every output and restart only after all acknowledgements")


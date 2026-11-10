"""Runtime check of the PWM write retry policy extracted from src/hardware_io.h."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
hw = (root / "src/hardware_io.h").read_text(encoding="utf-8").replace("\r\n", "\n")
start = hw.index("static uint32_t g_pwmRetryRecovered")
end = hw.index("static bool heaterPwmAllOff")
func = hw[start:end]

source = r'''
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <array>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#define PWM_MODULE_ENABLED 1
#define HEATER_PWM_ADDR 2
#define HEATER_PWM_FREQ_CENTIHZ 1000
static bool g_heaterPwmOnline = true;
static unsigned long g_heaterLastProbeMs = 5;
static uint32_t fakeNow = 1000000;
static unsigned delays = 0, calls = 0, failFirst = 0, logs = 0;
static std::string lastLog;
static uint32_t millis() { return fakeNow; }
static void delay(unsigned) { ++delays; }
template<class T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint16_t modbusCRC16(const uint8_t*, size_t) { return 0; }
namespace astro_modbus { static std::array<uint8_t,8> pwmFrame(uint8_t,uint8_t,uint16_t,uint16_t) { return {}; } }
static bool modbusRelayTransaction(const uint8_t*, size_t, uint8_t* resp, size_t, size_t* n) {
  ++calls;
  if (calls <= failFirst) { *n = 0; return false; }
  *n = 8; for (int i = 0; i < 8; ++i) resp[i] = 0; return true;
}
static void eventLogEx(const char*, const char*, const char* fmt, ...) {
  char buf[256]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  ++logs; lastLog = buf;
}
FUNC
static void reset(bool online, unsigned fail) {
  g_heaterPwmOnline = online; g_heaterLastProbeMs = 5; delays = calls = logs = 0; failFirst = fail; lastLog.clear();
}
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
  // 1) online: 2 failures then success -> ok after 3 attempts, no failure state
  reset(true, 2);
  CHECK(heaterPwmWriteChannel(1, 20.0f));
  CHECK(calls == 3 && delays == 2 && g_heaterPwmOnline && g_heaterLastProbeMs == 5);
  CHECK(g_pwmRetryRecovered == 1 && logs == 1 && lastLog.find("probie 3") != std::string::npos);
  // 2) online: permanent failure -> exactly 3 attempts, OFF sweep requested, one detailed WARN
  reset(true, 99);
  CHECK(!heaterPwmWriteChannel(2, 20.0f));
  CHECK(calls == 3 && delays == 2 && g_heaterLastProbeMs == 0 && logs == 1);
  CHECK(lastLog.find("no reply") != std::string::npos && lastLog.find("proby=3") != std::string::npos);
  // 3) offline (OFF sweep): single attempt, no sleeping, no log spam
  reset(false, 99);
  CHECK(!heaterPwmWriteChannel(3, 0.0f));
  CHECK(calls == 1 && delays == 0 && logs == 0);
  // 4) healthy path: one transaction, no retry bookkeeping
  reset(true, 0);
  CHECK(heaterPwmWriteChannel(1, 50.0f));
  CHECK(calls == 1 && delays == 0 && logs == 0);
  puts("OK");
  return 0;
}
'''.replace("FUNC", func)

out = root / ".pio/offline/pwm_retry.cpp"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(source, encoding="utf-8")
exe = out.with_suffix(".bin")
subprocess.run([sys.argv[1], "-std=gnu++17", "-Wall", str(out), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print("PASS PWM retry: 3 attempts online, 1 attempt offline, detailed failure log")

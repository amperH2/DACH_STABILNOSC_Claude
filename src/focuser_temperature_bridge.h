#pragma once
// Read-only sensor telemetry. It has no access to relay/motor/roof commands.
struct FocuserTemperatureProbe {
  float value = NAN;
  uint32_t at = 0;
  bool present = false;
  bool valid = false;
  void sample(float temperature, uint32_t now, float low, float high) {
    valid = isfinite(temperature) && temperature >= low && temperature <= high;
    if (valid) { value = temperature; at = now; present = true; }
  }
  String json(bool enabled, uint32_t now) const {
    uint32_t age = uint32_t(now - at);
    bool usable = enabled && present && valid && age <= 15000;
    String out = "{\"enabled\":"; out += enabled ? "true" : "false";
    out += ",\"valid\":"; out += usable ? "true" : "false";
    out += ",\"temperature_c\":"; out += usable ? String(value,2) : String("null");
    out += ",\"age_s\":"; out += present ? String(age / 1000.0f,3) : String("null");
    out += "}"; return out;
  }
};
static FocuserTemperatureProbe focusBmeTemperature, focusDhtTemperature, focusDsTemperature;
static String focuserTemperaturesJson() {
  uint32_t now = millis();
  String out = "{\"schema\":1,\"uptime_ms\":" + String(now) + ",\"sensors\":{";
  out += "\"bme280\":" + focusBmeTemperature.json(ENABLE_BME280,now);
  out += ",\"dht22\":" + focusDhtTemperature.json(ENABLE_DHT22,now);
  out += ",\"ds18b20\":" + focusDsTemperature.json(ENABLE_DS18B20,now);
  out += "}}"; return out;
}

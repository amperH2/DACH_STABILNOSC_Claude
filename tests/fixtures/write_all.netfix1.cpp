static void writeAll(Client &c, const uint8_t* data, size_t len) {
  const size_t CHUNK = 256;
  size_t pos = 0;
  uint8_t tmp[CHUNK];
  const uint32_t start = millis();
  while (pos < len) {
    if (!c.connected()) break;
    size_t n = (len - pos > CHUNK) ? CHUNK : (len - pos);
    memcpy(tmp, data + pos, n);
    size_t w = c.write(tmp, n);
    if (w > 0) pos += w; else delay(1);
    serviceRoofFast();
    httpUrgentTick(); // STOP/SafetyMonitor can use the other session during a response
    yield();
    if (millis() - start > 1000) { c.stop(); break; }
  }
}

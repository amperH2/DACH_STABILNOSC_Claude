static void writeAll(Client &c, const uint8_t* data, size_t len) {
  const size_t CHUNK = 1024;
  size_t pos = 0;
  uint8_t tmp[CHUNK];
  const uint32_t start = millis();
  uint32_t lastProgress = start;
  while (pos < len) {
    if (!c.connected()) break;
    size_t n = (len - pos > CHUNK) ? CHUNK : (len - pos);
    memcpy(tmp, data + pos, n);
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

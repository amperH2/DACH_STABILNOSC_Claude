// ============== MQTT ==============
#if USE_MQTT
static void mqttReconnect() {
  // ═══ v15.1: MQTT z automatycznym wyborem transportu (ETH<->WiFi), health-aware ═══
  NetIf mqDesired = netChoose();
  if (mqDesired == NET_NONE) return;            // brak jakiegokolwiek lacza
  static bool  mqttSslReady = false;
  static NetIf mqttBoundIf  = NET_NONE;
  if (!mqttSslReady || mqttBoundIf != mqDesired) {
    // (prze)wiazanie transportu - takze gdy zdrowy interfejs sie zmienil
    if (mqttSslReady) { mqttClient.disconnect(); mqttSSL.stop(); g_mqttConnected = false; }
#if USE_W5500
    if (mqDesired == NET_ETH) mqttSSL.setClient(&mqttEthBase);
    else                      mqttSSL.setClient(&mqttWifiClient);
#else
    mqttSSL.setClient(&mqttWifiClient);
#endif
    mqttClient.setClient(MQTT_TLS_ENABLED ? (Client&)mqttSSL :
#if USE_W5500
      (mqDesired == NET_ETH ? (Client&)mqttEthBase : (Client&)mqttWifiClient)
#else
      (Client&)mqttWifiClient
#endif
    );
    mqttSSL.setInsecure();              // bez weryfikacji certyfikatu
    mqttSSL.setBufferSizes(16384, 2048);
    mqttSSL.setTimeout(4);           // v15.1: skrocony - szybkie fail-fast na martwym ETH
    mqttSSL.setHandshakeTimeout(4);     // v15.1: skrocony (bylo 8s)
#if DIAG_TLS_DEBUG
    mqttSSL.setDebugLevel(4);           // DIAG: szczegolowy log BearSSL na Serial - do usuniecia po diagnozie
#endif
    mqttBoundIf  = mqDesired;
    mqttSslReady = true;
    Serial.print(F("[MQTT] Transport: ")); Serial.println(netName(mqDesired));
  }
  // ═══ /v15.1 ═══
  unsigned long now = millis();
  if (g_mqttLastReconnect != 0 && now - g_mqttLastReconnect < 30000UL) return;

  // v16: nie blokuj loop() gdy NINA odpytuje Alpaca (wspolny straznik ninaIsActive).
  // mqttReconnect() blokuje na TLS do ~8s - NINA mogłaby stracic SafetyMonitor.
  if (ninaIsActive()) {
    Serial.println(F("[MQTT] Odlozono reconnect (NINA aktywna)"));
    return;
  }
  g_mqttLastReconnect = now;

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(1024);
  mqttClient.setSocketTimeout(1);      // ASTRO v14: connect/loop nie blokuja petli > ~8s
  mqttClient.setKeepAlive(20);
  Serial.print(F("[MQTT] Laczenie z "));
  Serial.print(MQTT_HOST);
  Serial.print(':');
  Serial.println(MQTT_PORT);
#if USE_TASK_WDT
  esp_task_wdt_reset();                 // ASTRO v14: swiezy watchdog tuz przed blokujacym connect
#endif

  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
                            MQTT_TOPIC_ALERT, 1, false, "OFFLINE");
  } else {
    ok = mqttClient.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                            MQTT_TOPIC_ALERT, 1, false, "OFFLINE");
  }

  netReport(mqttBoundIf, ok);          // v15.1: aktualizuj health interfejsu MQTT

  if (ok) {
    g_mqttConnected = true;
    mqttClient.publish(MQTT_TOPIC_ALERT, "ONLINE", false);
    Serial.println(F("[MQTT] Polaczono!"));
  } else {
    g_mqttConnected = false;
    Serial.printf("[MQTT] Blad: %d\n", mqttClient.state());
    // DIAG: jawne zamkniecie transportu po nieudanym connect - PubSubClient/ESP_SSLClient
    // czasem nie zwalniaja gniazda W5500 przy bledzie, co po wielu nieudanych
    // próbach (co 30s) moze wyczerpac 8 sprzetowych socketow W5500 i ubic web/Alpaca.
    mqttSSL.stop();
#if USE_W5500
    if (mqttBoundIf == NET_ETH) mqttEthBase.stop();
    else                        mqttWifiClient.stop();
#else
    mqttWifiClient.stop();
#endif
  }
}

static void mqttPublishAlert(const String& msg) {
  if (!g_mqttConnected) return;
  mqttClient.publish(MQTT_TOPIC_ALERT, msg.c_str(), false);
}

static void mqttPublishState() {
  if (!g_mqttConnected) return;
  // Simple values
  mqttClient.publish(MQTT_TOPIC_SAFE, (g_unsafe || (ANEMOMETER_ENABLED && !windMeasurementUsable())) ? "0" : "1", true);
  mqttClient.publish(MQTT_TOPIC_RAIN, g_rain   ? "1" : "0", true);
  mqttClient.publish(MQTT_TOPIC_ROOF, roofStateName(roofState), true);

  char windBuf[12];
  if(windMeasurementUsable()) snprintf(windBuf, sizeof(windBuf), "%.1f", g_wind_ms);
  else strcpy(windBuf,"null");
  mqttClient.publish(MQTT_TOPIC_WIND, windBuf, true);

  // Full state JSON (trochę ograniczony zeby zmiescic w MQTT buffer)
  char stateBuf[512];
  snprintf(stateBuf, sizeof(stateBuf),
    "{\"safe\":%s,\"rain\":%s,\"wind\":%s,\"delta\":%.1f,\"roof\":\"%s\",\"parked\":%s}",
    (g_unsafe || (ANEMOMETER_ENABLED && !windMeasurementUsable())) ? "false" : "true",
    g_rain   ? "true"  : "false",
    windBuf,
    isnan(g_cloudDelta) ? -99.0f : g_cloudDelta,
    roofStateName(roofState),
    g_telescopeParked ? "true" : "false"
  );
  mqttClient.publish(MQTT_TOPIC_STATE, stateBuf, true);
}

static String pendingMqttAlert;
static void mqttQueueAlert(const String& text) { pendingMqttAlert=text.substring(0,240); }
static void mqttTick() {
  if (!netEthLinkUp() && !netWifiLinkUp()) return;
  if (!mqttClient.connected()) {
    g_mqttConnected = false;
    // v15.1 FIX: throttle - bez tego mqttReconnect() (blokujacy TLS) byl wywolywany
    // na KAZDEJ iteracji loop() gdy MQTT nie polaczony -> 8-16s blokowania -> NINA gubi Safety Monitor
    static unsigned long lastMqttAttempt = 0;
    unsigned long now2 = millis();
    if (lastMqttAttempt == 0 || now2 - lastMqttAttempt >= 30000UL) {
      lastMqttAttempt = now2;
      mqttReconnect();
    }
  } else {
    mqttClient.loop();
    if (pendingMqttAlert.length()) { mqttPublishAlert(pendingMqttAlert); pendingMqttAlert=""; }
    unsigned long now = millis();
    if (now - g_mqttLastPublish > MQTT_PUBLISH_INTERVAL_MS) {
      g_mqttLastPublish = now;
      mqttPublishState();
    }
  }
}
#endif


#if !USE_MQTT
static void mqttQueueAlert(const String&) {}
#endif

#ifndef USE_MQTT
  #define USE_MQTT 0
#endif
#if USE_MQTT
  #include <PubSubClient.h>
  #include <WiFi.h>
  #include <ESP_SSLClient.h>   // ASTRO v14: TLS dla MQTT (HiveMQ Cloud :8883)
  #ifndef MQTT_HOST
    #define MQTT_HOST "192.168.1.126"
  #endif
  #ifndef MQTT_PORT
    #define MQTT_PORT 1883
  #endif
  #ifndef MQTT_USER
    #define MQTT_USER ""
  #endif
  #ifndef MQTT_PASS
    #define MQTT_PASS ""
  #endif
  #ifndef MQTT_CLIENT_ID
    #define MQTT_CLIENT_ID "obserwatorium"
  #endif
  // Tematy MQTT (mozna nadpisac w platformio.ini)
  #ifndef MQTT_TOPIC_STATE
    #define MQTT_TOPIC_STATE  "obserwatorium/state"
  #endif
  #ifndef MQTT_TOPIC_SAFE
    #define MQTT_TOPIC_SAFE   "obserwatorium/safe"
  #endif
  #ifndef MQTT_TOPIC_RAIN
    #define MQTT_TOPIC_RAIN   "obserwatorium/rain"
  #endif
  #ifndef MQTT_TOPIC_WIND
    #define MQTT_TOPIC_WIND   "obserwatorium/wind"
  #endif
  #ifndef MQTT_TOPIC_ROOF
    #define MQTT_TOPIC_ROOF   "obserwatorium/roof"
  #endif
  #ifndef MQTT_TOPIC_ALERT
    #define MQTT_TOPIC_ALERT  "obserwatorium/alert"
  #endif

  // ═══ ASTRO v14: MQTT po TLS (HiveMQ Cloud :8883) przez Ethernet ═══
  WiFiClient mqttWifiClient;          // zapas transportu (WiFi)
#if USE_W5500
  EthernetClient mqttEthBase;         // transport bazowy po kablu
#endif
  ESP_SSLClient mqttSSL;              // TLS owijajacy transport
  PubSubClient mqttClient(mqttSSL);   // PubSubClient gada przez TLS
  // ═══ /ASTRO v14 ═══
  bool g_mqttConnected = false;
  unsigned long g_mqttLastReconnect = 0;
  unsigned long g_mqttLastPublish   = 0;
  const unsigned long MQTT_PUBLISH_INTERVAL_MS = 10000UL; // co 10s

  static void mqttReconnect();
  static void mqttPublishState();
  static void mqttPublishAlert(const String& msg);
#endif


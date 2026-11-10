#pragma once
// Included once after the existing transport helpers; all network IO is worker-only.
#include <ArduinoJson.h>
#include <ESP_SSLClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include "notification_policy.h"
#include "notify_ca.h"
#ifndef PUSHOVER_APP_TOKEN
#define PUSHOVER_APP_TOKEN ""
#endif
#ifndef PUSHOVER_USER_KEY
#define PUSHOVER_USER_KEY ""
#endif
#ifndef HEALTHCHECKS_UUID
#define HEALTHCHECKS_UUID ""
#endif
#ifndef TELEGRAM_TOKEN
#define TELEGRAM_TOKEN ""
#endif
#ifndef TELEGRAM_CHAT_ID
#define TELEGRAM_CHAT_ID ""
#endif
struct OnlineStatus {
 bool enabled=false,configured=false,compiled=false,inflight=false,hasOK=false,hasTry=false;
 uint32_t lastOK=0,lastTry=0,okCount=0,failed=0,dropped=0;int error=0;
};
struct OnlineJob {uint8_t channel=0,attempt=0;bool emergency=false;uint32_t created=0,due=0;char text[300 + sizeof(OBSERVATORY_PREFIX)]={};};
struct OnlineResult {uint8_t channel;int code;bool ok;};
static portMUX_TYPE g_onlineMux=portMUX_INITIALIZER_UNLOCKED;
static OnlineStatus g_onlineStatus[3];
static astro_notify::Health g_onlineHealth;
static QueueHandle_t g_onlineJobs=nullptr,g_onlineResults=nullptr;
static uint32_t g_onlineNextHeartbeat=0,g_onlinePushBlockedUntil=0;
static bool onlineCompiled(unsigned c){return c==0?USE_TELEGRAM:c==1?PUSHOVER_ENABLED:HEARTBEAT_ENABLED;}
static bool onlineToken(const char* t){if(strlen(t)!=30)return false;for(size_t i=0;i<30;++i)if(!isalnum((unsigned char)t[i]))return false;return true;}
static bool onlineUuid(){
 const char* u=HEALTHCHECKS_UUID;if(strlen(u)!=36)return false;
 for(unsigned i=0;i<36;++i){if(i==8||i==13||i==18||i==23){if(u[i]!='-')return false;}else if(!isxdigit((unsigned char)u[i]))return false;}return true;
}
static bool onlineEnabled(unsigned c){bool on;portENTER_CRITICAL(&g_onlineMux);on=c<3&&g_onlineStatus[c].enabled;portEXIT_CRITICAL(&g_onlineMux);return on;}
static bool onlineTelegramEnabled(){return onlineEnabled(0);}
static astro_notify::Health onlineHealthSnapshot(){astro_notify::Health h;portENTER_CRITICAL(&g_onlineMux);h=g_onlineHealth;portEXIT_CRITICAL(&g_onlineMux);return h;}
static void onlineRecord(unsigned c,bool ok,int code){
 if(c>=3)return;portENTER_CRITICAL(&g_onlineMux);auto& s=g_onlineStatus[c];
 s.inflight=false;s.hasTry=true;s.lastTry=millis();s.error=ok?0:code;
 if(ok){s.lastOK=millis();s.hasOK=true;++s.okCount;}else ++s.failed;
 portEXIT_CRITICAL(&g_onlineMux);
}
static void onlineTelegramStarted(){portENTER_CRITICAL(&g_onlineMux);g_onlineStatus[0].inflight=true;g_onlineStatus[0].hasTry=true;g_onlineStatus[0].lastTry=millis();portEXIT_CRITICAL(&g_onlineMux);}
static void onlineTelegramResult(bool success,int code){onlineRecord(0,success,code);}
static void onlineDrop(unsigned c){portENTER_CRITICAL(&g_onlineMux);++g_onlineStatus[c].dropped;g_onlineStatus[c].error=-7;portEXIT_CRITICAL(&g_onlineMux);}
static String onlineEncode(const String& text){
 String out;out.reserve(text.length()*2);
 for(unsigned i=0;i<text.length();++i){uint8_t c=text[i];if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=(char)c;else{char b[4];snprintf(b,sizeof(b),"%%%02X",c);out+=b;}}return out;
}
struct OnlineHttpReply {int code=0;String body;bool complete=false;};
// Read bytes under a total deadline, never readStringUntil() without a bound.
static OnlineHttpReply onlineRequest(const char* host,const String& path,const String& body,bool heartbeat){
 OnlineHttpReply out;if(time(nullptr)<1700000000){out.code=-2;return out;}
 if(heartbeat&&!onlineHealthSnapshot().usable(millis())){out.code=-3;return out;}
 WiFiClient wifi;
#if USE_W5500
 EthernetClient ethernet;ethernet.setConnectionTimeout(2000);
#endif
 ESP_SSLClient tls;
 tls.setDebugLevel(0);tls.setCACert(NOTIFY_CA_PEM);tls.setX509Time((uint32_t)time(nullptr));
 tls.setBufferSizes(16384,2048);tls.setTimeout(2);tls.setHandshakeTimeout(4);
 NetIf order[2]={netChoose(),NET_NONE};order[1]=netOther(order[0]);bool connected=false;
 for(unsigned i=0;i<2&&!connected;++i){
  if(order[i]==NET_NONE)continue;
#if USE_W5500
  if(order[i]==NET_ETH)tls.setClient(&ethernet);else tls.setClient(&wifi);
#else
  tls.setClient(&wifi);
#endif
  connected=tls.connect(host,443);if(!connected)tls.stop();
 }
 if(!connected){out.code=-1;return out;}
 // A delayed TLS connection may outlive the main loop's health lease.
 if(heartbeat&&!onlineHealthSnapshot().usable(millis())){tls.stop();out.code=-3;return out;}
 if(!onlineEnabled(heartbeat?2:1)){tls.stop();out.code=-8;return out;}
 String request=String("POST ")+path+" HTTP/1.0\r\nHost: "+host+
  "\r\nContent-Type: application/x-www-form-urlencoded\r\nConnection: close\r\nContent-Length: "+body.length()+"\r\n\r\n"+body;
 if(tls.print(request)!=request.length()){tls.stop();out.code=-1;return out;}
 const uint32_t started=millis();String raw;raw.reserve(2300);
 while(uint32_t(millis()-started)<5000 && (tls.connected()||tls.available())){
  unsigned budget=256;while(budget--&&tls.available()&&raw.length()<4096)raw+=(char)tls.read();
  if(raw.length()>=4096){out.code=-4;break;}vTaskDelay(pdMS_TO_TICKS(5));
 }
 const bool ended=!tls.connected()&&!tls.available();tls.stop();
 if(out.code<0)return out;
 const int split=raw.indexOf("\r\n\r\n");
 if(!raw.startsWith("HTTP/1.")||split<0){out.code=-4;return out;}
 out.code=raw.substring(9,12).toInt();out.body=raw.substring(split+4);
 String headers=raw.substring(0,split);headers.toLowerCase();
 const int lenAt=headers.indexOf("content-length:");
 if(lenAt>=0){const unsigned length=raw.substring(lenAt+15).toInt();out.complete=out.body.length()==length;}
 else out.complete=ended;
 // Unknown/chunked responses must not be mistaken for provider acceptance.
 if(headers.indexOf("chunked")>=0)out.complete=false;
 if(!out.complete)out.code=-4;
 return out;
}
static void onlineWorker(void*){
 OnlineJob pending[8];bool used[8]={};
 for(;;){
  OnlineJob incoming;
  while(xQueueReceive(g_onlineJobs,&incoming,0)==pdTRUE){
   // Reserve a pending slot for heartbeat so failed push retries cannot fill it.
   unsigned i=incoming.channel==ONLINE_HEARTBEAT?7:0;
   const unsigned limit=incoming.channel==ONLINE_HEARTBEAT?8:7;
   while(i<limit&&used[i])++i;
   if(i==limit)onlineDrop(incoming.channel);else{pending[i]=incoming;used[i]=true;}
  }
  int chosen=-1;
  for(unsigned i=0;i<8;++i)if(used[i]&&astro_notify::due(millis(),pending[i].due)){
   if(chosen<0||pending[i].channel==ONLINE_PUSHOVER)chosen=i;
  }
  if(chosen<0){vTaskDelay(pdMS_TO_TICKS(50));continue;}
  OnlineJob& j=pending[chosen];
  if(!onlineEnabled(j.channel)||uint32_t(millis()-j.created)>astro_notify::kJobTtlMs){used[chosen]=false;onlineRecord(j.channel,false,-8);continue;}
  if(g_onlinePushBlockedUntil && astro_notify::due(millis(),g_onlinePushBlockedUntil))g_onlinePushBlockedUntil=0;
  if(j.channel==ONLINE_PUSHOVER && g_onlinePushBlockedUntil){used[chosen]=false;onlineRecord(j.channel,false,429);continue;}
  if(xSemaphoreTake(g_notifyTransportMutex,pdMS_TO_TICKS(100))!=pdTRUE){vTaskDelay(pdMS_TO_TICKS(50));continue;}
  portENTER_CRITICAL(&g_onlineMux);g_onlineStatus[j.channel].inflight=true;portEXIT_CRITICAL(&g_onlineMux);
  OnlineHttpReply r;
  bool ok=false;
  if(j.channel==ONLINE_PUSHOVER){
   String body=String("token=")+onlineEncode(PUSHOVER_APP_TOKEN)+"&user="+onlineEncode(PUSHOVER_USER_KEY)+"&title="+onlineEncode(String(OBSERVATORY_PREFIX " ASTRO-ROOF SAFE"))+"&message="+onlineEncode(j.text)+"&priority="+(j.emergency?"2":"0");
   if(j.emergency)body+="&retry="+String(PUSHOVER_RETRY_SEC)+"&expire="+String(PUSHOVER_EXPIRE_SEC);
   r=onlineRequest("api.pushover.net","/1/messages.json",body,false);
   StaticJsonDocument<1400> doc;
   const bool jsonOK=!deserializeJson(doc,r.body);
   ok=r.code==200&&jsonOK&&doc["status"].as<int>()==1;
   // Do not expose body/errors or API tokens in any result/log.
   if(ok&&j.emergency){const char* receipt=doc["receipt"]|"";ok=strlen(receipt)==30;}
   if(r.code==200&&!ok)r.code=400;
   if(r.code==429)g_onlinePushBlockedUntil=millis()+3600000UL;
  }else{
   r=onlineRequest("hc-ping.com",String("/")+HEALTHCHECKS_UUID,"controller=healthy",true);
   r.body.trim();ok=r.code==200&&r.body=="OK";
   if(r.code==200&&!ok)r.code=400;
  }
  xSemaphoreGive(g_notifyTransportMutex);
  onlineRecord(j.channel,ok,r.code);
  OnlineResult result{j.channel,r.code,ok};xQueueSend(g_onlineResults,&result,0);
  ++j.attempt;
  if(!ok&&j.channel==ONLINE_PUSHOVER&&astro_notify::retryable(r.code)&&j.attempt<3){j.due=millis()+astro_notify::retryDelay(j.attempt);}
  else used[chosen]=false;
 }
}
static bool onlineEnqueue(uint8_t channel,const char* text,bool emergency){
 if(channel>2||!g_onlineJobs||!onlineEnabled(channel))return false;
 bool configured;portENTER_CRITICAL(&g_onlineMux);configured=g_onlineStatus[channel].configured;portEXIT_CRITICAL(&g_onlineMux);
 if(!configured){onlineRecord(channel,false,-5);return false;}
 OnlineJob j;j.channel=channel;j.emergency=emergency;j.created=millis();j.due=millis();if(channel==ONLINE_PUSHOVER) observatoryCopyMessage(j.text,sizeof(j.text),text); else strlcpy(j.text,text,sizeof(j.text));
 if(xQueueSend(g_onlineJobs,&j,0)!=pdTRUE){onlineDrop(channel);return false;}return true;
}
static void onlineCritical(const char* title,const char* message){
 static struct Seen{uint32_t hash=0,at=0;bool used=false;} seen[12];static unsigned head=0;
 uint32_t hash=2166136261u;for(const char* p=title;*p;++p)hash=(hash^(uint8_t)*p)*16777619u;
 for(const char* p=message;*p;++p)hash=(hash^(uint8_t)*p)*16777619u;
 for(auto& s:seen)if(s.used&&s.hash==hash&&uint32_t(millis()-s.at)<astro_notify::kDedupMs)return;
 String text=String(title)+": "+message;
 if(onlineEnqueue(1,text.c_str(),true)){seen[head]={hash,millis(),true};head=(head+1)%12;}
}
static bool onlineSetEnabled(uint8_t channel,bool enabled){
 if(channel>2||(!onlineCompiled(channel)&&enabled))return false;
 Preferences prefs;if(!prefs.begin("notify_ui",false))return false;
 const char* key=channel==0?"telegram":channel==1?"pushover":"heartbeat";
 const bool ok=prefs.putBool(key,enabled)>0;prefs.end();
 if(ok){portENTER_CRITICAL(&g_onlineMux);g_onlineStatus[channel].enabled=enabled;portEXIT_CRITICAL(&g_onlineMux);}return ok;
}
static bool onlineTest(uint8_t channel){
 if(channel==0){
#if USE_TELEGRAM
  if(!onlineTelegramEnabled())return false;telegramSend("ASTRO-ROOF SAFE: test Telegram",true);return true;
#else
  return false;
#endif
 }
 return onlineEnqueue(channel,"TEST: powiadomienia ASTRO-ROOF SAFE (bez sterowania sprzetem)",false);
}
static void onlineBegin(){
 g_notifyTransportMutex=xSemaphoreCreateMutex();
 Preferences prefs;const bool loaded=prefs.begin("notify_ui",true);
 const char* keys[]={"telegram","pushover","heartbeat"};
 for(unsigned i=0;i<3;++i){g_onlineStatus[i].compiled=onlineCompiled(i);g_onlineStatus[i].enabled=onlineCompiled(i)&&(loaded?prefs.getBool(keys[i],true):true);}
 if(loaded)prefs.end();
 g_onlineStatus[0].configured=strlen(TELEGRAM_TOKEN)>10 && String(TELEGRAM_TOKEN)!="YOUR_BOT_TOKEN" && strlen(TELEGRAM_CHAT_ID)>0;
 g_onlineStatus[1].configured=onlineToken(PUSHOVER_APP_TOKEN)&&onlineToken(PUSHOVER_USER_KEY);
 g_onlineStatus[2].configured=onlineUuid();
 if(!PUSHOVER_ENABLED&&!HEARTBEAT_ENABLED)return;
 g_onlineJobs=xQueueCreate(8,sizeof(OnlineJob));g_onlineResults=xQueueCreate(12,sizeof(OnlineResult));
 if(!g_notifyTransportMutex||!g_onlineJobs||!g_onlineResults || xTaskCreatePinnedToCore(onlineWorker,"notifyOnline",16384,nullptr,1,nullptr,0)!=pdPASS){
  for(unsigned i=1;i<3;++i){g_onlineStatus[i].enabled=false;g_onlineStatus[i].error=-6;}
 }
 g_onlineNextHeartbeat=millis()+HEARTBEAT_INTERVAL_SEC*1000UL;
}
static void onlineLoopProgress(uint32_t sensorsAt,bool hardwareOK){
 const uint32_t now=millis();portENTER_CRITICAL(&g_onlineMux);
 g_onlineHealth.loopAt=now;g_onlineHealth.roofAt=now;g_onlineHealth.sensorsAt=sensorsAt;
 g_onlineHealth.hardwareOK=hardwareOK;g_onlineHealth.completed=true;++g_onlineHealth.generation;
 portEXIT_CRITICAL(&g_onlineMux);
 OnlineResult result;unsigned budget=4;while(budget--&&g_onlineResults&&xQueueReceive(g_onlineResults,&result,0)==pdTRUE)
  eventLogEx(result.ok?"INFO":"ERROR","NOTIFY","channel=%u result=%d accepted=%u",result.channel,result.code,result.ok);
 if(astro_notify::due(now,g_onlineNextHeartbeat)){
  g_onlineNextHeartbeat=now+HEARTBEAT_INTERVAL_SEC*1000UL;
  if(onlineHealthSnapshot().usable(now))onlineEnqueue(2,"",false);
  else if(onlineEnabled(2))onlineRecord(2,false,-3);
 }
}
static String onlineStatusJson(){
 const uint32_t now=millis();OnlineStatus snapshot[3];portENTER_CRITICAL(&g_onlineMux);memcpy(snapshot,g_onlineStatus,sizeof(snapshot));portEXIT_CRITICAL(&g_onlineMux);
 String out="{\"channels\":[";const char* names[]={"Telegram","Pushover","Heartbeat"};
 for(unsigned i=0;i<3;++i){if(i)out+=',';auto& s=snapshot[i];out+="{\"id\":"+String(i)+",\"name\":\""+names[i]+"\",\"compiled\":"+(s.compiled?"true":"false")+",\"enabled\":"+(s.enabled?"true":"false")+",\"configured\":"+(s.configured?"true":"false")+",\"inflight\":"+(s.inflight?"true":"false")+",\"last_success_age_s\":"+(s.hasOK?String(uint32_t(now-s.lastOK)/1000):String("null"))+",\"last_attempt_age_s\":"+(s.hasTry?String(uint32_t(now-s.lastTry)/1000):String("null"))+",\"last_error\":"+String(s.error)+",\"accepted\":"+String(s.okCount)+",\"failed\":"+String(s.failed)+",\"dropped\":"+String(s.dropped)+"}";}
 out+="],\"controller_healthy\":";out+=onlineHealthSnapshot().usable(now)?"true":"false";out+="}";return out;
}

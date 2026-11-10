#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
enum OnlineChannel:uint8_t { ONLINE_TELEGRAM=0,ONLINE_PUSHOVER=1,ONLINE_HEARTBEAT=2 };
// Telegram and new HTTPS channels share one transport slot; never wait in loop().
static SemaphoreHandle_t g_notifyTransportMutex=nullptr;
static bool onlineTelegramEnabled();
static void onlineTelegramStarted();
static void onlineTelegramResult(bool success,int code);
static void onlineCritical(const char* title,const char* message);
static void onlineBegin();
static void onlineLoopProgress(uint32_t sensorsAt,bool hardwareOK);
static String onlineStatusJson();
static bool onlineSetEnabled(uint8_t channel,bool enabled);
static bool onlineTest(uint8_t channel);

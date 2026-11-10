#include <Arduino.h>
#include "AstroEthernetGuard.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/portmacro.h>
static SemaphoreHandle_t chipMutex=nullptr;
static portMUX_TYPE chipMux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t timeouts=0;
static SemaphoreHandle_t mutexHandle() {
  portENTER_CRITICAL(&chipMux); auto handle=chipMutex; portEXIT_CRITICAL(&chipMux);
  if(handle) return handle;
  auto fresh=xSemaphoreCreateRecursiveMutex();
  portENTER_CRITICAL(&chipMux);
  if(!chipMutex) { chipMutex=fresh; fresh=nullptr; }
  handle=chipMutex;
  portEXIT_CRITICAL(&chipMux);
  if(fresh) vSemaphoreDelete(fresh);
  return handle;
}
AstroEthernetGuard::AstroEthernetGuard() {
  acquire();
}
bool AstroEthernetGuard::acquire() {
  if(held) return true;
  const auto handle=mutexHandle();
  held=handle && xSemaphoreTakeRecursive(handle,pdMS_TO_TICKS(200))==pdTRUE;
  if(!held) astroEthTimeout();
  return held;
}
void AstroEthernetGuard::release() { if(held) { held=false; xSemaphoreGiveRecursive(chipMutex); } }
AstroEthernetGuard::~AstroEthernetGuard() { release(); }
void astroEthTimeout() { portENTER_CRITICAL(&chipMux); ++timeouts; portEXIT_CRITICAL(&chipMux); }
uint32_t astroEthTimeoutCount() {
  portENTER_CRITICAL(&chipMux); const auto n=timeouts; portEXIT_CRITICAL(&chipMux); return n;
}

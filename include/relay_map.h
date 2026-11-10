#pragma once
#include <stdint.h>
// Stable identity preserves legacy POE AUX1=CH7. CH6 heater becomes AUX2.
static constexpr uint8_t astroAuxChannel(uint8_t id) {
  switch(id){case 1:return 6;case 2:return 5;case 3:return 7;case 4:return 3;case 5:return 4;default:return 255;}
}
static constexpr uint8_t astroAuxIdForChannel(uint8_t ch) {
  switch(ch){case 6:return 1;case 5:return 2;case 7:return 3;case 3:return 4;case 4:return 5;default:return 0;}
}
static constexpr bool astroAuxAvailable(uint8_t id) {
  switch(id) {
    case 1:return AUX1_ENABLED;
    case 2:return AUX2_ENABLED;
    case 3:return AUX3_ENABLED;
    case 4:return AUX4_ENABLED && !COLLISION_POWER_RELAY_ENABLED;
    case 5:return AUX5_ENABLED && !MOUNT_POWER_RELAY_ENABLED;
    default:return false;
  }
}
static constexpr const char* astroRelayName(uint8_t ch) {
  switch(ch) {
    case 0:return "OPEN";case 1:return "CLOSE";case 2:return "STOP";
    case 3:return COLLISION_POWER_RELAY_ENABLED ? "COLLISION POWER" : "AUX4";
    case 4:return MOUNT_POWER_RELAY_ENABLED ? "MOUNT/AP900 POWER" : "AUX5";
    case 5:return "AUX2";case 6:return "AUX1";case 7:return "AUX3";default:return "INVALID";
  }
}

#pragma once
#ifndef ASTRO_TEST_CONFIG
#include "astro_generated_config.h"
#endif
#if SIM900_ENABLED && SIM7600_ENABLED
#error "Select only one SMS modem: SIM900 or SIM7600"
#endif
#if SIM7600_ENABLED && (!SIM7600_RS232_CONFIRMED || SIM_BAUD != 115200)
#error "SIM7600 DTU requires confirmed level conversion and SIM_BAUD=115200"
#endif
#if PUSHOVER_EXPIRE_SEC < PUSHOVER_RETRY_SEC
#error "Pushover expiry must cover at least one retry interval"
#endif
#ifndef OBS_BOARD_8RO
#error "Select a named PlatformIO environment and use scripts/load_config.py"
#endif
#if COLLISION_POWER_RELAY_ENABLED && !COLLISION_SENSOR_ENABLED
#error "Collision power requires collision sensor"
#endif
#if MOUNT_POWER_RELAY_ENABLED && !MOUNT_LIMIT_ENABLED
#error "Mount power requires E-STOP input"
#endif
#if (HEATER_PWM_ENABLED || LED_PWM_ENABLED) && !PWM_MODULE_ENABLED
#error "Heaters/LED require PWM_MODULE_ENABLED"
#endif
#if PWM_MODULE_ENABLED && !OBS_BOARD_8RO && MODBUS_RELAY_ADDR == HEATER_PWM_ADDR
#error "Relay and PWM must use different Modbus addresses"
#endif
#if OBS_BOARD_8RO && ILLUM_DO_ENABLED
#error "8RO CH8 is a relay AUX; cannot be local DO lighting"
#endif
#if ROOF_MOTION_ENABLED && (!PARK_GUARD_ENABLED || !MOUNT_LIMIT_ENABLED || !ROOF_INTERLOCK_RELAYS)
#error "Roof motion requires Park Guard, E-STOP and relay interlock"
#endif
#if !USE_WIFI && !USE_W5500
#error "Enable at least one network interface"
#endif
#if LED_PWM_CHANNEL != 4
#error "PWM CH1..CH3 are heaters; red LED must use CH4"
#endif
#if MODBUS_RELAY_ADDR < 1 || MODBUS_RELAY_ADDR > 255 || HEATER_PWM_ADDR < 1 || HEATER_PWM_ADDR > 255
#error "Modbus slave address must be 1..255"
#endif
#ifdef DEW_HEATER_ENABLED
#error "MLX enclosure heater removed; CH6 belongs to AUX2"
#endif

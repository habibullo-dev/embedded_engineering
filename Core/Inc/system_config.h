/* system_config.h - System-wide Configuration and Constants */
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "main.h"

// Terminal Colors - Modern, accessible color palette
#define COLOR_RESET     "\033[0m"
#define COLOR_PRIMARY   "\033[38;5;15m"   // Clean white
#define COLOR_SUCCESS   "\033[38;5;46m"   // Soft green
#define COLOR_ERROR     "\033[38;5;196m"  // Soft red
#define COLOR_WARNING   "\033[38;5;214m"  // Soft orange
#define COLOR_INFO      "\033[38;5;81m"   // Soft cyan
#define COLOR_MUTED     "\033[38;5;245m"  // Soft gray
#define COLOR_ACCENT    "\033[38;5;141m"  // Soft purple
#define COLOR_PROMPT    "\033[38;5;39m"   // Clean blue

// System Configuration
#define SYSTEM_VERSION "Multi-Sensor Terminal v2.1 (Modular)"
#define SYSTEM_AUTHOR  "STM32F767 Professional System"

// Timing Configuration
#define SENSOR_UPDATE_INTERVAL_MS    5000    // Update sensors every 5 seconds
#define LED_UPDATE_INTERVAL_MS       100     // Update LED timers every 100ms
#define SYSTEM_TICK_INTERVAL_MS      1       // System tick every 1ms

// Hardware Pin Definitions (can be moved here for centralized config)
#define I2C_INSTANCE        &hi2c2
#define UART_INSTANCE       &huart4

// System Limits
#define MAX_SYSTEM_TASKS    10      // For future FreeRTOS expansion
#define WATCHDOG_TIMEOUT_MS 30000   // 30 second watchdog

// Debug Configuration
#define DEBUG_ENABLED       1
#define VERBOSE_LOGGING     0

// Feature Flags
#define ENABLE_CLIMATE_SENSOR    1
#define ENABLE_ACCELEROMETER     1
#define ENABLE_LED_CONTROL       1
#define ENABLE_DIAGNOSTICS       1

// Utility Macros
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// System State Definitions
typedef enum {
    SYSTEM_STATE_INIT,
    SYSTEM_STATE_RUNNING,
    SYSTEM_STATE_ERROR,
    SYSTEM_STATE_SHUTDOWN
} SystemState_t;

// Common return codes
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR,
    STATUS_TIMEOUT,
    STATUS_BUSY,
    STATUS_NOT_INITIALIZED
} SystemStatus_t;

#endif /* SYSTEM_CONFIG_H */

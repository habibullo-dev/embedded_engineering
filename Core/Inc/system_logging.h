/* system_logging.h - System Logging Service Interface */
#ifndef SYSTEM_LOGGING_H
#define SYSTEM_LOGGING_H

#include "main.h"

// Log level definitions
typedef enum {
    LOG_INFO,
    LOG_SUCCESS,
    LOG_ERROR,
    LOG_WARNING,
    LOG_LOGIN,
    LOG_SENSOR,
    LOG_DEBUG
} LogLevel_t;

// Log entry structure
typedef struct {
    uint32_t timestamp;
    LogLevel_t level;
    char module[16];
    char message[64];
} LogEntry_t;

// Public function declarations
void SystemLog_Init(void);
void SystemLog_Add(LogLevel_t level, const char* module, const char* message);
void SystemLog_Display(void);
void SystemLog_Clear(void);
uint8_t SystemLog_GetCount(void);

// Utility functions
const char* SystemLog_GetLevelString(LogLevel_t level);
const char* SystemLog_GetLevelColor(LogLevel_t level);

#endif /* SYSTEM_LOGGING_H */

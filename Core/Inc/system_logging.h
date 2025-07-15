/* system_logging.h - System Logging Service Interface - FreeRTOS Version */
#ifndef SYSTEM_LOGGING_H
#define SYSTEM_LOGGING_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

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
    TickType_t timestamp;    // Changed to FreeRTOS tick type
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

// FreeRTOS-specific logging functions
void SystemLog_AddFromISR(LogLevel_t level, const char* module, const char* message);
void SystemLog_LogTaskInfo(const char* taskName);
void SystemLog_LogHeapInfo(void);

#endif /* SYSTEM_LOGGING_H */

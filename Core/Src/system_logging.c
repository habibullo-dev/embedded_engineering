/* system_logging.c - System Logging Service Implementation - FreeRTOS Version */
#include "system_logging.h"
#include "system_config.h"
#include "freertos_globals.h"  // For global FreeRTOS objects
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

// Private constants
#define LOG_SIZE 10

// Private variables
static LogEntry_t system_logs[LOG_SIZE];
static uint8_t log_count = 0;
static uint8_t log_index = 0;
static SemaphoreHandle_t logMutex = NULL;  // Protect log buffer from concurrent access

// External references
extern UART_HandleTypeDef huart4;

// Private function
static void send_string(const char* str) {
    if (uartMutex != NULL && xSemaphoreTake(uartMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 1000);
        xSemaphoreGive(uartMutex);
    } else {
        // Fallback without mutex if not available
        HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 1000);
    }
}

// Public function implementations
void SystemLog_Init(void) {
    // Create mutex for log buffer protection - only if not already created
    if (logMutex == NULL) {
        logMutex = xSemaphoreCreateMutex();
    }

    // Initialize log buffer
    log_count = 0;
    log_index = 0;
    memset(system_logs, 0, sizeof(system_logs));
}

void SystemLog_Add(LogLevel_t level, const char* module, const char* message) {
    // Thread-safe log entry addition
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Use FreeRTOS tick count instead of system_tick
        system_logs[log_index].timestamp = xTaskGetTickCount();
        system_logs[log_index].level = level;

        strncpy(system_logs[log_index].module, module, 15);
        system_logs[log_index].module[15] = '\0';

        strncpy(system_logs[log_index].message, message, 63);
        system_logs[log_index].message[63] = '\0';

        log_index = (log_index + 1) % LOG_SIZE;
        if (log_count < LOG_SIZE) {
            log_count++;
        }

        xSemaphoreGive(logMutex);
    } else {
        // If mutex not available, still try to log without protection
        system_logs[log_index].timestamp = xTaskGetTickCount();
        system_logs[log_index].level = level;

        strncpy(system_logs[log_index].module, module, 15);
        system_logs[log_index].module[15] = '\0';

        strncpy(system_logs[log_index].message, message, 63);
        system_logs[log_index].message[63] = '\0';

        log_index = (log_index + 1) % LOG_SIZE;
        if (log_count < LOG_SIZE) {
            log_count++;
        }
    }
}

void SystemLog_Display(void) {
    send_string(COLOR_INFO "System Logs:\r\n" COLOR_RESET);
    send_string(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    // Thread-safe log reading
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (log_count == 0) {
            send_string(COLOR_MUTED " No logs available\r\n" COLOR_RESET);
        } else {
            uint8_t start_idx = (log_index - log_count + LOG_SIZE) % LOG_SIZE;

            for (uint8_t i = 0; i < log_count; i++) {
                uint8_t idx = (start_idx + i) % LOG_SIZE;
                char log_line[120];

                // Convert FreeRTOS tick count to time format
                uint32_t tick_ms = system_logs[idx].timestamp * portTICK_PERIOD_MS;
                uint32_t seconds = tick_ms / 1000;
                uint32_t minutes = seconds / 60;
                uint32_t hours = minutes / 60;

                sprintf(log_line, "%s %02lu:%02lu:%02lu" COLOR_MUTED " %-7s %-10s %s" COLOR_RESET "\r\n",
                    SystemLog_GetLevelColor(system_logs[idx].level),
                    hours % 24, minutes % 60, seconds % 60,
                    SystemLog_GetLevelString(system_logs[idx].level),
                    system_logs[idx].module,
                    system_logs[idx].message);

                send_string(log_line);
            }
        }
        xSemaphoreGive(logMutex);
    } else {
        send_string(COLOR_ERROR " Error: Could not access log buffer\r\n" COLOR_RESET);
    }

    send_string(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void SystemLog_Clear(void) {
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        log_count = 0;
        log_index = 0;
        memset(system_logs, 0, sizeof(system_logs));
        xSemaphoreGive(logMutex);
    } else {
        // Clear without mutex if needed
        log_count = 0;
        log_index = 0;
        memset(system_logs, 0, sizeof(system_logs));
    }
}

uint8_t SystemLog_GetCount(void) {
    uint8_t count = 0;
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        count = log_count;
        xSemaphoreGive(logMutex);
    } else {
        count = log_count;  // Return without mutex protection
    }
    return count;
}

const char* SystemLog_GetLevelString(LogLevel_t level) {
    switch(level) {
        case LOG_INFO:    return "INFO";
        case LOG_SUCCESS: return "SUCCESS";
        case LOG_ERROR:   return "ERROR";
        case LOG_WARNING: return "WARN";
        case LOG_LOGIN:   return "LOGIN";
        case LOG_SENSOR:  return "SENSOR";
        case LOG_DEBUG:   return "DEBUG";
        default:          return "UNKNOWN";
    }
}

const char* SystemLog_GetLevelColor(LogLevel_t level) {
    switch(level) {
        case LOG_INFO:    return COLOR_INFO;
        case LOG_SUCCESS: return COLOR_SUCCESS;
        case LOG_ERROR:   return COLOR_ERROR;
        case LOG_WARNING: return COLOR_WARNING;
        case LOG_LOGIN:   return COLOR_ACCENT;
        case LOG_SENSOR:  return COLOR_SUCCESS;
        case LOG_DEBUG:   return COLOR_ACCENT;
        default:          return COLOR_PRIMARY;
    }
}

// Additional FreeRTOS-specific logging functions
void SystemLog_AddFromISR(LogLevel_t level, const char* module, const char* message) {
    // ISR-safe version of SystemLog_Add
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (logMutex != NULL) {
        if (xSemaphoreTakeFromISR(logMutex, &xHigherPriorityTaskWoken) == pdTRUE) {
            system_logs[log_index].timestamp = xTaskGetTickCountFromISR();
            system_logs[log_index].level = level;

            strncpy(system_logs[log_index].module, module, 15);
            system_logs[log_index].module[15] = '\0';

            strncpy(system_logs[log_index].message, message, 63);
            system_logs[log_index].message[63] = '\0';

            log_index = (log_index + 1) % LOG_SIZE;
            if (log_count < LOG_SIZE) {
                log_count++;
            }

            xSemaphoreGiveFromISR(logMutex, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

void SystemLog_LogTaskInfo(const char* taskName) {
    // Helper function to log task information
    TaskHandle_t xTask = xTaskGetHandle(taskName);
    if (xTask != NULL) {
        UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(xTask);
        eTaskState eState = eTaskGetState(xTask);
        UBaseType_t uxPriority = uxTaskPriorityGet(xTask);

        char logMsg[64];
        const char* stateStr;

        switch(eState) {
            case eRunning:   stateStr = "Running"; break;
            case eReady:     stateStr = "Ready"; break;
            case eBlocked:   stateStr = "Blocked"; break;
            case eSuspended: stateStr = "Suspended"; break;
            case eDeleted:   stateStr = "Deleted"; break;
            default:         stateStr = "Unknown"; break;
        }

        sprintf(logMsg, "%s: %s, Prio=%lu, Stack=%lu", taskName, stateStr, (unsigned long)uxPriority, (unsigned long)uxHighWaterMark);
        SystemLog_Add(LOG_DEBUG, "freertos", logMsg);
    }
}

void SystemLog_LogHeapInfo(void) {
    // Log current heap status
    size_t freeHeap = xPortGetFreeHeapSize();
    size_t minFreeHeap = xPortGetMinimumEverFreeHeapSize();

    char heapMsg[64];
    sprintf(heapMsg, "Heap: %d free, %d min free", freeHeap, minFreeHeap);
    SystemLog_Add(LOG_INFO, "freertos", heapMsg);
}

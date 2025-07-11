/* system_logging.c - System Logging Service Implementation */
#include "system_logging.h"
#include "system_config.h"
#include <string.h>
#include <stdio.h>

// Private constants
#define LOG_SIZE 10

// Private variables
static LogEntry_t system_logs[LOG_SIZE];
static uint8_t log_count = 0;
static uint8_t log_index = 0;

// External references
extern volatile uint32_t system_tick;
extern UART_HandleTypeDef huart4;

// Private function
static void send_string(const char* str) {
    HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 1000);
}

// Public function implementations
void SystemLog_Init(void) {
    log_count = 0;
    log_index = 0;
    memset(system_logs, 0, sizeof(system_logs));
}

void SystemLog_Add(LogLevel_t level, const char* module, const char* message) {
    system_logs[log_index].timestamp = system_tick;
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

void SystemLog_Display(void) {
    send_string(COLOR_INFO "System Logs:\r\n" COLOR_RESET);
    send_string(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    if (log_count == 0) {
        send_string(COLOR_MUTED " No logs available\r\n" COLOR_RESET);
    } else {
        uint8_t start_idx = (log_index - log_count + LOG_SIZE) % LOG_SIZE;

        for (uint8_t i = 0; i < log_count; i++) {
            uint8_t idx = (start_idx + i) % LOG_SIZE;
            char log_line[120];

            uint32_t seconds = system_logs[idx].timestamp / 1000;
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
    send_string(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void SystemLog_Clear(void) {
    SystemLog_Init();
}

uint8_t SystemLog_GetCount(void) {
    return log_count;
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

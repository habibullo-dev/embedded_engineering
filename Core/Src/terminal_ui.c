/* terminal_ui.c - Terminal User Interface with Interactive Logs */
#include "terminal_ui.h"
#include "system_config.h"
#include "system_logging.h"
#include "sensors.h"
#include "led_control.h"
#include "freertos_globals.h"  // For global FreeRTOS objects
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "persistent_logging.h"
#include "user_config.h"

// Configuration
#define MAX_CMD_LENGTH 32
#define RX_BUFFER_SIZE 64
#define HISTORY_SIZE 5
#define SESSION_TIMEOUT_MS 300000

// Account management states
#define ACCOUNT_STATE_IDLE 0
#define ACCOUNT_STATE_PASSWORD_VERIFY 1
#define ACCOUNT_STATE_NEW_USERNAME 2
#define ACCOUNT_STATE_NEW_PASSWORD 3
#define ACCOUNT_STATE_CONFIRM_PASSWORD 4

// UART buffer management
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_char;

// Command processing
static volatile char cmd_buffer[MAX_CMD_LENGTH + 1];
static volatile uint8_t cmd_index = 0;
static volatile uint8_t cursor_pos = 0;
static volatile uint8_t current_state = 0;  // 0=username, 1=password, 2=logged_in

// Account management state
static volatile uint8_t account_state = ACCOUNT_STATE_IDLE;
static volatile char temp_new_username[MAX_USERNAME_LENGTH];
static volatile char temp_new_password[MAX_PASSWORD_LENGTH];

// Logs mode state
static volatile uint8_t logs_mode_active = 0;
static volatile uint32_t current_log_page = 0;
static volatile uint32_t total_log_pages = 0;

// Command history
static volatile char command_history[HISTORY_SIZE][MAX_CMD_LENGTH + 1];
static volatile uint8_t history_count = 0;
static volatile uint8_t history_index = 0;
static volatile int8_t current_history_pos = -1;
static volatile char temp_command[MAX_CMD_LENGTH + 1];

// Session management
static volatile uint32_t last_activity = 0;

// Command auto-completion and validation
static const char* valid_commands[] = {
    "help", "whoami", "clear", "history", "logout",
    "logs", "clear-logs", "confirm-clear-logs", "account",
    "led", "status", "sensors", "uptime", "accel", "climate", 
    "i2cscan", "sensortest", "tasks", 
};
static const uint8_t command_count = sizeof(valid_commands) / sizeof(valid_commands[0]);

// External references
extern UART_HandleTypeDef huart4;
extern I2C_HandleTypeDef hi2c2;

// Function declarations
static uint8_t buffer_has_data(void);
static char get_char_from_buffer(void);
static void process_character(char c);
static void parse_led_command(char* cmd);
static void display_logs_page(uint32_t page);
static void show_logs_navigation(void);
static void show_logs_help(void);
static const char* get_level_color(LogLevel_t level);
static const char* get_level_name(LogLevel_t level);
static uint32_t parse_time_attribute(char* cmd);
static void add_to_history(char* cmd);
static void show_history_command(int8_t direction);
static void clear_current_line(void);
static void redraw_command_line(void);
static void move_cursor_left(void);
static void move_cursor_right(void);
static void insert_char_at_cursor(char c);
static void delete_char_at_cursor(void);
static void redraw_from_cursor(void);
static void process_account_command(void);
static void process_account_input(char* input);
static const char* format_uptime(uint32_t ms);
static uint8_t find_command_matches(const char* partial, char matches[][MAX_CMD_LENGTH + 1], uint8_t max_matches);
static uint8_t validate_command(const char* cmd);
static void redraw_command_with_color(void);
static void handle_tab_completion(void);
static char* trim_string(char* str);

// Thread-safe UART transmission helper
static void safe_uart_transmit(const uint8_t* data, uint16_t size) {
    if (xSemaphoreTake(uartMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(&huart4, data, size, 1000);
        xSemaphoreGive(uartMutex);
    }
}

// Public function implementations
void TerminalUI_Init(void) {
    __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
    NVIC_SetPriority(UART4_IRQn, 5);  // FreeRTOS compatible priority
    NVIC_EnableIRQ(UART4_IRQn);
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);

    current_state = 0;
    cmd_index = 0;
    cursor_pos = 0;
    history_count = 0;
    history_index = 0;
    current_history_pos = -1;
    last_activity = 0;
    account_state = ACCOUNT_STATE_IDLE;
    
    // Initialize user configuration system
    UserConfig_Init();
}

void TerminalUI_ProcessInput(void) {
    while (buffer_has_data()) {
        char c = get_char_from_buffer();
        process_character(c);
    }
}

void TerminalUI_ShowBanner(void) {
    // Clear terminal and position cursor at top
    TerminalUI_SendString("\033[2J\033[H");
    
    TerminalUI_SendString(COLOR_MUTED "╭─────────────────────────────────────────╮\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "STM32F767" COLOR_MUTED " Professional Terminal         │\r\n");
    TerminalUI_SendString("│ " COLOR_INFO "Multi-Sensor" COLOR_MUTED " • " COLOR_SUCCESS "HDC1080" COLOR_MUTED " • " COLOR_WARNING "ADXL345" COLOR_MUTED "    │\r\n");
    TerminalUI_SendString("│ " COLOR_SUCCESS "FreeRTOS v10.x" COLOR_MUTED " Multi-Threading       │\r\n");
    TerminalUI_SendString("╰─────────────────────────────────────────╯" COLOR_RESET "\r\n");

    // Show sensor status using global data
    const ClimateData_t* climate = Sensors_GetClimate();
    const AccelData_t* accel = Sensors_GetAccel();

    if (climate->sensor_ok || accel->sensor_ok) {
        char sensor_info[150];

        if (climate->sensor_ok && accel->sensor_ok) {
            sprintf(sensor_info, COLOR_MUTED "🌡️  %.1f°C, %.1f%% RH • 📐 %s\r\n",
                   climate->temperature, climate->humidity, Sensors_GetOrientationStatus());
        } else if (climate->sensor_ok) {
            sprintf(sensor_info, COLOR_MUTED "🌡️  %.1f°C, %.1f%% RH • 📐 Offline\r\n",
                   climate->temperature, climate->humidity);
        } else if (accel->sensor_ok) {
            sprintf(sensor_info, COLOR_MUTED "🌡️  Offline • 📐 %s\r\n", Sensors_GetOrientationStatus());
        }
        TerminalUI_SendString(sensor_info);
    } else {
        TerminalUI_SendString(COLOR_MUTED "🌡️  Offline • 📐 Offline\r\n");
    }
    TerminalUI_SendString("\r\n");
    TerminalUI_SendString(COLOR_MUTED "login: " COLOR_RESET);
}

void TerminalUI_ShowPrompt(void) {
    // Don't show normal prompt if we're in logs mode
    if (logs_mode_active) {
        HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);  // Still need to receive input for logs mode
        return;
    }
    TerminalUI_SendString("\r\n" COLOR_PROMPT "root" COLOR_MUTED "@" COLOR_PROMPT "stm32" COLOR_MUTED ":" COLOR_ACCENT "~" COLOR_MUTED "$ " COLOR_RESET);
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
}

void TerminalUI_SendString(const char* str) {
    if (xSemaphoreTake(uartMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 1000);
        xSemaphoreGive(uartMutex);
    }
}

void TerminalUI_PrintStatus(const char* message, const char* color) {
    if (xSemaphoreTake(uartMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit(&huart4, (uint8_t*)color, strlen(color), 1000);
        HAL_UART_Transmit(&huart4, (uint8_t*)message, strlen(message), 1000);
        HAL_UART_Transmit(&huart4, (uint8_t*)COLOR_RESET "\r\n", strlen(COLOR_RESET "\r\n"), 1000);
        xSemaphoreGive(uartMutex);
    }
}

void TerminalUI_ProcessCommand(void) {
    // Trim the command buffer to handle spaces properly
    char* trimmed_cmd = trim_string((char*)cmd_buffer);
    
    if (current_state == 0) { // Username
        char username_buffer[MAX_USERNAME_LENGTH];
        UserConfig_GetCurrentUsername(username_buffer, MAX_USERNAME_LENGTH);
        
        if (strcmp(trimmed_cmd, username_buffer) == 0) {
            TerminalUI_SendString(COLOR_MUTED "password: " COLOR_RESET);
            current_state = 1;
            PersistentLog_Add(LOG_INFO, "auth", "Valid username");
        } else {
            TerminalUI_PrintStatus("Invalid username", COLOR_ERROR);
            TerminalUI_SendString(COLOR_MUTED "login: " COLOR_RESET);
            PersistentLog_Add(LOG_WARNING, "auth", "Invalid username");
        }
    }
    else if (current_state == 1) { // Password
        char username_buffer[MAX_USERNAME_LENGTH];
        UserConfig_GetCurrentUsername(username_buffer, MAX_USERNAME_LENGTH);
        
        if (UserConfig_ValidateCredentials(username_buffer, trimmed_cmd)) {
            TerminalUI_PrintStatus("Welcome! Type 'help' for commands", COLOR_SUCCESS);
            Sensors_UpdateAll();
            TerminalUI_ShowPrompt();
            current_state = 2;
            last_activity = xTaskGetTickCount();  // Use FreeRTOS tick count
            PersistentLog_Add(LOG_LOGIN, "auth", "Authentication successful");
        } else {
            TerminalUI_PrintStatus("Access denied", COLOR_ERROR);
            TerminalUI_SendString(COLOR_MUTED "login: " COLOR_RESET);
            current_state = 0;
            PersistentLog_Add(LOG_ERROR, "auth", "Authentication failed");
        }
    }
    else if (current_state == 2) { // Logged in
        if (strcmp(trimmed_cmd, "whoami") == 0) {
            TerminalUI_PrintStatus("root", COLOR_INFO);
        }
        else if (strncmp(trimmed_cmd, "led", 3) == 0) {
            parse_led_command(trimmed_cmd);
        }
        else if (strcmp(trimmed_cmd, "clear") == 0) {
            TerminalUI_SendString("\033[2J\033[H");
        }
        else if (strcmp(trimmed_cmd, "history") == 0) {
            TerminalUI_SendString(COLOR_INFO "Command History:\r\n" COLOR_RESET);
            for (uint8_t i = 0; i < history_count; i++) {
                char msg[80];
                sprintf(msg, COLOR_MUTED " %d. " COLOR_PRIMARY "%s\r\n", i + 1, (char*)command_history[i]);
                TerminalUI_SendString(msg);
            }
        }
        else if (strcmp(trimmed_cmd, "logs") == 0) {
            TerminalUI_EnterLogsMode();
        }
        else if (strcmp(trimmed_cmd, "clear-logs") == 0) {
            uint32_t log_count = PersistentLog_GetCount();
            if (log_count == 0) {
                TerminalUI_PrintStatus("No logs to clear", COLOR_INFO);
            } else {
                char warning[100];
                sprintf(warning, "WARNING: This will permanently delete all %lu logs!", log_count);
                TerminalUI_PrintStatus(warning, COLOR_WARNING);
                TerminalUI_PrintStatus("Type 'confirm-clear-logs' to proceed", COLOR_ACCENT);
            }
        }
        else if (strcmp(trimmed_cmd, "confirm-clear-logs") == 0) {
            PersistentLog_EraseAll();
            TerminalUI_PrintStatus("All logs permanently deleted", COLOR_SUCCESS);
        }
        else if (strcmp(trimmed_cmd, "status") == 0) {
            TerminalUI_ShowSystemInfo();
        }
        else if (strcmp(trimmed_cmd, "uptime") == 0) {
            TerminalUI_ShowUptime();
        }
        else if (strcmp(trimmed_cmd, "sensors") == 0) {
            Sensors_UpdateAll();
            TerminalUI_ShowAllSensors();
        }
        else if (strcmp(trimmed_cmd, "accel") == 0) {
            Sensors_UpdateAccel();
            TerminalUI_ShowDetailedAccel();
        }
        else if (strcmp(trimmed_cmd, "climate") == 0) {
            const ClimateData_t* climate = Sensors_GetClimate();
            TerminalUI_SendString(COLOR_INFO "Climate Data:\r\n" COLOR_RESET);
            TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
            if (climate->sensor_ok) {
                char msg[120];
                sprintf(msg, COLOR_MUTED " Temperature: " COLOR_PRIMARY "%.2f°C\r\n", climate->temperature);
                TerminalUI_SendString(msg);
                sprintf(msg, COLOR_MUTED " Humidity:    " COLOR_PRIMARY "%.2f%% RH\r\n", climate->humidity);
                TerminalUI_SendString(msg);
                sprintf(msg, COLOR_MUTED " Status:      %s\r\n", Sensors_GetComfortStatus());
                TerminalUI_SendString(msg);
            } else {
                TerminalUI_SendString(COLOR_ERROR "Climate sensor offline or error\r\n");
            }
            TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
        }
        else if (strcmp(trimmed_cmd, "i2cscan") == 0) {
            TerminalUI_I2CScan();
        }
        else if (strcmp(trimmed_cmd, "i2ctest") == 0) {
            TerminalUI_I2CTest();
        }
        else if (strcmp(trimmed_cmd, "sensortest") == 0) {
            Sensors_RunAllTests();
        }
        else if (strcmp(trimmed_cmd, "tasks") == 0) {
            // FreeRTOS task monitoring
            TerminalUI_SendString(COLOR_INFO "FreeRTOS Task Information:\r\n" COLOR_RESET);
            TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

            char taskList[512];
            TerminalUI_SendString(COLOR_MUTED "Task Name       State  Prio  Stack  Num\r\n" COLOR_RESET);
            vTaskList(taskList);
            TerminalUI_SendString(taskList);

            TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

            // Show heap info
            size_t freeHeap = xPortGetFreeHeapSize();
            size_t minFreeHeap = xPortGetMinimumEverFreeHeapSize();
            char heapMsg[120];
            sprintf(heapMsg, COLOR_MUTED " Free Heap: " COLOR_PRIMARY "%d bytes" COLOR_MUTED " (Min: " COLOR_PRIMARY "%d" COLOR_MUTED ")\r\n",
                   freeHeap, minFreeHeap);
            TerminalUI_SendString(heapMsg);
        }
        else if (strcmp(trimmed_cmd, "stack") == 0) {
            // Show stack usage for current task
            UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            char stackMsg[60];
            sprintf(stackMsg, COLOR_INFO "Current task stack remaining: " COLOR_PRIMARY "%lu words\r\n", (unsigned long)uxHighWaterMark);
            TerminalUI_SendString(stackMsg);
        }
        else if (strcmp(trimmed_cmd, "help") == 0) {
            TerminalUI_ShowHelp();
        }
        else if (strcmp(trimmed_cmd, "logout") == 0) {
            TerminalUI_Logout();
            return;
        }
        else if (strcmp(trimmed_cmd, "account") == 0) {
            process_account_command();
            return;
        }
        else if (strlen(trimmed_cmd) == 0) {
            // Empty command - just show prompt
        }
        else {
            char error_msg[80];
            sprintf(error_msg, "Unknown command: %s", (char*)cmd_buffer);
            TerminalUI_PrintStatus(error_msg, COLOR_ERROR);
            TerminalUI_SendString(COLOR_MUTED "Type 'help' for available commands\r\n");
        }

        if (current_state == 2) { // Still logged in
            TerminalUI_ShowPrompt();
        }
    }
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
}

uint8_t TerminalUI_IsLoggedIn(void) {
    return (current_state == 2);
}

void TerminalUI_Logout(void) {
    TerminalUI_PrintStatus("Goodbye!", COLOR_WARNING);
    vTaskDelay(pdMS_TO_TICKS(300));  // Use FreeRTOS delay
    current_state = 0;
    last_activity = 0;
    PersistentLog_Add(LOG_LOGIN, "auth", "User logged out");
    TerminalUI_ShowBanner();
}

void TerminalUI_CheckTimeout(void) {
    if (current_state == 2 && last_activity > 0) {
        TickType_t currentTick = xTaskGetTickCount();
        if ((currentTick - last_activity) > pdMS_TO_TICKS(SESSION_TIMEOUT_MS)) {
            TerminalUI_PrintStatus("Session timeout - automatically logged out", COLOR_WARNING);
            current_state = 0;
            last_activity = 0;
            PersistentLog_Add(LOG_LOGIN, "auth", "Session timeout");
            TerminalUI_ShowBanner();
        }
    }
}

void TerminalUI_ShowHelp(void) {
    TerminalUI_SendString(COLOR_INFO "Available Commands:\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "Basic Commands:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "whoami" COLOR_MUTED "           Show current user\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "clear" COLOR_MUTED "            Clear terminal\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "history" COLOR_MUTED "          Show command history\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "logs" COLOR_MUTED "             Interactive log viewer\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "clear-logs" COLOR_MUTED "       Delete all logs (requires confirmation)\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "logout" COLOR_MUTED "           Exit session\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "account" COLOR_MUTED "          Change username and password\r\n");
    TerminalUI_SendString("\r\n" COLOR_MUTED " " COLOR_ACCENT "System Information:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "status" COLOR_MUTED "           Show comprehensive system status\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "uptime" COLOR_MUTED "           Show system uptime\r\n");
    TerminalUI_SendString("\r\n" COLOR_MUTED " " COLOR_ACCENT "FreeRTOS Commands:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "tasks" COLOR_MUTED "            Show FreeRTOS tasks and heap\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "stack" COLOR_MUTED "            Show current task stack usage\r\n");
    TerminalUI_SendString("\r\n" COLOR_MUTED " " COLOR_ACCENT "LED Control:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "led on|off N" COLOR_MUTED "     Control LED N (1-3)\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "led on|off all" COLOR_MUTED "   Control all LEDs\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "led on N -t SEC" COLOR_MUTED "  LED with timer (auto-off)\r\n");
    TerminalUI_SendString("\r\n" COLOR_MUTED " " COLOR_ACCENT "Multi-Sensor:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "sensors" COLOR_MUTED "          Show all sensors\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "climate" COLOR_MUTED "          Temperature/humidity details\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "accel" COLOR_MUTED "            Detailed accelerometer\r\n");
    TerminalUI_SendString("\r\n" COLOR_MUTED " " COLOR_ACCENT "Diagnostics:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "sensortest" COLOR_MUTED "       Comprehensive sensor test\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "i2cscan" COLOR_MUTED "          Scan I2C bus\r\n");
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

// UART Interrupt Callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart4) {
        rx_buffer[rx_head] = rx_char;
        rx_head = (rx_head + 1) % RX_BUFFER_SIZE;
        HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
    }
}

// Private function implementations
static uint8_t buffer_has_data(void) {
    return (rx_head != rx_tail);
}

static char get_char_from_buffer(void) {
    if (!buffer_has_data()) return 0;
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    return c;
}

static void process_character(char c) {
    // Handle logs mode input
    if (logs_mode_active) {
        TerminalUI_ProcessLogsMode(c);
        return;
    }

    if (c == '\r' || c == '\n') {
        if (cmd_index > MAX_CMD_LENGTH) {
            TerminalUI_PrintStatus("Command too long", COLOR_ERROR);
        } else {
            cmd_buffer[cmd_index] = '\0';
            TerminalUI_SendString("\r\n");
            if (current_state == 2 && strlen((char*)cmd_buffer) > 0) {
                add_to_history((char*)cmd_buffer);
            }
            
            // Check if we're in account management mode
            if (account_state != ACCOUNT_STATE_IDLE) {
                process_account_input((char*)cmd_buffer);
            } else {
                TerminalUI_ProcessCommand();
            }
        }
        memset((void*)cmd_buffer, 0, sizeof(cmd_buffer));
        cmd_index = 0;
        cursor_pos = 0;
        current_history_pos = -1;
        return;
    }

    static uint8_t escape_state = 0;
    if (c == 27) {
        escape_state = 1;
        return;
    } else if (escape_state == 1 && c == '[') {
        escape_state = 2;
        return;
    } else if (escape_state == 2) {
        escape_state = 0;
        if (c == 'A' && current_state == 2) show_history_command(-1);
        else if (c == 'B' && current_state == 2) show_history_command(1);
        else if (c == 'C' && current_state == 2) move_cursor_right();
        else if (c == 'D' && current_state == 2) move_cursor_left();
        return;
    }

    if (c == '\t' && current_state == 2) {  // TAB key for auto-completion
        handle_tab_completion();
        return;
    }
    escape_state = 0;

    if (c == 127 || c == 8) {
        if (cursor_pos > 0) {
            delete_char_at_cursor();
            current_history_pos = -1;
            // Update coloring after any deletion to reflect current command state
            if (current_state == 2) {
                redraw_command_with_color();
            }
        }
        return;
    }

    if (c >= 32 && c <= 126) {
        if (cmd_index < MAX_CMD_LENGTH) {
            insert_char_at_cursor(c);
            current_history_pos = -1;
            // Update coloring after any character insertion to reflect current command state
            if (current_state == 2) {
                redraw_command_with_color();
            }
            last_activity = xTaskGetTickCount();  // Use FreeRTOS tick count
        } else {
            TerminalUI_SendString(COLOR_ERROR "!" COLOR_RESET);
        }
    }
}

static void parse_led_command(char* cmd) {
    uint8_t is_on = (strstr(cmd, " on") != NULL);
    uint32_t timer_duration = parse_time_attribute(cmd);

    if (strstr(cmd, " all")) {
        LED_ControlAll(is_on);
        char msg[50];
        sprintf(msg, "All LEDs turned %s", is_on ? "on" : "off");
        TerminalUI_PrintStatus(msg, COLOR_SUCCESS);

        if (is_on && timer_duration > 0) {
            for (uint8_t i = 1; i <= 3; i++) {
                LED_SetTimer(i, timer_duration);
            }
        }
    } else {
        uint8_t led_num = 0;
        if (strstr(cmd, " 1")) led_num = 1;
        else if (strstr(cmd, " 2")) led_num = 2;
        else if (strstr(cmd, " 3")) led_num = 3;

        if (led_num >= 1 && led_num <= 3) {
            LED_Control(led_num, is_on);
            char msg[50];
            sprintf(msg, "LED%d turned %s", led_num, is_on ? "on" : "off");
            TerminalUI_PrintStatus(msg, COLOR_SUCCESS);

            if (is_on && timer_duration > 0) {
                LED_SetTimer(led_num, timer_duration);
            }
        } else {
            TerminalUI_PrintStatus("Invalid LED number (1-3)", COLOR_ERROR);
        }
    }
}

static uint32_t parse_time_attribute(char* cmd) {
    char* time_pos = strstr(cmd, "-t ");
    if (time_pos) {
        return atoi(time_pos + 3) * 1000;
    }
    return 0;
}

static const char* format_uptime(uint32_t ms) __attribute__((unused));  // Mark as unused to suppress warning
static const char* format_uptime(uint32_t ms) {
    static char uptime_str[50];
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    if (days > 0)
        sprintf(uptime_str, "%lu days, %02lu:%02lu:%02lu", days, hours % 24, minutes % 60, seconds % 60);
    else
        sprintf(uptime_str, "%02lu:%02lu:%02lu", hours % 24, minutes % 60, seconds % 60);

    return uptime_str;
}

/**
 * Find commands that match the partial input
 */
static uint8_t find_command_matches(const char* partial, char matches[][MAX_CMD_LENGTH + 1], uint8_t max_matches) {
    uint8_t match_count = 0;
    uint8_t partial_len = strlen(partial);
    
    if (partial_len == 0) return 0;  // No empty completions
    
    for (uint8_t i = 0; i < command_count && match_count < max_matches; i++) {
        if (strncmp(valid_commands[i], partial, partial_len) == 0) {
            strncpy(matches[match_count], valid_commands[i], MAX_CMD_LENGTH);
            matches[match_count][MAX_CMD_LENGTH] = '\0';
            match_count++;
        }
    }
    
    return match_count;
}

/**
 * Trim whitespace from string (modifies original string)
 */
static char* trim_string(char* str) {
    char* end;
    
    // Trim leading space
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    
    if (*str == 0) return str;  // All spaces
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    
    // Write new null terminator
    *(end + 1) = 0;
    
    return str;
}

/**
 * Validate command and return status: 0=invalid, 1=partial, 2=valid
 */
static uint8_t validate_command(const char* cmd) {
    // Create a trimmed copy for validation
    char trimmed_cmd[MAX_CMD_LENGTH + 1];
    strncpy(trimmed_cmd, cmd, MAX_CMD_LENGTH);
    trimmed_cmd[MAX_CMD_LENGTH] = '\0';
    trim_string(trimmed_cmd);
    
    uint8_t cmd_len = strlen(trimmed_cmd);
    if (cmd_len == 0) return 0;
    
    // Extract just the first word (the base command) for validation
    char base_cmd[MAX_CMD_LENGTH + 1];
    strncpy(base_cmd, trimmed_cmd, MAX_CMD_LENGTH);
    base_cmd[MAX_CMD_LENGTH] = '\0';
    
    // Find the first space to get just the command word
    char* space_pos = strchr(base_cmd, ' ');
    if (space_pos) {
        *space_pos = '\0';  // Terminate at first space
    }
    
    uint8_t base_len = strlen(base_cmd);
    if (base_len == 0) return 0;
    
    uint8_t exact_match = 0;
    uint8_t partial_match = 0;
    
    for (uint8_t i = 0; i < command_count; i++) {
        if (strcmp(valid_commands[i], base_cmd) == 0) {
            exact_match = 1;
            break;
        }
        if (strncmp(valid_commands[i], base_cmd, base_len) == 0) {
            partial_match = 1;
        }
    }
    
    if (exact_match) return 2;      // Valid complete command
    if (partial_match) return 1;    // Partial match
    return 0;                       // No match
}

/**
 * Redraw command line with color coding (minimal cursor movement)
 */
static void redraw_command_with_color(void) {
    if (current_state != 2 || cmd_index == 0) {
        return;  // Only colorize during logged-in state with commands
    }
    
    // Save cursor position and move to start of command
    char move_to_start[10];
    sprintf(move_to_start, "\033[%dD", cursor_pos);
    TerminalUI_SendString(move_to_start);
    
    // Apply appropriate color based on current command validation
    uint8_t validation = validate_command((char*)cmd_buffer);
    switch (validation) {
        case 0: TerminalUI_SendString(COLOR_ERROR); break;     // Invalid - red
        case 1: TerminalUI_SendString(COLOR_WARNING); break;   // Partial - yellow  
        case 2: TerminalUI_SendString(COLOR_SUCCESS); break;   // Valid - green
        default: TerminalUI_SendString(COLOR_RESET); break;
    }
    
    // Redraw command
    for (uint8_t i = 0; i < cmd_index; i++) {
        safe_uart_transmit((uint8_t*)&cmd_buffer[i], 1);
    }
    
    // Reset color and clear any trailing characters
    TerminalUI_SendString(COLOR_RESET "\033[K");
    
    // Move cursor back to original position
    if (cursor_pos < cmd_index) {
        char move_back[10];
        sprintf(move_back, "\033[%dD", cmd_index - cursor_pos);
        TerminalUI_SendString(move_back);
    }
}
static void add_to_history(char* cmd) {
    if (history_count > 0 && strcmp(cmd, (char*)command_history[(history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE]) == 0) {
        return;
    }
    strncpy((char*)command_history[history_index], cmd, MAX_CMD_LENGTH);
    command_history[history_index][MAX_CMD_LENGTH] = '\0';
    history_index = (history_index + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

static void show_history_command(int8_t direction) {
    if (history_count == 0) return;
    if (current_history_pos == -1) {
        strncpy((char*)temp_command, (char*)cmd_buffer, cmd_index);
        temp_command[cmd_index] = '\0';
    }
    if (direction == -1) {
        if (current_history_pos == -1) {
            current_history_pos = (history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        } else if (current_history_pos != (history_index - history_count + HISTORY_SIZE) % HISTORY_SIZE) {
            current_history_pos = (current_history_pos - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        }
    } else {
        if (current_history_pos != -1) {
            if (current_history_pos == (history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE) {
                current_history_pos = -1;
                clear_current_line();
                strcpy((char*)cmd_buffer, (char*)temp_command);
                cmd_index = strlen((char*)cmd_buffer);
                cursor_pos = cmd_index;
                redraw_command_line();
                return;
            } else {
                current_history_pos = (current_history_pos + 1) % HISTORY_SIZE;
            }
        }
    }
    if (current_history_pos != -1) {
        clear_current_line();
        strcpy((char*)cmd_buffer, (char*)command_history[current_history_pos]);
        cmd_index = strlen((char*)cmd_buffer);
        cursor_pos = cmd_index;
        redraw_command_line();
    }
}

static void clear_current_line(void) {
    TerminalUI_SendString("\033[K");  // Clear from cursor to end, no \r
}

static void redraw_command_line(void) {
    TerminalUI_SendString("\033[2K\r");  // Clear entire line, then move to start
    if (current_state == 2) {
        TerminalUI_SendString(COLOR_PROMPT "root" COLOR_MUTED "@" COLOR_PROMPT "stm32" COLOR_MUTED ":" COLOR_ACCENT "~" COLOR_MUTED "$ " COLOR_RESET);
    }

    for (uint8_t i = 0; i < cmd_index; i++) {
        safe_uart_transmit((uint8_t*)&cmd_buffer[i], 1);
    }

    if (cursor_pos < cmd_index) {
        char move_cmd[10];
        sprintf(move_cmd, "\033[%dD", cmd_index - cursor_pos);
        TerminalUI_SendString(move_cmd);
    }
}

static void move_cursor_left(void) {
    if (cursor_pos > 0) {
        cursor_pos--;
        TerminalUI_SendString("\033[D");
    }
}

static void move_cursor_right(void) {
    if (cursor_pos < cmd_index) {
        cursor_pos++;
        TerminalUI_SendString("\033[C");
    }
}

static void insert_char_at_cursor(char c) {
    if (cmd_index >= MAX_CMD_LENGTH) return;

    if (current_state == 1) {
        cmd_buffer[cmd_index] = c;
        cmd_index++;
        cursor_pos = cmd_index;
        TerminalUI_SendString("*");
        return;
    }

    for (int i = cmd_index; i > cursor_pos; --i) {
        cmd_buffer[i] = cmd_buffer[i - 1];
    }
    cmd_buffer[cursor_pos] = c;
    cmd_index++;
    cursor_pos++;
    cmd_buffer[cmd_index] = '\0';
    safe_uart_transmit((uint8_t*)&c, 1);
    if (cursor_pos < cmd_index) {
        redraw_from_cursor();
    }
}

static void delete_char_at_cursor(void) {
    if (cursor_pos == 0 || cmd_index == 0) return;
    for (int i = cursor_pos - 1; i < cmd_index - 1; ++i) {
        cmd_buffer[i] = cmd_buffer[i + 1];
    }
    cmd_index--;
    cursor_pos--;
    cmd_buffer[cmd_index] = '\0';
    TerminalUI_SendString("\033[D");
    redraw_from_cursor();
}

static void redraw_from_cursor(void) {
    TerminalUI_SendString("\033[K");
    for (uint8_t i = cursor_pos; i < cmd_index; i++) {
        safe_uart_transmit((uint8_t*)&cmd_buffer[i], 1);
    }
    if (cursor_pos < cmd_index) {
        char move_cmd[10];
        sprintf(move_cmd, "\033[%dD", cmd_index - cursor_pos);
        TerminalUI_SendString(move_cmd);
    }
}

// ===== LOGS MODE IMPLEMENTATION =====

void TerminalUI_EnterLogsMode(void) {
    uint32_t log_count = PersistentLog_GetCount();
    
    if (log_count == 0) {
        TerminalUI_PrintStatus("No persistent logs found", COLOR_WARNING);
        return;
    }
    
    logs_mode_active = 1;
    current_log_page = 0;
    total_log_pages = (log_count + 9) / 10;  // 10 logs per page
    
    TerminalUI_SendString("\r\n");
    TerminalUI_SendString(COLOR_ACCENT "╭────────────────────────────────────────╮\r\n");
    TerminalUI_SendString("│" COLOR_RESET "          PERSISTENT LOGS MODE         " COLOR_ACCENT "│\r\n");
    TerminalUI_SendString("╰────────────────────────────────────────╯" COLOR_RESET "\r\n");
    
    char info[100];
    sprintf(info, "Found %lu logs across %lu pages\r\n\r\n", log_count, total_log_pages);
    TerminalUI_SendString(info);
    
    // Display first page
    display_logs_page(current_log_page);
    show_logs_navigation();
}

void TerminalUI_ProcessLogsMode(char c) {
    
    switch (c) {
        case 'n':
        case 'N':
            if (current_log_page < total_log_pages - 1) {
                current_log_page++;
                display_logs_page(current_log_page);
                show_logs_navigation();
            } else {
                TerminalUI_SendString(COLOR_WARNING "Already on last page!" COLOR_RESET "\r\n");
                show_logs_navigation();
            }
            break;
            
        case 'p':
        case 'P':
            if (current_log_page > 0) {
                current_log_page--;
                display_logs_page(current_log_page);
                show_logs_navigation();
            } else {
                TerminalUI_SendString(COLOR_WARNING "Already on first page!" COLOR_RESET "\r\n");
                show_logs_navigation();
            }
            break;
            
        case 'q':
        case 'Q':
            logs_mode_active = 0;
            TerminalUI_SendString(COLOR_SUCCESS "Exiting logs mode..." COLOR_RESET "\r\n\r\n");
            TerminalUI_ShowPrompt();
            break;
            
        case 'h':
        case 'H':
            show_logs_help();
            break;
            
        default:
            TerminalUI_SendString(COLOR_ERROR "Invalid command! Press 'h' for help" COLOR_RESET "\r\n");
            show_logs_navigation();
            break;
    }
}

uint8_t TerminalUI_IsInLogsMode(void) {
    return logs_mode_active;
}

/**
 * Display a specific page of logs
 */
static void display_logs_page(uint32_t page) {
    // Clear screen and show header
    TerminalUI_SendString("\033[2J\033[H");
    
    TerminalUI_SendString(COLOR_ACCENT "╭────────────────────────────────────────╮\r\n");
    TerminalUI_SendString("│" COLOR_RESET "          PERSISTENT LOGS MODE         " COLOR_ACCENT "│\r\n");
    TerminalUI_SendString("╰────────────────────────────────────────╯" COLOR_RESET "\r\n");
    
    char page_info[80];
    sprintf(page_info, "Page %lu/%lu\r\n\r\n", page + 1, total_log_pages);
    TerminalUI_SendString(page_info);
    
    // Get and display logs for this page
    uint32_t start_idx = page * 10;
    uint32_t logs_on_page = 0;
    
    volatile FlashLogHeader_t* header = (volatile FlashLogHeader_t*)0x08040000;
    
    // Find valid logs and display them
    uint32_t displayed = 0;
    for (uint32_t i = 0; i < 50 && displayed < 10; i++) {
        if (header->logs[i].magic == 0xDEADBEEF) {
            if (logs_on_page >= start_idx && logs_on_page < start_idx + 10) {
                uint32_t seconds = header->logs[i].timestamp / 1000;
                uint32_t minutes = seconds / 60;
                uint32_t hours = minutes / 60;
                
                char log_line[150];
                const char* level_color = get_level_color(header->logs[i].level);
                sprintf(log_line, "%s%02lu. [%02lu:%02lu:%02lu] %s | %s: %s" COLOR_RESET "\r\n",
                       level_color,
                       logs_on_page + 1,
                       hours % 24, minutes % 60, seconds % 60,
                       get_level_name(header->logs[i].level),
                       (char*)header->logs[i].module, 
                       (char*)header->logs[i].message);
                TerminalUI_SendString(log_line);
                displayed++;
            }
            logs_on_page++;
        }
    }
}

/**
 * Show navigation help in logs mode
 */
static void show_logs_navigation(void) {
    TerminalUI_SendString("\r\n" COLOR_MUTED "╭─ NAVIGATION ────────────────────────────╮\r\n");
    if (current_log_page > 0) {
        TerminalUI_SendString("│ " COLOR_ACCENT "'p'" COLOR_MUTED " - Previous page                    │\r\n");
    }
    if (current_log_page < total_log_pages - 1) {
        TerminalUI_SendString("│ " COLOR_ACCENT "'n'" COLOR_MUTED " - Next page                        │\r\n");
    }
    TerminalUI_SendString("│ " COLOR_ACCENT "'h'" COLOR_MUTED " - Help                             │\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "'q'" COLOR_MUTED " - Exit logs mode                   │\r\n");
    TerminalUI_SendString("╰─────────────────────────────────────────╯" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_ACCENT "logs> " COLOR_RESET);
}

/**
 * Show detailed help for logs mode
 */
static void show_logs_help(void) {
    TerminalUI_SendString("\r\n" COLOR_INFO "╭─ LOGS MODE HELP ────────────────────────╮\r\n");
    TerminalUI_SendString("│ " COLOR_RESET "Commands available in logs mode:        " COLOR_INFO "│\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "'n'" COLOR_RESET " or " COLOR_ACCENT "'N'" COLOR_RESET " - Navigate to next page          " COLOR_INFO "│\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "'p'" COLOR_RESET " or " COLOR_ACCENT "'P'" COLOR_RESET " - Navigate to previous page      " COLOR_INFO "│\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "'q'" COLOR_RESET " or " COLOR_ACCENT "'Q'" COLOR_RESET " - Exit logs mode                 " COLOR_INFO "│\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "'h'" COLOR_RESET " or " COLOR_ACCENT "'H'" COLOR_RESET " - Show this help                 " COLOR_INFO "│\r\n");
    TerminalUI_SendString("╰─────────────────────────────────────────╯" COLOR_RESET "\r\n");
    show_logs_navigation();
}

/**
 * Get color for log level
 */
static const char* get_level_color(LogLevel_t level) {
    switch(level) {
        case LOG_ERROR:   return COLOR_ERROR;
        case LOG_WARNING: return COLOR_WARNING;
        case LOG_INFO:    return COLOR_INFO;
        case LOG_LOGIN:   return COLOR_ACCENT;
        case LOG_SUCCESS: return COLOR_SUCCESS;
        case LOG_SENSOR:  return COLOR_MUTED;
        case LOG_DEBUG:   return COLOR_MUTED;
        default:          return COLOR_RESET;
    }
}

/**
 * Get readable name for log level
 */
static const char* get_level_name(LogLevel_t level) {
    switch(level) {
        case LOG_ERROR:   return "ERROR";
        case LOG_WARNING: return "WARN ";
        case LOG_INFO:    return "INFO ";
        case LOG_LOGIN:   return "LOGIN";
        case LOG_SUCCESS: return "SUCCS";
        case LOG_SENSOR:  return "SENSR";
        case LOG_DEBUG:   return "DEBUG";
        default:          return "UNKN ";
    }
}

/**
 * Handle TAB key for auto-completion
 */
static void handle_tab_completion(void) {
    char matches[10][MAX_CMD_LENGTH + 1];
    uint8_t match_count = find_command_matches((char*)cmd_buffer, matches, 10);
    
    if (match_count == 0) {
        // Quick visual feedback - no cursor movement
        TerminalUI_SendString(COLOR_ERROR "?" COLOR_RESET);
        vTaskDelay(pdMS_TO_TICKS(100));
        TerminalUI_SendString("\b \b");  // Erase the ?
    } else if (match_count == 1) {
        // Complete the command smoothly
        const char* complete_cmd = matches[0];
        uint8_t current_len = strlen((char*)cmd_buffer);
        uint8_t complete_len = strlen(complete_cmd);
        
        // Add only the missing characters
        for (uint8_t i = current_len; i < complete_len; i++) {
            if (cmd_index < MAX_CMD_LENGTH) {
                cmd_buffer[cmd_index] = complete_cmd[i];
                safe_uart_transmit((uint8_t*)&complete_cmd[i], 1);
                cmd_index++;
                cursor_pos++;
            }
        }
        cmd_buffer[cmd_index] = '\0';
        
        // Update command colors after completion
        redraw_command_with_color();
        
        // DON'T add space automatically - let user decide when to add parameters
        // This prevents validation issues and gives better UX
    } else {
        // Multiple matches - find common prefix and complete it
        uint8_t current_len = strlen((char*)cmd_buffer);
        uint8_t common_len = current_len;
        
        // Find the longest common prefix among all matches
        for (uint8_t pos = current_len; pos < MAX_CMD_LENGTH; pos++) {
            char test_char = matches[0][pos];
            if (test_char == '\0') break;
            
            uint8_t all_match = 1;
            for (uint8_t i = 1; i < match_count; i++) {
                if (matches[i][pos] != test_char || matches[i][pos] == '\0') {
                    all_match = 0;
                    break;
                }
            }
            
            if (all_match) {
                common_len = pos + 1;
            } else {
                break;
            }
        }
        
        // If we can extend the current input, do it
        if (common_len > current_len) {
            for (uint8_t i = current_len; i < common_len; i++) {
                if (cmd_index < MAX_CMD_LENGTH) {
                    cmd_buffer[cmd_index] = matches[0][i];
                    safe_uart_transmit((uint8_t*)&matches[0][i], 1);
                    cmd_index++;
                    cursor_pos++;
                }
            }
            cmd_buffer[cmd_index] = '\0';
            
            // Update command colors after partial completion
            redraw_command_with_color();
        } else {
            // No common prefix to complete - show a brief indicator
            TerminalUI_SendString(COLOR_WARNING "+" COLOR_RESET);
            vTaskDelay(pdMS_TO_TICKS(150));
            TerminalUI_SendString("\b \b");
        }
    }
}

// ===== ACCOUNT MANAGEMENT IMPLEMENTATION =====

/**
 * Process the account command - initiate credential change
 */
static void process_account_command(void) {
    if (current_state != 2) {
        TerminalUI_PrintStatus("Authentication required", COLOR_ERROR);
        return;
    }
    
    TerminalUI_SendString(COLOR_INFO "Account Management\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
    
    char current_username[MAX_USERNAME_LENGTH];
    UserConfig_GetCurrentUsername(current_username, MAX_USERNAME_LENGTH);
    
    char status_msg[80];
    sprintf(status_msg, "Current user: %s", current_username);
    TerminalUI_PrintStatus(status_msg, COLOR_INFO);
    
    if (UserConfig_IsUsingDefaults()) {
        TerminalUI_PrintStatus("Using default credentials", COLOR_WARNING);
    } else {
        TerminalUI_PrintStatus("Using custom credentials", COLOR_SUCCESS);
    }
    
    TerminalUI_SendString("\r\n" COLOR_WARNING "To change credentials, please enter your current password:\r\n");
    TerminalUI_SendString(COLOR_MUTED "password: " COLOR_RESET);
    
    account_state = ACCOUNT_STATE_PASSWORD_VERIFY;
}

/**
 * Process input during account management
 */
static void process_account_input(char* input) {
    char* trimmed_input = trim_string(input);
    
    switch (account_state) {
        case ACCOUNT_STATE_PASSWORD_VERIFY: {
            char current_username[MAX_USERNAME_LENGTH];
            UserConfig_GetCurrentUsername(current_username, MAX_USERNAME_LENGTH);
            
            if (UserConfig_ValidateCredentials(current_username, trimmed_input)) {
                TerminalUI_PrintStatus("Password verified", COLOR_SUCCESS);
                TerminalUI_SendString("\r\n" COLOR_INFO "Enter new username (3-15 chars): " COLOR_RESET);
                account_state = ACCOUNT_STATE_NEW_USERNAME;
            } else {
                TerminalUI_PrintStatus("Invalid password - account change cancelled", COLOR_ERROR);
                account_state = ACCOUNT_STATE_IDLE;
                TerminalUI_ShowPrompt();
            }
            break;
        }
        
        case ACCOUNT_STATE_NEW_USERNAME: {
            if (strlen(trimmed_input) < 3 || strlen(trimmed_input) > 15) {
                TerminalUI_PrintStatus("Username must be 3-15 characters", COLOR_ERROR);
                TerminalUI_SendString(COLOR_INFO "Enter new username (3-15 chars): " COLOR_RESET);
                return;
            }
            
            // Check for invalid characters
            for (int i = 0; trimmed_input[i]; i++) {
                if (!isalnum((unsigned char)trimmed_input[i]) && trimmed_input[i] != '_' && trimmed_input[i] != '-') {
                    TerminalUI_PrintStatus("Username can only contain letters, numbers, '_' and '-'", COLOR_ERROR);
                    TerminalUI_SendString(COLOR_INFO "Enter new username (3-15 chars): " COLOR_RESET);
                    return;
                }
            }
            
            strncpy((char*)temp_new_username, trimmed_input, MAX_USERNAME_LENGTH - 1);
            temp_new_username[MAX_USERNAME_LENGTH - 1] = '\0';
            
            char confirm_msg[80];
            sprintf(confirm_msg, "New username will be: %s", (char*)temp_new_username);
            TerminalUI_PrintStatus(confirm_msg, COLOR_INFO);
            
            TerminalUI_SendString(COLOR_INFO "Enter new password (4-15 chars): " COLOR_RESET);
            account_state = ACCOUNT_STATE_NEW_PASSWORD;
            break;
        }
        
        case ACCOUNT_STATE_NEW_PASSWORD: {
            if (strlen(trimmed_input) < 4 || strlen(trimmed_input) > 15) {
                TerminalUI_PrintStatus("Password must be 4-15 characters", COLOR_ERROR);
                TerminalUI_SendString(COLOR_INFO "Enter new password (4-15 chars): " COLOR_RESET);
                return;
            }
            
            strncpy((char*)temp_new_password, trimmed_input, MAX_PASSWORD_LENGTH - 1);
            temp_new_password[MAX_PASSWORD_LENGTH - 1] = '\0';
            
            TerminalUI_SendString(COLOR_INFO "Confirm new password: " COLOR_RESET);
            account_state = ACCOUNT_STATE_CONFIRM_PASSWORD;
            break;
        }
        
        case ACCOUNT_STATE_CONFIRM_PASSWORD: {
            if (strcmp((char*)temp_new_password, trimmed_input) == 0) {
                // Passwords match - save new credentials
                if (UserConfig_ChangeCredentials((char*)temp_new_username, (char*)temp_new_password)) {
                    TerminalUI_SendString("\r\n");
                    TerminalUI_PrintStatus("✓ Credentials successfully updated!", COLOR_SUCCESS);
                    TerminalUI_PrintStatus("Your new credentials are now active", COLOR_INFO);
                    
                    char success_msg[100];
                    sprintf(success_msg, "New user: %s", (char*)temp_new_username);
                    TerminalUI_PrintStatus(success_msg, COLOR_ACCENT);
                    
                    // Clear temporary storage
                    memset((void*)temp_new_username, 0, MAX_USERNAME_LENGTH);
                    memset((void*)temp_new_password, 0, MAX_PASSWORD_LENGTH);
                } else {
                    TerminalUI_PrintStatus("✗ Failed to save new credentials", COLOR_ERROR);
                    TerminalUI_PrintStatus("Please try again", COLOR_WARNING);
                }
            } else {
                TerminalUI_PrintStatus("Passwords do not match", COLOR_ERROR);
                TerminalUI_SendString(COLOR_INFO "Enter new password (4-15 chars): " COLOR_RESET);
                account_state = ACCOUNT_STATE_NEW_PASSWORD;
                return;
            }
            
            account_state = ACCOUNT_STATE_IDLE;
            TerminalUI_SendString("\r\n");
            TerminalUI_ShowPrompt();
            break;
        }
        
        default:
            account_state = ACCOUNT_STATE_IDLE;
            TerminalUI_ShowPrompt();
            break;
    }
}

/* terminal_ui.c - Terminal User Interface Implementation */
#include "terminal_ui.h"
#include "system_config.h"
#include "system_logging.h"
#include "sensors.h"
#include "led_control.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Private constants
#define MAX_CMD_LENGTH 32
#define RX_BUFFER_SIZE 64
#define USERNAME "admin"
#define PASSWORD "1234"
#define HISTORY_SIZE 5
#define SESSION_TIMEOUT_MS 300000

// Private variables - UART
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_char;

// Private variables - Command processing
static volatile char cmd_buffer[MAX_CMD_LENGTH + 1];
static volatile uint8_t cmd_index = 0;
static volatile uint8_t cursor_pos = 0;
static volatile uint8_t current_state = 0;  // 0=username, 1=password, 2=logged_in

// Private variables - Command history
static volatile char command_history[HISTORY_SIZE][MAX_CMD_LENGTH + 1];
static volatile uint8_t history_count = 0;
static volatile uint8_t history_index = 0;
static volatile int8_t current_history_pos = -1;
static volatile char temp_command[MAX_CMD_LENGTH + 1];

// Private variables - Session management
static volatile uint32_t last_activity = 0;

// External references
extern UART_HandleTypeDef huart4;
extern I2C_HandleTypeDef hi2c2;
extern volatile uint32_t system_tick;

// Private function declarations
static uint8_t buffer_has_data(void);
static char get_char_from_buffer(void);
static void process_character(char c);
static void parse_led_command(char* cmd);
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
static const char* format_uptime(uint32_t ms);

// Public function implementations
void TerminalUI_Init(void) {
    __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
    NVIC_SetPriority(UART4_IRQn, 0);
    NVIC_EnableIRQ(UART4_IRQn);
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);

    current_state = 0;
    cmd_index = 0;
    cursor_pos = 0;
    history_count = 0;
    history_index = 0;
    current_history_pos = -1;
    last_activity = 0;
}

void TerminalUI_ProcessInput(void) {
    while (buffer_has_data()) {
        char c = get_char_from_buffer();
        process_character(c);
    }
}

void TerminalUI_ShowBanner(void) {
    TerminalUI_SendString("\r\n");
    TerminalUI_SendString(COLOR_MUTED "╭─────────────────────────────────────────╮\r\n");
    TerminalUI_SendString("│ " COLOR_ACCENT "STM32F767" COLOR_MUTED " Professional Terminal         │\r\n");
    TerminalUI_SendString("│ " COLOR_INFO "Multi-Sensor" COLOR_MUTED " • " COLOR_SUCCESS "HDC1080" COLOR_MUTED " • " COLOR_WARNING "ADXL345" COLOR_MUTED "    │\r\n");
    TerminalUI_SendString("│ " COLOR_SUCCESS "Secure Shell v2.1" COLOR_MUTED " with Diagnostics      │\r\n");
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
    TerminalUI_SendString("\r\n" COLOR_PROMPT "root" COLOR_MUTED "@" COLOR_PROMPT "stm32" COLOR_MUTED ":" COLOR_ACCENT "~" COLOR_MUTED "$ " COLOR_RESET);
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
}

void TerminalUI_SendString(const char* str) {
    HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 1000);
}

void TerminalUI_PrintStatus(const char* message, const char* color) {
    TerminalUI_SendString(color);
    TerminalUI_SendString(message);
    TerminalUI_SendString(COLOR_RESET "\r\n");
}

void TerminalUI_ProcessCommand(void) {
    if (current_state == 0) { // Username
        if (strcmp((char*)cmd_buffer, USERNAME) == 0) {
            TerminalUI_SendString(COLOR_MUTED "password: " COLOR_RESET);
            current_state = 1;
            SystemLog_Add(LOG_INFO, "auth", "Valid username");
        } else {
            TerminalUI_PrintStatus("Invalid username", COLOR_ERROR);
            TerminalUI_SendString(COLOR_MUTED "login: " COLOR_RESET);
            SystemLog_Add(LOG_WARNING, "auth", "Invalid username");
        }
    }
    else if (current_state == 1) { // Password
        if (strcmp((char*)cmd_buffer, PASSWORD) == 0) {
            TerminalUI_PrintStatus("Welcome! Type 'help' for commands", COLOR_SUCCESS);
            Sensors_UpdateAll();
            TerminalUI_ShowPrompt();
            current_state = 2;
            last_activity = system_tick;
            SystemLog_Add(LOG_LOGIN, "auth", "Authentication successful");
        } else {
            TerminalUI_PrintStatus("Access denied", COLOR_ERROR);
            TerminalUI_SendString(COLOR_MUTED "login: " COLOR_RESET);
            current_state = 0;
            SystemLog_Add(LOG_ERROR, "auth", "Authentication failed");
        }
    }
    else if (current_state == 2) { // Logged in
        if (strcmp((char*)cmd_buffer, "whoami") == 0) {
            TerminalUI_PrintStatus("root", COLOR_INFO);
        }
        else if (strncmp((char*)cmd_buffer, "led", 3) == 0) {
            parse_led_command((char*)cmd_buffer);
        }
        else if (strcmp((char*)cmd_buffer, "clear") == 0) {
            TerminalUI_SendString("\033[2J\033[H");
        }
        else if (strcmp((char*)cmd_buffer, "history") == 0) {
            TerminalUI_SendString(COLOR_INFO "Command History:\r\n" COLOR_RESET);
            for (uint8_t i = 0; i < history_count; i++) {
                char msg[80];  // Increased buffer size
                sprintf(msg, COLOR_MUTED " %d. " COLOR_PRIMARY "%s\r\n", i + 1, (char*)command_history[i]);
                TerminalUI_SendString(msg);
            }
        }
        else if (strcmp((char*)cmd_buffer, "logs") == 0) {
            SystemLog_Display();
        }
        else if (strcmp((char*)cmd_buffer, "status") == 0) {
            TerminalUI_ShowSystemInfo();
        }
        else if (strcmp((char*)cmd_buffer, "uptime") == 0) {
            TerminalUI_ShowUptime();
        }
        else if (strcmp((char*)cmd_buffer, "sensors") == 0) {
            Sensors_UpdateAll();
            TerminalUI_ShowAllSensors();
        }
        else if (strcmp((char*)cmd_buffer, "accel") == 0) {
            Sensors_UpdateAccel();
            TerminalUI_ShowDetailedAccel();
        }
        else if (strcmp((char*)cmd_buffer, "climate") == 0) {
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
        else if (strcmp((char*)cmd_buffer, "i2cscan") == 0) {
            TerminalUI_I2CScan();
        }
        else if (strcmp((char*)cmd_buffer, "i2ctest") == 0) {
            TerminalUI_I2CTest();
        }
        else if (strcmp((char*)cmd_buffer, "sensortest") == 0) {
            // Comprehensive sensor test
            TerminalUI_SendString(COLOR_INFO "=== Comprehensive Sensor Test ===\r\n" COLOR_RESET);
            TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

            const ClimateData_t* climate = Sensors_GetClimate();
            const AccelData_t* accel = Sensors_GetAccel();

            // Test HDC1080
            TerminalUI_SendString(COLOR_ACCENT "Testing HDC1080 (Climate)...\r\n" COLOR_RESET);
            Sensors_UpdateClimate();
            climate = Sensors_GetClimate();
            if (climate->sensor_ok) {
                char msg[100];
                sprintf(msg, COLOR_SUCCESS "✓ HDC1080: %.2f°C, %.2f%% RH\r\n" COLOR_RESET,
                       climate->temperature, climate->humidity);
                TerminalUI_SendString(msg);
            } else {
                TerminalUI_SendString(COLOR_ERROR "✗ HDC1080: Communication failed\r\n" COLOR_RESET);
            }

            TerminalUI_SendString("\r\n");

            // Test ADXL345
            TerminalUI_SendString(COLOR_ACCENT "Testing ADXL345 (Accelerometer)...\r\n" COLOR_RESET);
            Sensors_UpdateAccel();
            accel = Sensors_GetAccel();
            if (accel->sensor_ok) {
                char msg[120];
                sprintf(msg, COLOR_SUCCESS "✓ ADXL345: X=%.3fg, Y=%.3fg, Z=%.3fg\r\n" COLOR_RESET,
                       accel->x_g, accel->y_g, accel->z_g);
                TerminalUI_SendString(msg);
            } else {
                TerminalUI_SendString(COLOR_ERROR "✗ ADXL345: Communication failed\r\n" COLOR_RESET);
            }

            TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

            if (climate->sensor_ok && accel->sensor_ok) {
                TerminalUI_SendString(COLOR_SUCCESS "🎉 ALL SENSORS OPERATIONAL!\r\n" COLOR_RESET);
            } else if (climate->sensor_ok || accel->sensor_ok) {
                TerminalUI_SendString(COLOR_WARNING "⚠ PARTIAL SENSOR FUNCTIONALITY\r\n" COLOR_RESET);
            } else {
                TerminalUI_SendString(COLOR_ERROR "❌ NO SENSORS RESPONDING\r\n" COLOR_RESET);
            }
        }
        else if (strcmp((char*)cmd_buffer, "help") == 0) {
            TerminalUI_ShowHelp();
        }
        else if (strcmp((char*)cmd_buffer, "logout") == 0) {
            TerminalUI_Logout();
            return;
        }
        else if (strlen((char*)cmd_buffer) == 0) {
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
    HAL_Delay(300);
    current_state = 0;
    last_activity = 0;
    SystemLog_Add(LOG_LOGIN, "auth", "User logged out");
    TerminalUI_ShowBanner();
}

void TerminalUI_CheckTimeout(void) {
    if (current_state == 2 && last_activity > 0) {
        if ((system_tick - last_activity) > SESSION_TIMEOUT_MS) {
            TerminalUI_PrintStatus("Session timeout - automatically logged out", COLOR_WARNING);
            current_state = 0;
            last_activity = 0;
            SystemLog_Add(LOG_LOGIN, "auth", "Session timeout");
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
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "logs" COLOR_MUTED "             Show system logs\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "logout" COLOR_MUTED "           Exit session\r\n");
    TerminalUI_SendString("\r\n" COLOR_MUTED " " COLOR_ACCENT "System Information:" COLOR_RESET "\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "status" COLOR_MUTED "           Show comprehensive system status\r\n");
    TerminalUI_SendString(COLOR_MUTED " " COLOR_ACCENT "uptime" COLOR_MUTED "           Show system uptime\r\n");
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
    if (c == '\r' || c == '\n') {
        if (cmd_index > MAX_CMD_LENGTH) {
            TerminalUI_PrintStatus("Command too long", COLOR_ERROR);
        } else {
            cmd_buffer[cmd_index] = '\0';
            TerminalUI_SendString("\r\n");
            if (current_state == 2 && strlen((char*)cmd_buffer) > 0) {
                add_to_history((char*)cmd_buffer);
            }
            TerminalUI_ProcessCommand();
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
    escape_state = 0;

    if (c == 127 || c == 8) {
        if (cursor_pos > 0) {
            delete_char_at_cursor();
            current_history_pos = -1;
        }
        return;
    }

    if (c >= 32 && c <= 126) {
        if (cmd_index < MAX_CMD_LENGTH) {
            insert_char_at_cursor(c);
            current_history_pos = -1;
            last_activity = system_tick;
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
                LED_SetTimer(i, timer_duration);  // FIX: Remove - system_tick
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
                LED_SetTimer(led_num, timer_duration);  // FIX: Remove - system_tick
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
    TerminalUI_SendString("\r\033[K");
    TerminalUI_ShowPrompt();
}

static void redraw_command_line(void) {
    for (uint8_t i = 0; i < cmd_index; i++) {
        HAL_UART_Transmit(&huart4, (uint8_t*)&cmd_buffer[i], 1, 100);
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
    HAL_UART_Transmit(&huart4, (uint8_t*)&c, 1, 100);
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
        HAL_UART_Transmit(&huart4, (uint8_t*)&cmd_buffer[i], 1, 100);
    }
    if (cursor_pos < cmd_index) {
        char move_cmd[10];
        sprintf(move_cmd, "\033[%dD", cmd_index - cursor_pos);
        TerminalUI_SendString(move_cmd);
    }
}

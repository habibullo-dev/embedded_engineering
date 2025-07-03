/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F767 Professional Terminal with Enhanced UI/UX
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_CMD_LENGTH 32
#define RX_BUFFER_SIZE 64
#define USERNAME "admin"
#define PASSWORD "1234"

// Modern, accessible color palette
#define COLOR_RESET     "\033[0m"
#define COLOR_PRIMARY   "\033[38;5;15m"   // Clean white
#define COLOR_SUCCESS   "\033[38;5;46m"   // Soft green
#define COLOR_ERROR     "\033[38;5;196m"  // Soft red
#define COLOR_WARNING   "\033[38;5;214m"  // Soft orange
#define COLOR_INFO      "\033[38;5;81m"   // Soft cyan
#define COLOR_MUTED     "\033[38;5;245m"  // Soft gray
#define COLOR_ACCENT    "\033[38;5;141m"  // Soft purple
#define COLOR_PROMPT    "\033[38;5;39m"   // Clean blue

// LED definitions
#define LED1_PIN GPIO_PIN_0
#define LED2_PIN GPIO_PIN_7
#define LED3_PIN GPIO_PIN_14
#define LED_PORT GPIOB

// Circular buffer for UART reception
volatile char rx_buffer[RX_BUFFER_SIZE];
volatile uint8_t rx_head = 0;
volatile uint8_t rx_tail = 0;
volatile uint8_t rx_char;

// Command processing
volatile char cmd_buffer[MAX_CMD_LENGTH + 1];
volatile uint8_t cmd_index = 0;
volatile uint8_t cursor_pos = 0;
volatile uint8_t current_state = 0;  // 0=username, 1=password, 2=logged_in

// Command history system
#define HISTORY_SIZE 5
volatile char command_history[HISTORY_SIZE][MAX_CMD_LENGTH + 1];
volatile uint8_t history_count = 0;
volatile uint8_t history_index = 0;
volatile int8_t current_history_pos = -1;
volatile char temp_command[MAX_CMD_LENGTH + 1];

// Logging system
#define LOG_SIZE 10
typedef enum {
    LOG_INFO,
    LOG_SUCCESS,
    LOG_ERROR,
    LOG_WARNING,
    LOG_LOGIN
} log_level_t;

typedef struct {
    uint32_t timestamp;
    log_level_t level;
    char command[MAX_CMD_LENGTH + 1];
    char message[64];
} log_entry_t;

volatile log_entry_t system_logs[LOG_SIZE];
volatile uint8_t log_count = 0;
volatile uint8_t log_index = 0;

// LED timer management
volatile uint32_t led_timers[3] = {0, 0, 0};
volatile uint8_t led_states[3] = {0, 0, 0};
volatile uint32_t system_tick = 0;

// Real-time clock for Seoul timezone (UTC+9)
typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

volatile rtc_time_t current_time = {5, 0, 10, 7, 3, 2025};  // Start at 09:00:00 Seoul time
volatile uint32_t rtc_tick_counter = 0;

// Session management
#define SESSION_TIMEOUT_MS 300000  // 5 minutes
volatile uint32_t last_activity = 0;
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t buffer_has_data(void);
char get_char_from_buffer(void);
void process_character(char c);
void process_command(void);
void send_string(const char* str);
void show_banner(void);
void show_prompt(void);

// LED control functions
void led_control(uint8_t led_num, uint8_t state);
void led_control_all(uint8_t state);
void led_timer_update(void);
void parse_led_command(char* cmd);
uint32_t parse_time_attribute(char* cmd);

// Command history functions
void add_to_history(char* cmd);
void show_history_command(int8_t direction);
void clear_current_line(void);
void redraw_command_line(void);

// Cursor management functions
void move_cursor_left(void);
void move_cursor_right(void);
void insert_char_at_cursor(char c);
void delete_char_at_cursor(void);
void redraw_from_cursor(void);

// Logging system functions
void add_log(log_level_t level, const char* command, const char* message);
void show_logs(void);
const char* get_log_level_string(log_level_t level);
const char* get_log_level_color(log_level_t level);

// UI helper functions
void print_status(const char* message, const char* color);
void print_separator(void);

// RTC and system functions
void update_rtc(void);
void show_uptime(void);
void show_system_info(void);
void check_session_timeout(void);
const char* format_uptime(uint32_t ms);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_UART4_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();

  /* USER CODE BEGIN 2 */
  show_banner();
  send_string(COLOR_MUTED "login: " COLOR_RESET);

  __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
  NVIC_SetPriority(UART4_IRQn, 0);
  NVIC_EnableIRQ(UART4_IRQn);

  HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
  led_control_all(0);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    while (buffer_has_data())
    {
        char c = get_char_from_buffer();
        process_character(c);
    }

    led_timer_update();
    update_rtc();
    check_session_timeout();

    static uint32_t last_tick = 0;
    if (HAL_GetTick() != last_tick)
    {
        last_tick = HAL_GetTick();
        system_tick++;
    }
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief UART RX Complete Callback
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart4)
    {
        rx_buffer[rx_head] = rx_char;
        rx_head = (rx_head + 1) % RX_BUFFER_SIZE;
        HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
    }
}

/**
  * @brief Check if buffer has data
  */
uint8_t buffer_has_data(void)
{
    return (rx_head != rx_tail);
}

/**
  * @brief Get character from buffer
  */
char get_char_from_buffer(void)
{
    if (!buffer_has_data()) return 0;

    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    return c;
}

/**
  * @brief Process individual character
  */
void process_character(char c)
{
    if (c == '\r' || c == '\n')
    {
        if (cmd_index > MAX_CMD_LENGTH)
        {
            print_status("Command too long", COLOR_ERROR);
            add_log(LOG_ERROR, (char*)cmd_buffer, "Command too long");
        }
        else
        {
            cmd_buffer[cmd_index] = '\0';
            send_string("\r\n");

            if (current_state == 2 && strlen((char*)cmd_buffer) > 0)
            {
                add_to_history((char*)cmd_buffer);
            }

            process_command();
        }
        cmd_index = 0;
        cursor_pos = 0;
        current_history_pos = -1;
        return;
    }

    // Handle escape sequences
    static uint8_t escape_state = 0;

    if (c == 27)
    {
        escape_state = 1;
        return;
    }
    else if (escape_state == 1 && c == '[')
    {
        escape_state = 2;
        return;
    }
    else if (escape_state == 2)
    {
        escape_state = 0;
        if (c == 'A' && current_state == 2) show_history_command(-1);
        else if (c == 'B' && current_state == 2) show_history_command(1);
        else if (c == 'C' && current_state == 2) move_cursor_right();
        else if (c == 'D' && current_state == 2) move_cursor_left();
        return;
    }

    escape_state = 0;

    // Handle backspace
    if (c == 127 || c == 8)
    {
        if (cursor_pos > 0)
        {
            delete_char_at_cursor();
            current_history_pos = -1;
        }
        return;
    }

    // Handle printable characters
    if (c >= 32 && c <= 126)
    {
        if (cmd_index < MAX_CMD_LENGTH)
        {
            insert_char_at_cursor(c);
            current_history_pos = -1;
            last_activity = system_tick;  // Update activity
        }
        else
        {
            send_string(COLOR_ERROR "!" COLOR_RESET);
        }
    }
}

/**
  * @brief Send string via UART
  */
void send_string(const char* str)
{
    HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 1000);
}

/**
  * @brief Show terminal prompt
  */
void show_prompt(void)
{
    send_string(COLOR_PROMPT "root" COLOR_MUTED "@" COLOR_PROMPT "stm32" COLOR_MUTED ":" COLOR_ACCENT "~" COLOR_MUTED "$ " COLOR_RESET);
}

/**
  * @brief Process complete command
  */
void process_command(void)
{
    if (current_state == 0) // Username
    {
        if (strcmp((char*)cmd_buffer, USERNAME) == 0)
        {
            send_string(COLOR_MUTED "password: " COLOR_RESET);
            current_state = 1;
            add_log(LOG_INFO, (char*)cmd_buffer, "Valid username");
        }
        else
        {
            print_status("Invalid username", COLOR_ERROR);
            send_string(COLOR_MUTED "login: " COLOR_RESET);
            add_log(LOG_WARNING, (char*)cmd_buffer, "Invalid username");
        }
    }
    else if (current_state == 1) // Password
    {
        if (strcmp((char*)cmd_buffer, PASSWORD) == 0)
        {
            print_status("Welcome! Type 'help' for commands", COLOR_SUCCESS);
            show_prompt();
            current_state = 2;
            add_log(LOG_LOGIN, "login", "Authentication successful");
        }
        else
        {
            print_status("Access denied", COLOR_ERROR);
            send_string(COLOR_MUTED "login: " COLOR_RESET);
            current_state = 0;
            add_log(LOG_ERROR, "login", "Authentication failed");
        }
    }
    else if (current_state == 2) // Logged in
    {
        if (strcmp((char*)cmd_buffer, "whoami") == 0)
        {
            print_status("root", COLOR_INFO);
            show_prompt();
            add_log(LOG_SUCCESS, (char*)cmd_buffer, "Command executed");
        }
        else if (strncmp((char*)cmd_buffer, "led", 3) == 0)
        {
            parse_led_command((char*)cmd_buffer);
            show_prompt();
            add_log(LOG_SUCCESS, (char*)cmd_buffer, "LED command executed");
        }
        else if (strcmp((char*)cmd_buffer, "clear") == 0)
        {
            send_string("\033[2J\033[H");
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Terminal cleared");
        }
        else if (strcmp((char*)cmd_buffer, "history") == 0)
        {
            send_string(COLOR_INFO "Command History:\r\n" COLOR_RESET);
            for (uint8_t i = 0; i < history_count; i++)
            {
                char msg[50];
                sprintf(msg, COLOR_MUTED " %d. " COLOR_PRIMARY "%s\r\n", i + 1, (char*)command_history[i]);
                send_string(msg);
            }
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "History displayed");
        }
        else if (strcmp((char*)cmd_buffer, "logs") == 0)
        {
            show_logs();
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Logs displayed");
        }
        else if (strcmp((char*)cmd_buffer, "status") == 0)
        {
            send_string(COLOR_INFO "System Status:\r\n" COLOR_RESET);
            char status[100];
            sprintf(status, COLOR_MUTED " Uptime: " COLOR_PRIMARY "%lu ms\r\n", system_tick);
            send_string(status);
            sprintf(status, COLOR_MUTED " LEDs: " COLOR_PRIMARY "1:%s 2:%s 3:%s\r\n",
                led_states[0] ? "ON" : "OFF",
                led_states[1] ? "ON" : "OFF",
                led_states[2] ? "ON" : "OFF");
            send_string(status);
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Status displayed");
        }
        else if (strcmp((char*)cmd_buffer, "uptime") == 0)
        {
            show_uptime();
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Uptime displayed");
        }
        else if (strcmp((char*)cmd_buffer, "sysinfo") == 0)
        {
            show_system_info();
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "System info displayed");
        }
        else if (strcmp((char*)cmd_buffer, "time") == 0)
        {
            char time_str[80];
            sprintf(time_str, COLOR_INFO "Seoul Time: " COLOR_PRIMARY "%04d-%02d-%02d %02d:%02d:%02d KST" COLOR_RESET,
                current_time.year, current_time.month, current_time.day,
                current_time.hours, current_time.minutes, current_time.seconds);
            print_status(time_str, "");
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Time displayed");
        }
        else if (strcmp((char*)cmd_buffer, "logout") == 0)
        {
            print_status("Goodbye!", COLOR_WARNING);
            HAL_Delay(300);
            current_state = 0;
            add_log(LOG_LOGIN, (char*)cmd_buffer, "User logged out");
            show_banner();
            send_string(COLOR_MUTED "login: " COLOR_RESET);
        }
        else if (strcmp((char*)cmd_buffer, "help") == 0)
        {
            send_string(COLOR_INFO "Available Commands:\r\n" COLOR_RESET);
            print_separator();
            send_string(COLOR_MUTED " " COLOR_ACCENT "whoami" COLOR_MUTED "           Show current user\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "led on|off N" COLOR_MUTED "     Control LED N (1-3)\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "led on|off all" COLOR_MUTED "   Control all LEDs\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "status" COLOR_MUTED "           Show system status\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "uptime" COLOR_MUTED "           Show system uptime\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "sysinfo" COLOR_MUTED "          Show detailed system info\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "time" COLOR_MUTED "             Show current time\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "clear" COLOR_MUTED "            Clear terminal\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "history" COLOR_MUTED "          Show command history\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "logs" COLOR_MUTED "             Show system logs\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "logout" COLOR_MUTED "           Exit session\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "help" COLOR_MUTED "             Show this help\r\n");
            print_separator();
            send_string(COLOR_WARNING "Timer Option:\r\n" COLOR_RESET);
            send_string(COLOR_MUTED " Add " COLOR_ACCENT "-t SEC" COLOR_MUTED " to LED commands for auto-off\r\n");
            send_string(COLOR_MUTED " Example: " COLOR_ACCENT "led on 1 -t 5" COLOR_MUTED " (turn on LED1 for 5 sec)\r\n");
            send_string("\r\n");
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Help displayed");
        }
        else if (strlen((char*)cmd_buffer) == 0)
        {
            show_prompt();
        }
        else
        {
            char error_msg[80];
            sprintf(error_msg, "Unknown command: %s", (char*)cmd_buffer);
            print_status(error_msg, COLOR_ERROR);
            send_string(COLOR_MUTED "Type 'help' for available commands\r\n");
            show_prompt();
            add_log(LOG_WARNING, (char*)cmd_buffer, "Unknown command");
        }
    }
}

/**
  * @brief Display startup banner
  */
void show_banner(void)
{
    send_string("\r\n");
    send_string(COLOR_MUTED "╭─────────────────────────────────────────╮\r\n");
    send_string("│ " COLOR_ACCENT "STM32F767" COLOR_MUTED " Professional Terminal         │\r\n");
    send_string("│ " COLOR_INFO "Cortex-M7" COLOR_MUTED " @ " COLOR_PRIMARY "216MHz" COLOR_MUTED " • " COLOR_PRIMARY "2MB Flash" COLOR_MUTED "          │\r\n");
    send_string("│ " COLOR_SUCCESS "Secure Shell v2.0" COLOR_MUTED "                       │\r\n");
    send_string("╰─────────────────────────────────────────╯" COLOR_RESET "\r\n\r\n");
}

/**
  * @brief Print status message with consistent formatting
  */
void print_status(const char* message, const char* color)
{
    send_string(color);
    send_string(message);
    send_string(COLOR_RESET "\r\n");
}

/**
  * @brief Control individual LED
  */
void led_control(uint8_t led_num, uint8_t state)
{
    uint16_t pin;

    switch(led_num)
    {
        case 1: pin = LED1_PIN; break;
        case 2: pin = LED2_PIN; break;
        case 3: pin = LED3_PIN; break;
        default: return;
    }

    if (state)
    {
        HAL_GPIO_WritePin(LED_PORT, pin, GPIO_PIN_SET);
        led_states[led_num - 1] = 1;
    }
    else
    {
        HAL_GPIO_WritePin(LED_PORT, pin, GPIO_PIN_RESET);
        led_states[led_num - 1] = 0;
        led_timers[led_num - 1] = 0;
    }
}

/**
  * @brief Control all LEDs
  */
void led_control_all(uint8_t state)
{
    for (uint8_t i = 1; i <= 3; i++)
    {
        led_control(i, state);
    }
}

/**
  * @brief Update LED timers
  */
void led_timer_update(void)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        if (led_timers[i] > 0 && led_states[i] == 1)
        {
            if (system_tick >= led_timers[i])
            {
                led_control(i + 1, 0);
                char msg[50];
                sprintf(msg, "LED%d timer expired", i + 1);
                print_status(msg, COLOR_WARNING);
                show_prompt();
            }
        }
    }
}

/**
  * @brief Parse time attribute
  */
uint32_t parse_time_attribute(char* cmd)
{
    char* time_pos = strstr(cmd, "-t ");
    if (time_pos)
    {
        return atoi(time_pos + 3) * 1000 + system_tick;
    }
    return 0;
}

/**
  * @brief Parse LED commands
  */
void parse_led_command(char* cmd)
{
    uint8_t is_on = (strstr(cmd, " on") != NULL);
    uint32_t timer_duration = parse_time_attribute(cmd);

    if (strstr(cmd, " all"))
    {
        led_control_all(is_on);
        char msg[50];
        sprintf(msg, "All LEDs turned %s", is_on ? "on" : "off");
        print_status(msg, COLOR_SUCCESS);

        if (is_on && timer_duration > 0)
        {
            for (uint8_t i = 0; i < 3; i++)
            {
                led_timers[i] = timer_duration;
            }
        }
    }
    else
    {
        uint8_t led_num = 0;
        if (strstr(cmd, " 1")) led_num = 1;
        else if (strstr(cmd, " 2")) led_num = 2;
        else if (strstr(cmd, " 3")) led_num = 3;

        if (led_num >= 1 && led_num <= 3)
        {
            led_control(led_num, is_on);
            char msg[50];
            sprintf(msg, "LED%d turned %s", led_num, is_on ? "on" : "off");
            print_status(msg, COLOR_SUCCESS);

            if (is_on && timer_duration > 0)
            {
                led_timers[led_num - 1] = timer_duration;
            }
        }
        else
        {
            print_status("Invalid LED number (1-3)", COLOR_ERROR);
        }
    }
}

/**
  * @brief Add command to history
  */
void add_to_history(char* cmd)
{
    if (history_count > 0 && strcmp(cmd, (char*)command_history[(history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE]) == 0)
    {
        return;
    }

    strncpy((char*)command_history[history_index], cmd, MAX_CMD_LENGTH);
    command_history[history_index][MAX_CMD_LENGTH] = '\0';

    history_index = (history_index + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE)
    {
        history_count++;
    }
}

/**
  * @brief Navigate command history
  */
void show_history_command(int8_t direction)
{
    if (history_count == 0) return;

    if (current_history_pos == -1)
    {
        strncpy((char*)temp_command, (char*)cmd_buffer, cmd_index);
        temp_command[cmd_index] = '\0';
    }

    if (direction == -1)
    {
        if (current_history_pos == -1)
        {
            current_history_pos = (history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        }
        else if (current_history_pos != (history_index - history_count + HISTORY_SIZE) % HISTORY_SIZE)
        {
            current_history_pos = (current_history_pos - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        }
    }
    else
    {
        if (current_history_pos != -1)
        {
            if (current_history_pos == (history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE)
            {
                current_history_pos = -1;
                clear_current_line();
                strcpy((char*)cmd_buffer, (char*)temp_command);
                cmd_index = strlen((char*)cmd_buffer);
                cursor_pos = cmd_index;
                redraw_command_line();
                return;
            }
            else
            {
                current_history_pos = (current_history_pos + 1) % HISTORY_SIZE;
            }
        }
    }

    if (current_history_pos != -1)
    {
        clear_current_line();
        strcpy((char*)cmd_buffer, (char*)command_history[current_history_pos]);
        cmd_index = strlen((char*)cmd_buffer);
        cursor_pos = cmd_index;
        redraw_command_line();
    }
}

/**
  * @brief Clear current line
  */
void clear_current_line(void)
{
    send_string("\r\033[K");
    show_prompt();
}

/**
  * @brief Redraw command line
  */
void redraw_command_line(void)
{
    char temp_cmd[MAX_CMD_LENGTH + 1];
    strncpy(temp_cmd, (char*)cmd_buffer, cmd_index);
    temp_cmd[cmd_index] = '\0';

    uint8_t is_valid = 0;
    if (strncmp(temp_cmd, "led", 3) == 0 ||
        strncmp(temp_cmd, "whoami", 6) == 0 ||
        strncmp(temp_cmd, "logout", 6) == 0 ||
        strncmp(temp_cmd, "clear", 5) == 0 ||
        strncmp(temp_cmd, "history", 7) == 0 ||
        strncmp(temp_cmd, "logs", 4) == 0 ||
        strncmp(temp_cmd, "status", 6) == 0 ||
        strncmp(temp_cmd, "uptime", 6) == 0 ||
        strncmp(temp_cmd, "sysinfo", 7) == 0 ||
        strncmp(temp_cmd, "time", 4) == 0 ||
        strncmp(temp_cmd, "help", 4) == 0)
    {
        is_valid = 1;
    }

    if (is_valid)
    {
        send_string(COLOR_SUCCESS);
    }
    else
    {
        send_string(COLOR_MUTED);
    }

    for (uint8_t i = 0; i < cmd_index; i++)
    {
        HAL_UART_Transmit(&huart4, (uint8_t*)&cmd_buffer[i], 1, 100);
    }
    send_string(COLOR_RESET);

    if (cursor_pos < cmd_index)
    {
        char move_cmd[10];
        sprintf(move_cmd, "\033[%dD", cmd_index - cursor_pos);
        send_string(move_cmd);
    }
}

/**
  * @brief Move cursor left
  */
void move_cursor_left(void)
{
    if (cursor_pos > 0)
    {
        cursor_pos--;
        send_string("\033[D");
    }
}

/**
  * @brief Move cursor right
  */
void move_cursor_right(void)
{
    if (cursor_pos < cmd_index)
    {
        cursor_pos++;
        send_string("\033[C");
    }
}

/**
  * @brief Insert character at cursor
  */
void insert_char_at_cursor(char c)
{
    if (cmd_index >= MAX_CMD_LENGTH) return;

    if (current_state != 2)
    {
        cmd_buffer[cmd_index] = c;
        cmd_index++;
        cursor_pos = cmd_index;

        // Password masking - show * instead of actual character for password input
        if (current_state == 1) {
            send_string("*");
        } else {
            HAL_UART_Transmit(&huart4, (uint8_t*)&c, 1, 100);
        }
        return;
    }

    // Shift characters to the right
    for (int i = cmd_index; i > cursor_pos; i--)
    {
        cmd_buffer[i] = cmd_buffer[i - 1];
    }

    cmd_buffer[cursor_pos] = c;
    cmd_index++;

    if (cursor_pos == cmd_index - 1)
    {
        // Inserting at end
        cursor_pos++;
        send_string(COLOR_MUTED);
        HAL_UART_Transmit(&huart4, (uint8_t*)&c, 1, 100);
        send_string(COLOR_RESET);
    }
    else
    {
        // Inserting in middle - redraw from insertion point
        redraw_from_cursor();
        cursor_pos++;
    }
}

/**
  * @brief Delete character at cursor
  */
void delete_char_at_cursor(void)
{
    if (cursor_pos == 0 || cmd_index == 0) return;

    for (uint8_t i = cursor_pos - 1; i < cmd_index - 1; i++)
    {
        cmd_buffer[i] = cmd_buffer[i + 1];
    }

    cmd_index--;
    cursor_pos--;

    send_string("\033[D");
    redraw_from_cursor();
}

/**
  * @brief Redraw from cursor position - FIXED VERSION
  */
void redraw_from_cursor(void)
{
    // Clear from current cursor position to end of line
    send_string("\033[K");

    // Print all characters from current cursor position to end
    for (uint8_t i = cursor_pos; i < cmd_index; i++)
    {
        if (current_state == 2)
        {
            send_string(COLOR_MUTED);
        }
        HAL_UART_Transmit(&huart4, (uint8_t*)&cmd_buffer[i], 1, 100);
        if (current_state == 2)
        {
            send_string(COLOR_RESET);
        }
    }

    // Move cursor back to the correct position
    if (cursor_pos < cmd_index)
    {
        char move_cmd[10];
        sprintf(move_cmd, "\033[%dD", cmd_index - cursor_pos);
        send_string(move_cmd);
    }
}

/**
  * @brief Add log entry
  */
void add_log(log_level_t level, const char* command, const char* message)
{
    system_logs[log_index].timestamp = system_tick;
    system_logs[log_index].level = level;
    strncpy((char*)system_logs[log_index].command, command, MAX_CMD_LENGTH);
    system_logs[log_index].command[MAX_CMD_LENGTH] = '\0';
    strncpy((char*)system_logs[log_index].message, message, 63);
    system_logs[log_index].message[63] = '\0';

    log_index = (log_index + 1) % LOG_SIZE;
    if (log_count < LOG_SIZE)
    {
        log_count++;
    }
}

/**
  * @brief Display system logs
  */
void show_logs(void)
{
    send_string(COLOR_INFO "System Logs:\r\n" COLOR_RESET);
    print_separator();

    if (log_count == 0)
    {
        send_string(COLOR_MUTED " No logs available\r\n" COLOR_RESET);
    }
    else
    {
        uint8_t start_idx = (log_index - log_count + LOG_SIZE) % LOG_SIZE;

        for (uint8_t i = 0; i < log_count; i++)
        {
            uint8_t idx = (start_idx + i) % LOG_SIZE;
            char log_line[120];

            uint32_t seconds = system_logs[idx].timestamp / 1000;
            uint32_t minutes = seconds / 60;
            uint32_t hours = minutes / 60;

            sprintf(log_line, "%s %02lu:%02lu:%02lu" COLOR_MUTED " %-7s %-10s %s" COLOR_RESET "\r\n",
                get_log_level_color(system_logs[idx].level),
                hours % 24, minutes % 60, seconds % 60,
                get_log_level_string(system_logs[idx].level),
                (char*)system_logs[idx].command,
                (char*)system_logs[idx].message);

            send_string(log_line);
        }
    }

    print_separator();
}

/**
  * @brief Print separator line
  */
void print_separator(void)
{
    send_string(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

/**
  * @brief Get log level string
  */
const char* get_log_level_string(log_level_t level)
{
    switch(level)
    {
        case LOG_INFO:    return "INFO";
        case LOG_SUCCESS: return "SUCCESS";
        case LOG_ERROR:   return "ERROR";
        case LOG_WARNING: return "WARN";
        case LOG_LOGIN:   return "LOGIN";
        default:          return "UNKNOWN";
    }
}

/**
  * @brief Get log level color
  */
const char* get_log_level_color(log_level_t level)
{
    switch(level)
    {
        case LOG_INFO:    return COLOR_INFO;
        case LOG_SUCCESS: return COLOR_SUCCESS;
        case LOG_ERROR:   return COLOR_ERROR;
        case LOG_WARNING: return COLOR_WARNING;
        case LOG_LOGIN:   return COLOR_ACCENT;
        default:          return COLOR_PRIMARY;
    }
}

/**
  * @brief Update real-time clock for Seoul timezone (UTC+9)
  */
void update_rtc(void)
{
    rtc_tick_counter++;
    if (rtc_tick_counter >= 1000) // 1 second
    {
        rtc_tick_counter = 0;
        current_time.seconds++;

        if (current_time.seconds >= 60)
        {
            current_time.seconds = 0;
            current_time.minutes++;

            if (current_time.minutes >= 60)
            {
                current_time.minutes = 0;
                current_time.hours++;

                if (current_time.hours >= 24)
                {
                    current_time.hours = 0;
                    current_time.day++;

                    // Simple month handling (30 days per month for simplicity)
                    if (current_time.day > 30)
                    {
                        current_time.day = 1;
                        current_time.month++;

                        if (current_time.month > 12)
                        {
                            current_time.month = 1;
                            current_time.year++;
                        }
                    }
                }
            }
        }
    }
}

/**
  * @brief Check session timeout
  */
void check_session_timeout(void)
{
    if (current_state == 2 && last_activity > 0)
    {
        if ((system_tick - last_activity) > SESSION_TIMEOUT_MS)
        {
            print_status("Session timeout - automatically logged out", COLOR_WARNING);
            current_state = 0;
            add_log(LOG_LOGIN, "timeout", "Session timeout");
            show_banner();
            send_string(COLOR_MUTED "login: " COLOR_RESET);
            last_activity = 0;
        }
    }
}

/**
  * @brief Format uptime string
  */
const char* format_uptime(uint32_t ms)
{
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
  * @brief Show system uptime
  */
void show_uptime(void)
{
    send_string(COLOR_INFO "System Uptime:\r\n" COLOR_RESET);
    char uptime_msg[80];
    sprintf(uptime_msg, COLOR_MUTED " Boot time: " COLOR_PRIMARY "%s ago\r\n", format_uptime(system_tick));
    send_string(uptime_msg);
    sprintf(uptime_msg, COLOR_MUTED " Clock time: " COLOR_PRIMARY "%02d:%02d:%02d (Day %d)\r\n",
        current_time.hours, current_time.minutes, current_time.seconds, current_time.day);
    send_string(uptime_msg);
}

/**
  * @brief Show detailed system information
  */
void show_system_info(void)
{
    send_string(COLOR_INFO "System Information:\r\n" COLOR_RESET);
    print_separator();

    send_string(COLOR_MUTED " MCU:        " COLOR_PRIMARY "STM32F767VIT6\r\n");
    send_string(COLOR_MUTED " Core:       " COLOR_PRIMARY "ARM Cortex-M7 @ 216MHz\r\n");
    send_string(COLOR_MUTED " Flash:      " COLOR_PRIMARY "2MB\r\n");
    send_string(COLOR_MUTED " RAM:        " COLOR_PRIMARY "512KB\r\n");
    send_string(COLOR_MUTED " Firmware:   " COLOR_PRIMARY "Terminal v2.0\r\n");

    char info_line[80];
    sprintf(info_line, COLOR_MUTED " Uptime:     " COLOR_PRIMARY "%s\r\n", format_uptime(system_tick));
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " Time:       " COLOR_PRIMARY "%02d:%02d:%02d (Day %d)\r\n",
        current_time.hours, current_time.minutes, current_time.seconds, current_time.day);
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " LEDs:       " COLOR_PRIMARY "1:%s 2:%s 3:%s\r\n",
        led_states[0] ? "ON" : "OFF",
        led_states[1] ? "ON" : "OFF",
        led_states[2] ? "ON" : "OFF");
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " Commands:   " COLOR_PRIMARY "%d executed\r\n", history_count);
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " Log entries:" COLOR_PRIMARY " %d\r\n", log_count);
    send_string(info_line);

    print_separator();
}

/* USER CODE END 4 */

/**
  * @brief  Error Handler
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif

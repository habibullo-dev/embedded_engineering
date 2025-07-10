/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F767 Professional Terminal with HDC1080 + Diagnostics (COMPLETE FIXED)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"
#include "i2c.h"

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

// HDC1080 Temperature/Humidity Sensor Configuration
#define HDC1080_ADDRESS     0x40 << 1  // I2C address (left shifted for HAL)
#define HDC1080_TEMP_REG    0x00       // Temperature register
#define HDC1080_HUMIDITY_REG 0x01      // Humidity register
#define HDC1080_CONFIG_REG  0x02       // Configuration register

// LED definitions
#define LED1_PIN GPIO_PIN_0
#define LED2_PIN GPIO_PIN_7
#define LED3_PIN GPIO_PIN_14
#define LED_PORT GPIOB

// Session timeout (5 minutes)
#define SESSION_TIMEOUT_MS 300000

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
    LOG_LOGIN,
    LOG_SENSOR,
    LOG_DEBUG
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

volatile rtc_time_t current_time = {9, 0, 0, 10, 7, 2025};  // Start at 09:00:00 Seoul time
volatile uint32_t rtc_tick_counter = 0;

// Session management
volatile uint32_t last_activity = 0;

// HDC1080 sensor data structure
typedef struct {
    float temperature;    // Temperature in Celsius
    float humidity;       // Relative Humidity in %
    uint8_t sensor_ok;    // Sensor status flag
} HDC1080_Data;

volatile HDC1080_Data climate_data = {0.0f, 0.0f, 0};

// External handles
extern I2C_HandleTypeDef hi2c2;  // I2C2 handle for HDC1080 sensor
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// UART functions
uint8_t buffer_has_data(void);
char get_char_from_buffer(void);
void process_character(char c);
void process_command(void);
void send_string(const char* str);
void show_banner(void);
void show_banner_with_current_climate(void);
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

// HDC1080 sensor functions (FIXED)
HAL_StatusTypeDef HDC1080_Init(void);
HAL_StatusTypeDef HDC1080_ReadData(HDC1080_Data* data);
HAL_StatusTypeDef HDC1080_ReadData_Combined(HDC1080_Data* data);
void HDC1080_UpdateData(void);
void force_climate_update(void);
const char* get_comfort_status(float temp, float humidity);

// Diagnostic functions
void I2C_Scan(void);
void I2C_Test_Config(void);
void HDC1080_Detailed_Test(void);
void HDC1080_Detailed_Test_Fixed(void);
void Simple_HDC1080_Test(void);
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
  MX_I2C2_Init();  // Initialize I2C2 for HDC1080

  /* USER CODE BEGIN 2 */
  show_banner_with_current_climate();  // Updated banner with live climate data
  send_string(COLOR_MUTED "login: " COLOR_RESET);

  __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
  NVIC_SetPriority(UART4_IRQn, 0);
  NVIC_EnableIRQ(UART4_IRQn);

  HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
  led_control_all(0);

  // Initialize HDC1080 sensor
  HAL_Delay(100); // power up
  if (HDC1080_Init() == HAL_OK) {
      add_log(LOG_SUCCESS, "hdc1080", "Temperature/Humidity sensor initialized");
      climate_data.sensor_ok = 1;
      HAL_Delay(50); // after config
      HDC1080_UpdateData();
  } else {
      add_log(LOG_ERROR, "hdc1080", "Sensor initialization failed");
      climate_data.sensor_ok = 0;
  }

  add_log(LOG_INFO, "system", "Terminal with climate monitoring ready");
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

    // Update sensor data periodically (every 5 seconds)
    static uint32_t last_sensor_update = 0;
    if ((system_tick - last_sensor_update) > 5000) {
        HDC1080_UpdateData();
        last_sensor_update = system_tick;
    }

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
 * @brief Initialize HDC1080 Temperature/Humidity Sensor
 * @retval HAL_StatusTypeDef - HAL_OK if successful
 */
HAL_StatusTypeDef HDC1080_Init(void)
{
    HAL_StatusTypeDef status;
    uint8_t config_data[2];

    // Step 1: Check if sensor is responding
    status = HAL_I2C_IsDeviceReady(&hi2c2, HDC1080_ADDRESS, 3, 1000);
    if (status != HAL_OK) {
        add_log(LOG_ERROR, "hdc1080", "Device not ready");
        return status;  // Sensor not responding
    }

    // Step 2: Configure sensor
    // Set acquisition mode: both temperature and humidity, 14-bit resolution
    config_data[0] = 0x10;  // MSB: Temperature + Humidity acquisition mode
    config_data[1] = 0x00;  // LSB: 14-bit resolution for both measurements

    status = HAL_I2C_Mem_Write(&hi2c2, HDC1080_ADDRESS, HDC1080_CONFIG_REG,
                              I2C_MEMADD_SIZE_8BIT, config_data, 2, 1000);

    if (status != HAL_OK) {
        add_log(LOG_ERROR, "hdc1080", "Config write failed");
        return status;
    }

    // Step 3: Wait for sensor to configure itself
    HAL_Delay(15);  // HDC1080 needs ~15ms to configure

    add_log(LOG_SUCCESS, "hdc1080", "Initialization complete");
    return status;
}

/**
 * @brief Read temperature and humidity from HDC1080 (CORRECTED VERSION)
 * @param data: Pointer to HDC1080_Data structure to store results
 * @retval HAL_StatusTypeDef - HAL_OK if successful
 */
HAL_StatusTypeDef HDC1080_ReadData(HDC1080_Data* data)
{
    // Add this line to reset the I2C bus before each measurement
    HAL_I2C_DeInit(&hi2c2);
    HAL_I2C_Init(&hi2c2);

    HAL_StatusTypeDef status;
    uint8_t temp_data[2], humid_data[2];
    uint16_t temp_raw, humid_raw;
    uint8_t reg_addr;

    // Step 1: Trigger temperature measurement by writing to temperature register
    reg_addr = HDC1080_TEMP_REG;
    status = HAL_I2C_Master_Transmit(&hi2c2, HDC1080_ADDRESS, &reg_addr, 1, 1000);
    if (status != HAL_OK) {
        data->sensor_ok = 0;
        add_log(LOG_WARNING, "hdc1080", "Temperature trigger failed");
        return status;
    }

    // Step 2: Wait for temperature conversion (6.35ms for 14-bit resolution)
    HAL_Delay(7);

    // Step 3: Read temperature data (2 bytes)
    status = HAL_I2C_Master_Receive(&hi2c2, HDC1080_ADDRESS, temp_data, 2, 1000);
    if (status != HAL_OK) {
        data->sensor_ok = 0;
        add_log(LOG_WARNING, "hdc1080", "Temperature read failed");
        return status;
    }

    // Step 4: Trigger humidity measurement
    reg_addr = HDC1080_HUMIDITY_REG;
    status = HAL_I2C_Master_Transmit(&hi2c2, HDC1080_ADDRESS, &reg_addr, 1, 1000);
    if (status != HAL_OK) {
        data->sensor_ok = 0;
        add_log(LOG_WARNING, "hdc1080", "Humidity trigger failed");
        return status;
    }

    // Step 5: Wait for humidity conversion
    HAL_Delay(7);

    // Step 6: Read humidity data (2 bytes)
    status = HAL_I2C_Master_Receive(&hi2c2, HDC1080_ADDRESS, humid_data, 2, 1000);
    if (status != HAL_OK) {
        data->sensor_ok = 0;
        add_log(LOG_WARNING, "hdc1080", "Humidity read failed");
        return status;
    }

    // Step 7: Convert raw data to physical values
    // HDC1080 uses big-endian format (MSB first)
    temp_raw = (temp_data[0] << 8) | temp_data[1];
    humid_raw = (humid_data[0] << 8) | humid_data[1];

    // Step 8: Apply conversion formulas from HDC1080 datasheet
    data->temperature = ((float)temp_raw / 65536.0f) * 165.0f - 40.0f;  // Convert to °C
    data->humidity = ((float)humid_raw / 65536.0f) * 100.0f;            // Convert to %RH
    data->sensor_ok = 1;

    return HAL_OK;
}

/**
 * @brief Alternative: Combined measurement mode (more efficient)
 * Configure HDC1080 to measure both temp+humidity with single trigger
 */
HAL_StatusTypeDef HDC1080_ReadData_Combined(HDC1080_Data* data)
{
    // Add this line to reset the I2C bus before each measurement
    HAL_I2C_DeInit(&hi2c2);
    HAL_I2C_Init(&hi2c2);
    HAL_Delay(5); // Short delay after bus reset

    HAL_StatusTypeDef status;
    uint8_t measurement_data[4];  // 2 bytes temp + 2 bytes humidity
    uint16_t temp_raw, humid_raw;
    uint8_t reg_addr;

    // Step 1: Trigger combined measurement (temp + humidity)
    reg_addr = HDC1080_TEMP_REG;  // Writing to temp reg triggers both when configured
    status = HAL_I2C_Master_Transmit(&hi2c2, HDC1080_ADDRESS, &reg_addr, 1, 1000);
    if (status != HAL_OK) {
        data->sensor_ok = 0;
        add_log(LOG_WARNING, "hdc1080", "Combined measurement trigger failed");
        return status;
    }

    // Step 2: Wait for both conversions (temp + humidity = ~12.7ms total)
    HAL_Delay(15);

    // Step 3: Read all 4 bytes (temp MSB, temp LSB, humidity MSB, humidity LSB)
    status = HAL_I2C_Master_Receive(&hi2c2, HDC1080_ADDRESS, measurement_data, 4, 1000);
    if (status != HAL_OK) {
        data->sensor_ok = 0;
        add_log(LOG_WARNING, "hdc1080", "Combined measurement read failed");
        return status;
    }

    // Step 4: Parse the data
    temp_raw = (measurement_data[0] << 8) | measurement_data[1];
    humid_raw = (measurement_data[2] << 8) | measurement_data[3];

    // Step 5: Convert to physical values
    data->temperature = ((float)temp_raw / 65536.0f) * 165.0f - 40.0f;
    data->humidity = ((float)humid_raw / 65536.0f) * 100.0f;
    data->sensor_ok = 1;

    return HAL_OK;
}

/**
 * @brief Update global climate data (called periodically)
 */
void HDC1080_UpdateData(void)
{
    int tries = 3;
    while (tries--) {
        // Use the combined measurement function instead
        if (HDC1080_ReadData_Combined((HDC1080_Data*)&climate_data) == HAL_OK) {
            climate_data.sensor_ok = 1;
            return;
        }
        HAL_Delay(20); // Wait a bit before retry
    }
    climate_data.sensor_ok = 0;
    add_log(LOG_WARNING, "hdc1080", "Periodic read failed");
}

/**
 * @brief Force update global climate data immediately
 */
void force_climate_update(void)
{

    HDC1080_Data fresh_data;
    // Use the combined measurement function instead
    if (HDC1080_ReadData_Combined(&fresh_data) == HAL_OK) {
        // Update global climate data
        climate_data.temperature = fresh_data.temperature;
        climate_data.humidity = fresh_data.humidity;
        climate_data.sensor_ok = 1;

        add_log(LOG_SUCCESS, "climate", "Global data updated");
    } else {
        climate_data.sensor_ok = 0;
        add_log(LOG_ERROR, "climate", "Global update failed");
    }
}

/**
 * @brief Get comfort zone status based on temperature and humidity
 * @param temp: Temperature in Celsius
 * @param humidity: Relative humidity in %
 * @retval Status string with color codes
 */
const char* get_comfort_status(float temp, float humidity)
{
    // Comfort zone: 20-25°C and 40-60% humidity
    if (temp >= 20.0f && temp <= 25.0f && humidity >= 40.0f && humidity <= 60.0f) {
        return COLOR_SUCCESS "Comfort Zone ✓" COLOR_RESET;
    }
    // Too hot
    else if (temp > 25.0f) {
        return COLOR_ERROR "Too Hot" COLOR_RESET;
    }
    // Too cold
    else if (temp < 20.0f) {
        return COLOR_INFO "Too Cold" COLOR_RESET;
    }
    // Temperature OK but humidity issues
    else if (humidity < 40.0f) {
        return COLOR_WARNING "Too Dry" COLOR_RESET;
    }
    else if (humidity > 60.0f) {
        return COLOR_WARNING "Too Humid" COLOR_RESET;
    }
    else {
        return COLOR_MUTED "Unknown" COLOR_RESET;
    }
}

/**
 * @brief Updated HDC1080 detailed test with correct protocol
 */
void HDC1080_Detailed_Test_Fixed(void)
{
    send_string(COLOR_INFO "HDC1080 Detailed Test (FIXED):\r\n" COLOR_RESET);
    print_separator();

    // Step 1: Check device presence
    send_string(COLOR_MUTED " Step 1: Device presence check...\r\n" COLOR_RESET);
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, HDC1080_ADDRESS, 3, 1000);

    if (status != HAL_OK) {
        send_string(COLOR_ERROR " ✗ HDC1080 NOT responding\r\n" COLOR_RESET);
        char error_msg[60];
        sprintf(error_msg, COLOR_ERROR "   HAL Error Code: %d", status);
        send_string(error_msg);
        if (status == HAL_TIMEOUT) {
            send_string(" (TIMEOUT)\r\n" COLOR_RESET);
            send_string(COLOR_WARNING "   → Check: Power (3.3V), wiring, connections\r\n" COLOR_RESET);
        } else if (status == HAL_ERROR) {
            send_string(" (ERROR)\r\n" COLOR_RESET);
            send_string(COLOR_WARNING "   → Check: SDA/SCL connections, I2C configuration\r\n" COLOR_RESET);
        }
        print_separator();
        return;
    }
    send_string(COLOR_SUCCESS " ✓ HDC1080 responds at address 0x40\r\n" COLOR_RESET);

    // Step 2: Test proper measurement sequence
    send_string(COLOR_MUTED " Step 2: Temperature measurement (correct protocol)...\r\n" COLOR_RESET);

    // Trigger temperature measurement
    uint8_t reg_addr = HDC1080_TEMP_REG;
    status = HAL_I2C_Master_Transmit(&hi2c2, HDC1080_ADDRESS, &reg_addr, 1, 1000);

    if (status != HAL_OK) {
        send_string(COLOR_ERROR " ✗ Temperature trigger failed\r\n" COLOR_RESET);
        char error_msg[50];
        sprintf(error_msg, COLOR_ERROR "   Error code: %d\r\n" COLOR_RESET, status);
        send_string(error_msg);
        print_separator();
        return;
    }
    send_string(COLOR_SUCCESS " ✓ Temperature measurement triggered\r\n" COLOR_RESET);

    // Wait for conversion
    send_string(COLOR_MUTED "   Waiting for conversion (7ms)...\r\n" COLOR_RESET);
    HAL_Delay(7);

    // Read temperature data
    uint8_t temp_data[2];
    status = HAL_I2C_Master_Receive(&hi2c2, HDC1080_ADDRESS, temp_data, 2, 1000);

    if (status != HAL_OK) {
        send_string(COLOR_ERROR " ✗ Temperature read failed\r\n" COLOR_RESET);
        char error_msg[50];
        sprintf(error_msg, COLOR_ERROR "   Error code: %d\r\n" COLOR_RESET, status);
        send_string(error_msg);
        print_separator();
        return;
    }

    uint16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
    float temperature = ((float)temp_raw / 65536.0f) * 165.0f - 40.0f;

    char temp_msg[80];
    sprintf(temp_msg, COLOR_SUCCESS " ✓ Temperature: %.2f°C (raw: 0x%04X)\r\n" COLOR_RESET,
           temperature, temp_raw);
    send_string(temp_msg);

    // Check if temperature is reasonable
    if (temperature > -10.0f && temperature < 60.0f) {
        send_string(COLOR_SUCCESS " ✓ Temperature reading looks reasonable\r\n" COLOR_RESET);
    } else {
        send_string(COLOR_WARNING " ⚠ Temperature reading seems unrealistic\r\n" COLOR_RESET);
        send_string(COLOR_MUTED "   (May be normal if sensor just powered up)\r\n" COLOR_RESET);
    }

    // Step 3: Test humidity measurement
    send_string(COLOR_MUTED " Step 3: Humidity measurement...\r\n" COLOR_RESET);

    reg_addr = HDC1080_HUMIDITY_REG;
    status = HAL_I2C_Master_Transmit(&hi2c2, HDC1080_ADDRESS, &reg_addr, 1, 1000);

    if (status != HAL_OK) {
        send_string(COLOR_ERROR " ✗ Humidity trigger failed\r\n" COLOR_RESET);
        char error_msg[50];
        sprintf(error_msg, COLOR_ERROR "   Error code: %d\r\n" COLOR_RESET, status);
        send_string(error_msg);
        print_separator();
        return;
    }

    send_string(COLOR_MUTED "   Waiting for conversion (7ms)...\r\n" COLOR_RESET);
    HAL_Delay(7);

    uint8_t humid_data[2];
    status = HAL_I2C_Master_Receive(&hi2c2, HDC1080_ADDRESS, humid_data, 2, 1000);

    if (status == HAL_OK) {
        uint16_t humid_raw = (humid_data[0] << 8) | humid_data[1];
        float humidity = ((float)humid_raw / 65536.0f) * 100.0f;

        char humid_msg[80];
        sprintf(humid_msg, COLOR_SUCCESS " ✓ Humidity: %.2f%% RH (raw: 0x%04X)\r\n" COLOR_RESET,
               humidity, humid_raw);
        send_string(humid_msg);

        if (humidity >= 0.0f && humidity <= 100.0f && temperature > -10.0f && temperature < 60.0f) {
            send_string(COLOR_SUCCESS " ✓ HDC1080 is working perfectly!\r\n" COLOR_RESET);
        } else {
            send_string(COLOR_WARNING " ⚠ Humidity reading out of range\r\n" COLOR_RESET);
        }

        // Step 4: Test combined measurement mode
        send_string(COLOR_MUTED " Step 4: Testing combined measurement mode...\r\n" COLOR_RESET);
        HDC1080_Data combined_data;
        if (HDC1080_ReadData_Combined(&combined_data) == HAL_OK) {
            char combined_msg[100];
            sprintf(combined_msg, COLOR_SUCCESS " ✓ Combined mode: %.2f°C, %.2f%% RH\r\n" COLOR_RESET,
                   combined_data.temperature, combined_data.humidity);
            send_string(combined_msg);
            send_string(COLOR_SUCCESS " ✓ All HDC1080 functions working perfectly!\r\n" COLOR_RESET);
        } else {
            send_string(COLOR_WARNING " ⚠ Combined mode failed, but individual readings work\r\n" COLOR_RESET);
        }

    } else {
        send_string(COLOR_ERROR " ✗ Humidity read failed\r\n" COLOR_RESET);
        char error_msg[50];
        sprintf(error_msg, COLOR_ERROR "   Error code: %d\r\n" COLOR_RESET, status);
        send_string(error_msg);
    }

    print_separator();
}

/**
 * @brief Simple HDC1080 test - now uses fixed communication (UPDATED)
 */
void Simple_HDC1080_Test(void)
{
    send_string("\r\n" COLOR_INFO "=== HDC1080 Quick Test (FIXED) ===\r\n" COLOR_RESET);

    // Test 1: Basic ping
    HAL_StatusTypeDef result = HAL_I2C_IsDeviceReady(&hi2c2, HDC1080_ADDRESS, 3, 1000);
    char test_msg[80];
    sprintf(test_msg, "Device Ready Test: %s\r\n", (result == HAL_OK) ? COLOR_SUCCESS "PASS" COLOR_RESET : COLOR_ERROR "FAIL" COLOR_RESET);
    send_string(test_msg);

    if (result != HAL_OK) {
        sprintf(test_msg, COLOR_ERROR "Error code: %d\r\n" COLOR_RESET, result);
        send_string(test_msg);
        send_string(COLOR_WARNING "Check: Power (3.3V), Wiring (PF0/PF1), I2C2 enabled\r\n" COLOR_RESET);
        return;
    }

    // Test 2: Read configuration register
    uint8_t data[2];
    result = HAL_I2C_Mem_Read(&hi2c2, HDC1080_ADDRESS, HDC1080_CONFIG_REG, I2C_MEMADD_SIZE_8BIT, data, 2, 1000);
    sprintf(test_msg, "Register Read Test: %s\r\n", (result == HAL_OK) ? COLOR_SUCCESS "PASS" COLOR_RESET : COLOR_ERROR "FAIL" COLOR_RESET);
    send_string(test_msg);

    if (result == HAL_OK) {
        send_string(COLOR_SUCCESS "SUCCESS! HDC1080 is working.\r\n" COLOR_RESET);
        sprintf(test_msg, COLOR_MUTED "Config register: 0x%02X%02X\r\n" COLOR_RESET, data[0], data[1]);
        send_string(test_msg);

        // Test 3: Try actual measurement using FIXED method
        send_string("Testing actual measurement (FIXED PROTOCOL)...\r\n");
        HDC1080_Data test_data;
        if (HDC1080_ReadData(&test_data) == HAL_OK) {
            sprintf(test_msg, COLOR_SUCCESS "Measurement: %.2f°C, %.2f%% RH\r\n" COLOR_RESET,
                   test_data.temperature, test_data.humidity);
            send_string(test_msg);
            send_string(COLOR_SUCCESS "✓ All tests PASSED! HDC1080 fully operational.\r\n" COLOR_RESET);
        } else {
            send_string(COLOR_WARNING "Measurement failed - check HDC1080_ReadData function\r\n" COLOR_RESET);
        }
    } else {
        send_string(COLOR_ERROR "Communication failed. Check wiring.\r\n" COLOR_RESET);
    }

    send_string("=======================================\r\n\r\n");
}

/**
 * @brief Scan I2C bus for connected devices
 */
void I2C_Scan(void)
{
    send_string(COLOR_INFO "Scanning I2C2 bus...\r\n" COLOR_RESET);
    print_separator();
    uint8_t devices_found = 0;

    for (uint8_t address = 1; address < 128; address++) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, address << 1, 1, 100);
        if (status == HAL_OK) {
            char msg[70];
            sprintf(msg, COLOR_SUCCESS " Device found at 0x%02X", address);
            send_string(msg);

            // Identify known devices
            if (address == 0x40) send_string(" (HDC1080 Temperature/Humidity)");
            else if (address == 0x53) send_string(" (ADXL345 Accelerometer)");
            else if (address == 0x68) send_string(" (MPU6050 or DS1307)");
            else if (address == 0x77) send_string(" (BMP280/BME280)");

            send_string(COLOR_RESET "\r\n");
            devices_found++;
        }
    }

    if (devices_found == 0) {
        send_string(COLOR_ERROR " No I2C devices found!\r\n" COLOR_RESET);
        send_string(COLOR_WARNING " Troubleshooting steps:\r\n" COLOR_RESET);
        send_string(COLOR_MUTED "   1. Check power connections (3.3V, not 5V!)\r\n");
        send_string(COLOR_MUTED "   2. Verify SDA/SCL wiring to PF0/PF1\r\n");
        send_string(COLOR_MUTED "   3. Ensure I2C2 is enabled in STM32CubeMX\r\n");
        send_string(COLOR_MUTED "   4. Check for loose connections\r\n");
    } else {
        char msg[50];
        sprintf(msg, COLOR_INFO " Total devices found: %d\r\n" COLOR_RESET, devices_found);
        send_string(msg);
    }
    print_separator();
}

/**
 * @brief Test I2C2 peripheral configuration
 */
void I2C_Test_Config(void)
{
    send_string(COLOR_INFO "I2C2 Configuration Test:\r\n" COLOR_RESET);
    print_separator();

    // Check if I2C2 is enabled
    if (__HAL_RCC_I2C2_IS_CLK_ENABLED()) {
        send_string(COLOR_SUCCESS " ✓ I2C2 Clock: Enabled\r\n" COLOR_RESET);
    } else {
        send_string(COLOR_ERROR " ✗ I2C2 Clock: DISABLED!\r\n" COLOR_RESET);
        send_string(COLOR_WARNING "   → Solution: Enable I2C2 in STM32CubeMX Connectivity\r\n" COLOR_RESET);
        print_separator();
        return;
    }

    // Check GPIO F clock
    if (__HAL_RCC_GPIOF_IS_CLK_ENABLED()) {
        send_string(COLOR_SUCCESS " ✓ GPIOF Clock: Enabled\r\n" COLOR_RESET);
    } else {
        send_string(COLOR_ERROR " ✗ GPIOF Clock: DISABLED!\r\n" COLOR_RESET);
        send_string(COLOR_WARNING "   → Solution: GPIO F pins not configured in CubeMX\r\n" COLOR_RESET);
    }

    // Check GPIO configuration
    GPIO_TypeDef* port_f = GPIOF;
    uint32_t pf0_mode = (port_f->MODER >> (0 * 2)) & 0x3;
    uint32_t pf1_mode = (port_f->MODER >> (1 * 2)) & 0x3;

    if (pf0_mode == 0x2 && pf1_mode == 0x2) {  // Alternate function mode
        send_string(COLOR_SUCCESS " ✓ GPIO PF0/PF1: Configured as alternate function\r\n" COLOR_RESET);
    } else {
        send_string(COLOR_ERROR " ✗ GPIO PF0/PF1: NOT configured properly!\r\n" COLOR_RESET);
        char mode_msg[80];
        sprintf(mode_msg, COLOR_MUTED "   PF0 mode: %lu, PF1 mode: %lu (should be 2)\r\n" COLOR_RESET, pf0_mode, pf1_mode);
        send_string(mode_msg);
        send_string(COLOR_WARNING "   → Solution: Set PF0=I2C2_SDA, PF1=I2C2_SCL in CubeMX\r\n" COLOR_RESET);
    }

    // Check alternate function configuration
    uint32_t pf0_af = (port_f->AFR[0] >> (0 * 4)) & 0xF;
    uint32_t pf1_af = (port_f->AFR[0] >> (1 * 4)) & 0xF;

    if (pf0_af == 4 && pf1_af == 4) {  // AF4 is I2C2 for PF0/PF1
        send_string(COLOR_SUCCESS " ✓ GPIO Alternate Function: I2C2 (AF4)\r\n" COLOR_RESET);
    } else {
        send_string(COLOR_ERROR " ✗ GPIO Alternate Function: NOT I2C2!\r\n" COLOR_RESET);
        char af_msg[80];
        sprintf(af_msg, COLOR_MUTED "   PF0 AF: %lu, PF1 AF: %lu (should be 4)\r\n" COLOR_RESET, pf0_af, pf1_af);
        send_string(af_msg);
    }

    // Test basic I2C functionality
    send_string(COLOR_INFO " Testing I2C2 basic operation...\r\n" COLOR_RESET);
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, 0x00, 1, 100);

    if (status == HAL_ERROR || status == HAL_TIMEOUT) {
        send_string(COLOR_SUCCESS " ✓ I2C2 peripheral: Working (no device at 0x00 - expected)\r\n" COLOR_RESET);
    } else {
        send_string(COLOR_WARNING " ⚠ I2C2 peripheral: Unexpected response\r\n" COLOR_RESET);
    }

    // Show I2C configuration details
    char config_msg[100];
    sprintf(config_msg, COLOR_MUTED " I2C2 Instance: 0x%08lX\r\n" COLOR_RESET, (uint32_t)hi2c2.Instance);
    send_string(config_msg);

    print_separator();
}

/**
 * @brief Test HDC1080 step by step (OLD VERSION - kept for reference)
 */
void HDC1080_Detailed_Test(void)
{
    send_string(COLOR_INFO "HDC1080 Detailed Test (OLD VERSION):\r\n" COLOR_RESET);
    print_separator();
    send_string(COLOR_WARNING "This test uses the old communication method.\r\n" COLOR_RESET);
    send_string(COLOR_INFO "Use 'sensortest' for the corrected version.\r\n" COLOR_RESET);
    print_separator();
}

/**
 * @brief UART RX Complete Callback
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart4)
    {
        rx_buffer[rx_head] = rx_char;
        rx_head = (rx_head + 1) % RX_BUFFER_SIZE;
        // Always re-arm the RX interrupt
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
        // Always reset input state after command
        memset(cmd_buffer, 0, sizeof(cmd_buffer));
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
    send_string("\r\n" COLOR_PROMPT "root" COLOR_MUTED "@" COLOR_PROMPT "stm32" COLOR_MUTED ":" COLOR_ACCENT "~" COLOR_MUTED "$ " COLOR_RESET);
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1); // Ensure RX interrupt is always re-armed
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
            force_climate_update();
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
            show_system_info();
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Status displayed");
        }
        else if (strcmp((char*)cmd_buffer, "uptime") == 0)
        {
            show_uptime();
            show_prompt();
            add_log(LOG_INFO, (char*)cmd_buffer, "Uptime displayed");
        }
        else if (strcmp((char*)cmd_buffer, "updateclimate") == 0)
        {
            force_climate_update();
            show_prompt();
            add_log(LOG_SENSOR, (char*)cmd_buffer, "Manual climate update");
        }
        else if (strcmp((char*)cmd_buffer, "climate") == 0)
        {
            // Show the global climate_data (periodically updated)
            send_string(COLOR_INFO "Climate Data:\r\n" COLOR_RESET);
            print_separator();
            if (climate_data.sensor_ok) {
                char msg[120];
                sprintf(msg, COLOR_MUTED " Temperature: " COLOR_PRIMARY "%.2f°C\r\n", climate_data.temperature);
                send_string(msg);
                sprintf(msg, COLOR_MUTED " Humidity:    " COLOR_PRIMARY "%.2f%% RH\r\n", climate_data.humidity);
                send_string(msg);
                sprintf(msg, COLOR_MUTED " Status:      %s\r\n",
                        get_comfort_status(climate_data.temperature, climate_data.humidity));
                send_string(msg);

                // Additional climate analysis
                if (climate_data.temperature > 30.0f) {
                    send_string(COLOR_WARNING " Warning: High temperature detected\r\n");
                } else if (climate_data.temperature < 10.0f) {
                    send_string(COLOR_INFO " Info: Low temperature detected\r\n");
                }

                if (climate_data.humidity > 70.0f) {
                    send_string(COLOR_WARNING " Warning: High humidity may cause condensation\r\n");
                } else if (climate_data.humidity < 30.0f) {
                    send_string(COLOR_INFO " Info: Low humidity may cause static electricity\r\n");
                }
            } else {
                send_string(COLOR_ERROR "Climate sensor offline or error\r\n");
                send_string(COLOR_WARNING "Try 'sensortest' for detailed diagnostics\r\n");
            }
            print_separator();
            show_prompt();
            add_log(LOG_SENSOR, (char*)cmd_buffer, "Climate data displayed");
        }
        else if (strcmp((char*)cmd_buffer, "i2cscan") == 0)
        {
            I2C_Scan();
            show_prompt();
            add_log(LOG_DEBUG, (char*)cmd_buffer, "I2C scan completed");
        }
        else if (strcmp((char*)cmd_buffer, "i2ctest") == 0)
        {
            I2C_Test_Config();
            show_prompt();
            add_log(LOG_DEBUG, (char*)cmd_buffer, "I2C config test completed");
        }
        else if (strcmp((char*)cmd_buffer, "sensortest") == 0)
        {
            HDC1080_Detailed_Test_Fixed();
            force_climate_update();
            show_prompt();
            add_log(LOG_DEBUG, (char*)cmd_buffer, "Sensor test completed");
        }
        else if (strcmp((char*)cmd_buffer, "quicktest") == 0)
        {
            Simple_HDC1080_Test();
            show_prompt();
            add_log(LOG_DEBUG, (char*)cmd_buffer, "Quick test completed");
        }
        else if (strcmp((char*)cmd_buffer, "logout") == 0)
        {
            print_status("Goodbye!", COLOR_WARNING);
            HAL_Delay(300);
            current_state = 0;
            add_log(LOG_LOGIN, (char*)cmd_buffer, "User logged out");
            show_banner_with_current_climate();
            send_string(COLOR_MUTED "login: " COLOR_RESET);
            return;
        }
        else if (strcmp((char*)cmd_buffer, "help") == 0)
        {
            send_string(COLOR_INFO "Available Commands:\r\n" COLOR_RESET);
            print_separator();
            send_string(COLOR_MUTED " " COLOR_ACCENT "Basic Commands:" COLOR_RESET "\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "whoami" COLOR_MUTED "           Show current user\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "clear" COLOR_MUTED "            Clear terminal\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "history" COLOR_MUTED "          Show command history\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "logs" COLOR_MUTED "             Show system logs\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "logout" COLOR_MUTED "           Exit session\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "help" COLOR_MUTED "             Show this help\r\n");
            send_string("\r\n" COLOR_MUTED " " COLOR_ACCENT "System Information:" COLOR_RESET "\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "status" COLOR_MUTED "           Show comprehensive system status\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "uptime" COLOR_MUTED "           Show system uptime\r\n");
            send_string("\r\n" COLOR_MUTED " " COLOR_ACCENT "LED Control:" COLOR_RESET "\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "led on|off N" COLOR_MUTED "     Control LED N (1-3)\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "led on|off all" COLOR_MUTED "   Control all LEDs\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "led on N -t SEC" COLOR_MUTED "  LED with timer (auto-off)\r\n");
            send_string("\r\n" COLOR_MUTED " " COLOR_ACCENT "Climate Monitoring:" COLOR_RESET "\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "climate" COLOR_MUTED "          Detailed temperature/humidity\r\n");
            send_string(COLOR_MUTED " " COLOR_ACCENT "updateclimate" COLOR_MUTED "    Force refresh global climate data\r\n");
            // Diagnostics and hardware info removed for cleaner help output
            print_separator();
            send_string(COLOR_WARNING "Session Management:\r\n" COLOR_RESET);
            send_string(COLOR_MUTED " Session timeout: 5 minutes of inactivity\r\n");
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
    // Always re-enable UART RX interrupt after command processing
    HAL_UART_Receive_IT(&huart4, (uint8_t*)&rx_char, 1);
}

/**
  * @brief Display startup banner with live climate data
  */
void show_banner_with_current_climate(void)
{
    send_string("\r\n");
    send_string(COLOR_MUTED "╭─────────────────────────────────────────╮\r\n");
    send_string("│ " COLOR_ACCENT "STM32F767" COLOR_MUTED " Professional Terminal         │\r\n");
    send_string("│ " COLOR_INFO "HDC1080" COLOR_MUTED " Climate Sensor • " COLOR_PRIMARY "I2C2" COLOR_MUTED "        │\r\n");
    send_string("│ " COLOR_SUCCESS "Secure Shell v2.1" COLOR_MUTED " with Diagnostics      │\r\n");
    send_string("╰─────────────────────────────────────────╯" COLOR_RESET "\r\n");

    // Try to get fresh climate data for banner
    HDC1080_Data current_climate;
    if (HDC1080_ReadData(&current_climate) == HAL_OK) {
        char climate_info[100];
        sprintf(climate_info, COLOR_MUTED "🌡️  Climate: %.1f°C, %.1f%% RH - %s\r\n",
               current_climate.temperature, current_climate.humidity,
               get_comfort_status(current_climate.temperature, current_climate.humidity));
        send_string(climate_info);

        // Update global data as well
        climate_data.temperature = current_climate.temperature;
        climate_data.humidity = current_climate.humidity;
        climate_data.sensor_ok = 1;
    }
    // Remove warning for cleaner
    send_string("\r\n");
}

/**
  * @brief Display original startup banner (backward compatibility)
  */
void show_banner(void)
{
    show_banner_with_current_climate();
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
        strncmp(temp_cmd, "climate", 7) == 0 ||
        strncmp(temp_cmd, "updateclimate", 13) == 0 ||
        strncmp(temp_cmd, "help", 4) == 0 ||
        strncmp(temp_cmd, "i2c", 3) == 0 ||
        strncmp(temp_cmd, "sensor", 6) == 0 ||
        strncmp(temp_cmd, "quick", 5) == 0)
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
  * @brief Insert character at cursor
  */
void insert_char_at_cursor(char c)
{
    if (cmd_index >= MAX_CMD_LENGTH) return;

    // Only mask password input
    if (current_state == 1) {
        cmd_buffer[cmd_index] = c;
        cmd_index++;
        cursor_pos = cmd_index;
        // Echo * for password, but also echo the character for visibility bugfix
        send_string("*");
        return;
    }

    // Shift right for insertion
    for (int i = cmd_index; i > cursor_pos; --i) {
        cmd_buffer[i] = cmd_buffer[i - 1];
    }
    cmd_buffer[cursor_pos] = c;
    cmd_index++;
    cursor_pos++;
    cmd_buffer[cmd_index] = '\0';
    // Echo the character immediately for user feedback
    HAL_UART_Transmit(&huart4, (uint8_t*)&c, 1, 100);
    // Redraw the rest of the line if not at the end
    if (cursor_pos < cmd_index) {
        redraw_from_cursor();
    }
}

/**
  * @brief Delete character at cursor
  */
void delete_char_at_cursor(void)
{
    if (cursor_pos == 0 || cmd_index == 0) return;
    // Shift left for deletion
    for (int i = cursor_pos - 1; i < cmd_index - 1; ++i) {
        cmd_buffer[i] = cmd_buffer[i + 1];
    }
    cmd_index--;
    cursor_pos--;
    cmd_buffer[cmd_index] = '\0';
    send_string("\033[D");
    redraw_from_cursor();
}

/**
  * @brief Move cursor left
  */
void move_cursor_left(void)
{
    if (cursor_pos > 0) {
        cursor_pos--;
        send_string("\033[D");
    }
}

/**
  * @brief Move cursor right
  */
void move_cursor_right(void)
{
    if (cursor_pos < cmd_index) {
        cursor_pos++;
        send_string("\033[C");
    }
}

/**
  * @brief Redraw from cursor position
  */
void redraw_from_cursor(void)
{
    send_string("\033[K");
    for (uint8_t i = cursor_pos; i < cmd_index; i++) {
        HAL_UART_Transmit(&huart4, (uint8_t*)&cmd_buffer[i], 1, 100);
    }
    // Move cursor back to original position
    if (cursor_pos < cmd_index) {
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
        case LOG_SENSOR:  return "SENSOR";
        case LOG_DEBUG:   return "DEBUG";
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
        case LOG_SENSOR:  return COLOR_SUCCESS;
        case LOG_DEBUG:   return COLOR_ACCENT;
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
            show_banner_with_current_climate();
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
  * @brief Show detailed system information with climate data
  */
void show_system_info(void)
{
    send_string(COLOR_INFO "System Information:\r\n" COLOR_RESET);
    print_separator();

    // Climate sensor status and data
    send_string(COLOR_MUTED " Climate Sensor:\r\n");
    if (climate_data.sensor_ok) {
        char climate_msg[120];
        sprintf(climate_msg, COLOR_MUTED "   HDC1080:     " COLOR_SUCCESS "Online" COLOR_MUTED " (I2C2: PF0/PF1)\r\n");
        send_string(climate_msg);

        sprintf(climate_msg, COLOR_MUTED "   Temperature: " COLOR_PRIMARY "%.1f°C\r\n", climate_data.temperature);
        send_string(climate_msg);

        sprintf(climate_msg, COLOR_MUTED "   Humidity:    " COLOR_PRIMARY "%.1f%% RH\r\n", climate_data.humidity);
        send_string(climate_msg);

        sprintf(climate_msg, COLOR_MUTED "   Status:      %s\r\n",
               get_comfort_status(climate_data.temperature, climate_data.humidity));
        send_string(climate_msg);
    } else {
        send_string(COLOR_MUTED "   HDC1080:     " COLOR_ERROR "Offline/Error\r\n");
        send_string(COLOR_WARNING "   → Use 'sensortest' for diagnostics\r\n");
    }

    // System information
    send_string(COLOR_MUTED " MCU:           " COLOR_PRIMARY "STM32F767VIT6\r\n");
    send_string(COLOR_MUTED " Core:          " COLOR_PRIMARY "ARM Cortex-M7 @ 216MHz\r\n");
    send_string(COLOR_MUTED " Flash:         " COLOR_PRIMARY "2MB\r\n");
    send_string(COLOR_MUTED " RAM:           " COLOR_PRIMARY "512KB\r\n");
    send_string(COLOR_MUTED " Firmware:      " COLOR_PRIMARY "Terminal v2.1 with Diagnostics (COMPLETE FIXED)\r\n");
    send_string(COLOR_MUTED " Interfaces:    " COLOR_PRIMARY "I2C2, UART4, USB OTG\r\n");

    char info_line[80];
    sprintf(info_line, COLOR_MUTED " Uptime:        " COLOR_PRIMARY "%s\r\n", format_uptime(system_tick));
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " Time:          " COLOR_PRIMARY "%02d:%02d:%02d (Day %d)\r\n",
        current_time.hours, current_time.minutes, current_time.seconds, current_time.day);
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " LEDs:          " COLOR_PRIMARY "1:%s 2:%s 3:%s\r\n",
        led_states[0] ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY,
        led_states[1] ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY,
        led_states[2] ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY);
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " Commands:      " COLOR_PRIMARY "%d executed\r\n", history_count);
    send_string(info_line);

    sprintf(info_line, COLOR_MUTED " Log entries:   " COLOR_PRIMARY "%d\r\n", log_count);
    send_string(info_line);

    print_separator();
}

/**
  * @brief System Clock Configuration
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

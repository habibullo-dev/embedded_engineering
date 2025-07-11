/* main.c - System Entry Point and Main Loop (Refactored Modular Version) */
#include "main.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"
#include "i2c.h"
#include <stdio.h>

// Application modules
#include "system_config.h"
#include "system_logging.h"
#include "led_control.h"
#include "sensors.h"
#include "terminal_ui.h"

// Global system variables
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

// Function prototypes
void SystemClock_Config(void);
void Error_Handler(void);

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
    MX_I2C2_Init();

    /* Application Initialization -----------------------------------------------*/

    // Initialize system services
    SystemLog_Init();
    SystemLog_Add(LOG_INFO, "main", "System services initialized");

    // Initialize hardware modules
    LED_Init();
    SystemLog_Add(LOG_SUCCESS, "main", "LED control initialized");

    // Initialize terminal interface
    TerminalUI_Init();
    SystemLog_Add(LOG_SUCCESS, "main", "Terminal UI initialized");

    // Initialize sensors (this may fail gracefully)
    if (Sensors_Init()) {
        SystemLog_Add(LOG_SUCCESS, "main", "Multi-sensor system ready");
    } else {
        SystemLog_Add(LOG_WARNING, "main", "Some sensors failed initialization");
    }

    // Show startup banner with live sensor data
    TerminalUI_ShowBanner();

    SystemLog_Add(LOG_INFO, "main", "System startup complete");

    /* Main System Loop -------------------------------------------------------*/
    uint32_t last_sensor_update = 0;
    uint32_t last_led_update = 0;
    uint32_t last_tick = 0;

    while (1)
    {
        // Process user interface
        TerminalUI_ProcessInput();

        // Update LED timers every 100ms
        if ((system_tick - last_led_update) >= LED_UPDATE_INTERVAL_MS) {
            LED_UpdateTimers();
            last_led_update = system_tick;
        }

        // Update sensors every 5 seconds
        if ((system_tick - last_sensor_update) >= SENSOR_UPDATE_INTERVAL_MS) {
            Sensors_UpdateAll();
            last_sensor_update = system_tick;
        }

        // Check for session timeout (if logged in)
        if (TerminalUI_IsLoggedIn()) {
            TerminalUI_CheckTimeout();
        }

        // Update system tick (1ms resolution)
        if (HAL_GetTick() != last_tick) {
            last_tick = HAL_GetTick();
            system_tick++;
        }
    }
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
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();
    SystemLog_Add(LOG_ERROR, "main", "System error - entering error handler");
    while (1)
    {
        // Error state - could implement error recovery here
    }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    SystemLog_Add(LOG_ERROR, "assert", "Assertion failed");
}
#endif /* USE_FULL_ASSERT */

/* Additional UI Functions for Terminal Module Support */

void TerminalUI_ShowSystemInfo(void) {
    const ClimateData_t* climate = Sensors_GetClimate();
    const AccelData_t* accel = Sensors_GetAccel();

    TerminalUI_SendString(COLOR_INFO "System Information:\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    // Multi-sensor status
    TerminalUI_SendString(COLOR_MUTED " Multi-Sensor System:\r\n");

    if (climate->sensor_ok) {
        char msg[120];
        sprintf(msg, COLOR_MUTED "   HDC1080:     " COLOR_SUCCESS "Online" COLOR_MUTED " (I2C2)\r\n");
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Temperature: " COLOR_PRIMARY "%.1f°C\r\n", climate->temperature);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Humidity:    " COLOR_PRIMARY "%.1f%% RH\r\n", climate->humidity);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Comfort:     %s\r\n", Sensors_GetComfortStatus());
        TerminalUI_SendString(msg);
    } else {
        TerminalUI_SendString(COLOR_MUTED "   HDC1080:     " COLOR_ERROR "Offline/Error\r\n");
    }

    if (accel->sensor_ok) {
        char msg[120];
        sprintf(msg, COLOR_MUTED "   ADXL345:     " COLOR_SUCCESS "Online" COLOR_MUTED " (I2C2)\r\n");
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Accel:       " COLOR_PRIMARY "%.3fg total\r\n", accel->magnitude);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Tilt:        " COLOR_PRIMARY "X=%.1f°, Y=%.1f°\r\n", accel->tilt_x, accel->tilt_y);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Orient:      %s\r\n", Sensors_GetOrientationStatus());
        TerminalUI_SendString(msg);
    } else {
        TerminalUI_SendString(COLOR_MUTED "   ADXL345:     " COLOR_ERROR "Offline/Error\r\n");
    }

    // System info
    TerminalUI_SendString(COLOR_MUTED " MCU:           " COLOR_PRIMARY "STM32F767VIT6\r\n");
    TerminalUI_SendString(COLOR_MUTED " Firmware:      " COLOR_PRIMARY SYSTEM_VERSION "\r\n");

    char info_line[150];  // Increased buffer size
    uint32_t seconds = system_tick / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    if (days > 0)
        sprintf(info_line, COLOR_MUTED " Uptime:        " COLOR_PRIMARY "%lu days, %02lu:%02lu:%02lu\r\n", days, hours % 24, minutes % 60, seconds % 60);
    else
        sprintf(info_line, COLOR_MUTED " Uptime:        " COLOR_PRIMARY "%02lu:%02lu:%02lu\r\n", hours % 24, minutes % 60, seconds % 60);
    TerminalUI_SendString(info_line);

    sprintf(info_line, COLOR_MUTED " LEDs:          " COLOR_PRIMARY "1:%s 2:%s 3:%s\r\n",
        LED_GetState(1) ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY,
        LED_GetState(2) ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY,
        LED_GetState(3) ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY);
    TerminalUI_SendString(info_line);

    sprintf(info_line, COLOR_MUTED " Log entries:   " COLOR_PRIMARY "%d\r\n", SystemLog_GetCount());
    TerminalUI_SendString(info_line);

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void TerminalUI_ShowUptime(void) {
    TerminalUI_SendString(COLOR_INFO "System Uptime:\r\n" COLOR_RESET);

    uint32_t seconds = system_tick / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    char uptime_msg[80];
    if (days > 0)
        sprintf(uptime_msg, COLOR_MUTED " Boot time: " COLOR_PRIMARY "%lu days, %02lu:%02lu:%02lu ago\r\n", days, hours % 24, minutes % 60, seconds % 60);
    else
        sprintf(uptime_msg, COLOR_MUTED " Boot time: " COLOR_PRIMARY "%02lu:%02lu:%02lu ago\r\n", hours % 24, minutes % 60, seconds % 60);
    TerminalUI_SendString(uptime_msg);
}

void TerminalUI_ShowAllSensors(void) {
    const ClimateData_t* climate = Sensors_GetClimate();
    const AccelData_t* accel = Sensors_GetAccel();

    TerminalUI_SendString(COLOR_INFO "All Sensors Status:\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    // Climate sensor section
    TerminalUI_SendString(COLOR_ACCENT "🌡️  Climate Sensor (HDC1080):\r\n" COLOR_RESET);
    if (climate->sensor_ok) {
        char msg[120];
        sprintf(msg, COLOR_MUTED "   Temperature: " COLOR_PRIMARY "%.2f°C\r\n", climate->temperature);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Humidity:    " COLOR_PRIMARY "%.2f%% RH\r\n", climate->humidity);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Status:      %s\r\n", Sensors_GetComfortStatus());
        TerminalUI_SendString(msg);
    } else {
        TerminalUI_SendString(COLOR_ERROR "   Status: Offline/Error\r\n" COLOR_RESET);
    }

    TerminalUI_SendString("\r\n");

    // Accelerometer section
    TerminalUI_SendString(COLOR_ACCENT "📐 Accelerometer (ADXL345):\r\n" COLOR_RESET);
    if (accel->sensor_ok) {
        char msg[120];
        sprintf(msg, COLOR_MUTED "   X-axis:      " COLOR_PRIMARY "%.3fg (%.1f°)\r\n", accel->x_g, accel->tilt_x);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Y-axis:      " COLOR_PRIMARY "%.3fg (%.1f°)\r\n", accel->y_g, accel->tilt_y);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Z-axis:      " COLOR_PRIMARY "%.3fg\r\n", accel->z_g);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Magnitude:   " COLOR_PRIMARY "%.3fg\r\n", accel->magnitude);
        TerminalUI_SendString(msg);
        sprintf(msg, COLOR_MUTED "   Orientation: %s\r\n", Sensors_GetOrientationStatus());
        TerminalUI_SendString(msg);
    } else {
        TerminalUI_SendString(COLOR_ERROR "   Status: Offline/Error\r\n" COLOR_RESET);
    }

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void TerminalUI_ShowDetailedAccel(void) {
    const AccelData_t* accel = Sensors_GetAccel();

    TerminalUI_SendString(COLOR_INFO "Detailed Accelerometer Data:\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    if (!accel->sensor_ok) {
        TerminalUI_SendString(COLOR_ERROR "Accelerometer offline or error\r\n");
        TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
        return;
    }

    // Force fresh reading
    Sensors_UpdateAccel();
    accel = Sensors_GetAccel();

    char msg[150];

    TerminalUI_SendString(COLOR_ACCENT "📊 Raw Data:\r\n" COLOR_RESET);
    sprintf(msg, COLOR_MUTED "   X-axis: " COLOR_PRIMARY "%d LSB" COLOR_MUTED " → " COLOR_PRIMARY "%.3fg\r\n", accel->x_raw, accel->x_g);
    TerminalUI_SendString(msg);
    sprintf(msg, COLOR_MUTED "   Y-axis: " COLOR_PRIMARY "%d LSB" COLOR_MUTED " → " COLOR_PRIMARY "%.3fg\r\n", accel->y_raw, accel->y_g);
    TerminalUI_SendString(msg);
    sprintf(msg, COLOR_MUTED "   Z-axis: " COLOR_PRIMARY "%d LSB" COLOR_MUTED " → " COLOR_PRIMARY "%.3fg\r\n", accel->z_raw, accel->z_g);
    TerminalUI_SendString(msg);

    TerminalUI_SendString("\r\n" COLOR_ACCENT "📐 Orientation Analysis:\r\n" COLOR_RESET);
    sprintf(msg, COLOR_MUTED "   X-Tilt:    " COLOR_PRIMARY "%.1f°\r\n", accel->tilt_x);
    TerminalUI_SendString(msg);
    sprintf(msg, COLOR_MUTED "   Y-Tilt:    " COLOR_PRIMARY "%.1f°\r\n", accel->tilt_y);
    TerminalUI_SendString(msg);
    sprintf(msg, COLOR_MUTED "   Magnitude: " COLOR_PRIMARY "%.3fg\r\n", accel->magnitude);
    TerminalUI_SendString(msg);
    sprintf(msg, COLOR_MUTED "   Status:    %s\r\n", Sensors_GetOrientationStatus());
    TerminalUI_SendString(msg);

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void TerminalUI_I2CScan(void) {
    TerminalUI_SendString(COLOR_INFO "Scanning I2C2 bus...\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
    uint8_t devices_found = 0;

    for (uint8_t address = 1; address < 128; address++) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, address << 1, 1, 100);
        if (status == HAL_OK) {
            char msg[70];
            sprintf(msg, COLOR_SUCCESS " Device found at 0x%02X", address);
            TerminalUI_SendString(msg);

            if (address == 0x40) TerminalUI_SendString(" (HDC1080 Temperature/Humidity)");
            else if (address == 0x53) TerminalUI_SendString(" (ADXL345 Accelerometer)");
            else if (address == 0x68) TerminalUI_SendString(" (MPU6050 or DS1307)");
            else if (address == 0x77) TerminalUI_SendString(" (BMP280/BME280)");

            TerminalUI_SendString(COLOR_RESET "\r\n");
            devices_found++;
        }
    }

    if (devices_found == 0) {
        TerminalUI_SendString(COLOR_ERROR " No I2C devices found!\r\n" COLOR_RESET);
    } else {
        char msg[50];
        sprintf(msg, COLOR_INFO " Total devices found: %d\r\n" COLOR_RESET, devices_found);
        TerminalUI_SendString(msg);
    }
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void TerminalUI_I2CTest(void) {
    TerminalUI_SendString(COLOR_INFO "I2C2 Configuration Test:\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    if (__HAL_RCC_I2C2_IS_CLK_ENABLED()) {
        TerminalUI_SendString(COLOR_SUCCESS " ✓ I2C2 Clock: Enabled\r\n" COLOR_RESET);
    } else {
        TerminalUI_SendString(COLOR_ERROR " ✗ I2C2 Clock: DISABLED!\r\n" COLOR_RESET);
    }

    if (__HAL_RCC_GPIOF_IS_CLK_ENABLED()) {
        TerminalUI_SendString(COLOR_SUCCESS " ✓ GPIOF Clock: Enabled\r\n" COLOR_RESET);
    } else {
        TerminalUI_SendString(COLOR_ERROR " ✗ GPIOF Clock: DISABLED!\r\n" COLOR_RESET);
    }

    TerminalUI_SendString(COLOR_INFO " Testing I2C2 basic operation...\r\n" COLOR_RESET);
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, 0x00, 1, 100);

    if (status == HAL_ERROR || status == HAL_TIMEOUT) {
        TerminalUI_SendString(COLOR_SUCCESS " ✓ I2C2 peripheral: Working\r\n" COLOR_RESET);
    } else {
        TerminalUI_SendString(COLOR_WARNING " ⚠ I2C2 peripheral: Unexpected response\r\n" COLOR_RESET);
    }

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

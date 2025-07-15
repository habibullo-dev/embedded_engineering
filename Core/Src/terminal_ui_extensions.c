/* terminal_ui_extensions.c - Additional UI Functions for FreeRTOS Terminal */
#include "terminal_ui.h"
#include "system_config.h"
#include "system_logging.h"
#include "sensors.h"
#include "led_control.h"
#include "freertos_globals.h"  // For global FreeRTOS objects
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

// External references
extern I2C_HandleTypeDef hi2c2;

// System info display function
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
    TerminalUI_SendString(COLOR_MUTED " OS:            " COLOR_PRIMARY "FreeRTOS v10.x\r\n");

    // FreeRTOS uptime using tick count
    char info_line[150];
    TickType_t uptimeTicks = xTaskGetTickCount();
    uint32_t uptimeMs = uptimeTicks * portTICK_PERIOD_MS;
    uint32_t seconds = uptimeMs / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    if (days > 0)
        sprintf(info_line, COLOR_MUTED " Uptime:        " COLOR_PRIMARY "%lu days, %02lu:%02lu:%02lu\r\n",
                days, hours % 24, minutes % 60, seconds % 60);
    else
        sprintf(info_line, COLOR_MUTED " Uptime:        " COLOR_PRIMARY "%02lu:%02lu:%02lu\r\n",
                hours % 24, minutes % 60, seconds % 60);
    TerminalUI_SendString(info_line);

    sprintf(info_line, COLOR_MUTED " LEDs:          " COLOR_PRIMARY "1:%s 2:%s 3:%s\r\n",
        LED_GetState(1) ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY,
        LED_GetState(2) ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY,
        LED_GetState(3) ? COLOR_SUCCESS "ON" COLOR_PRIMARY : COLOR_MUTED "OFF" COLOR_PRIMARY);
    TerminalUI_SendString(info_line);

    sprintf(info_line, COLOR_MUTED " Log entries:   " COLOR_PRIMARY "%d\r\n", SystemLog_GetCount());
    TerminalUI_SendString(info_line);

    // FreeRTOS specific info
    size_t freeHeap = xPortGetFreeHeapSize();
    size_t minFreeHeap = xPortGetMinimumEverFreeHeapSize();
    sprintf(info_line, COLOR_MUTED " Free Heap:     " COLOR_PRIMARY "%d" COLOR_MUTED " bytes (min: " COLOR_PRIMARY "%d" COLOR_MUTED ")\r\n",
            freeHeap, minFreeHeap);
    TerminalUI_SendString(info_line);

    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    sprintf(info_line, COLOR_MUTED " Active Tasks:  " COLOR_PRIMARY "%lu\r\n", (unsigned long)taskCount);
    TerminalUI_SendString(info_line);

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

void TerminalUI_ShowUptime(void) {
    TerminalUI_SendString(COLOR_INFO "System Uptime:\r\n" COLOR_RESET);

    TickType_t uptimeTicks = xTaskGetTickCount();
    uint32_t uptimeMs = uptimeTicks * portTICK_PERIOD_MS;
    uint32_t seconds = uptimeMs / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    char uptime_msg[80];
    if (days > 0)
        sprintf(uptime_msg, COLOR_MUTED " Boot time: " COLOR_PRIMARY "%lu days, %02lu:%02lu:%02lu ago\r\n",
                days, hours % 24, minutes % 60, seconds % 60);
    else
        sprintf(uptime_msg, COLOR_MUTED " Boot time: " COLOR_PRIMARY "%02lu:%02lu:%02lu ago\r\n",
                hours % 24, minutes % 60, seconds % 60);
    TerminalUI_SendString(uptime_msg);

    // Additional FreeRTOS timing info
    sprintf(uptime_msg, COLOR_MUTED " System ticks: " COLOR_PRIMARY "%lu\r\n", uptimeTicks);
    TerminalUI_SendString(uptime_msg);
    sprintf(uptime_msg, COLOR_MUTED " Tick period:  " COLOR_PRIMARY "%lu ms\r\n", portTICK_PERIOD_MS);
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

        // Show last update time
        TickType_t currentTick = xTaskGetTickCount();
        uint32_t timeSinceUpdate = (currentTick - climate->last_update) * portTICK_PERIOD_MS;
        sprintf(msg, COLOR_MUTED "   Last update: " COLOR_PRIMARY "%lu ms ago\r\n", timeSinceUpdate);
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

        // Show last update time
        TickType_t currentTick = xTaskGetTickCount();
        uint32_t timeSinceUpdate = (currentTick - accel->last_update) * portTICK_PERIOD_MS;
        sprintf(msg, COLOR_MUTED "   Last update: " COLOR_PRIMARY "%lu ms ago\r\n", timeSinceUpdate);
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

    // Thread-safe I2C scanning
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
        xSemaphoreGive(i2cMutex);
    } else {
        TerminalUI_SendString(COLOR_ERROR " I2C bus timeout - scan aborted\r\n" COLOR_RESET);
        TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
        return;
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

    // Thread-safe I2C test
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, 0x00, 1, 100);
        xSemaphoreGive(i2cMutex);

        if (status == HAL_ERROR || status == HAL_TIMEOUT) {
            TerminalUI_SendString(COLOR_SUCCESS " ✓ I2C2 peripheral: Working\r\n" COLOR_RESET);
        } else {
            TerminalUI_SendString(COLOR_WARNING " ⚠ I2C2 peripheral: Unexpected response\r\n" COLOR_RESET);
        }
    } else {
        TerminalUI_SendString(COLOR_ERROR " ✗ I2C2 mutex timeout\r\n" COLOR_RESET);
    }

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);
}

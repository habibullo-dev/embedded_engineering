/* sensors.c - Multi-Sensor Management Implementation */
#include "sensors.h"
#include "system_logging.h"
#include "system_config.h"
#include "terminal_ui.h"  // Add this include for TerminalUI_SendString
#include <stdio.h>
#include <math.h>

// Private sensor data
static ClimateData_t climate_data = {0.0f, 0.0f, 0, 0};
static AccelData_t accel_data = {0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0};

// External references
extern I2C_HandleTypeDef hi2c2;
extern volatile uint32_t system_tick;

// Private constants
#define HDC1080_ADDRESS     0x40 << 1
#define HDC1080_TEMP_REG    0x00
#define HDC1080_HUMIDITY_REG 0x01
#define HDC1080_CONFIG_REG  0x02

#define ADXL345_ADDRESS     0x53 << 1
#define ADXL345_DEVID_REG   0x00
#define ADXL345_POWER_CTL   0x2D
#define ADXL345_DATA_FORMAT 0x31
#define ADXL345_DATAX0      0x32
#define ADXL345_DEVICE_ID   0xE5
#define ADXL345_MEASURE_MODE 0x08
#define ADXL345_RANGE_2G    0x00

// Private function declarations
static uint8_t hdc1080_init(void);
static uint8_t adxl345_init(void);
static uint8_t hdc1080_read_data(void);
static uint8_t adxl345_read_data(void);
static float calculate_tilt_angle(float accel_axis, float accel_z);

// Public function implementations
uint8_t Sensors_Init(void) {
    uint8_t climate_ok = hdc1080_init();
    uint8_t accel_ok = adxl345_init();

    if (climate_ok) {
        SystemLog_Add(LOG_SUCCESS, "sensors", "HDC1080 initialized");
        Sensors_UpdateClimate();
    } else {
        SystemLog_Add(LOG_ERROR, "sensors", "HDC1080 init failed");
    }

    if (accel_ok) {
        SystemLog_Add(LOG_SUCCESS, "sensors", "ADXL345 initialized");
        Sensors_UpdateAccel();
    } else {
        SystemLog_Add(LOG_ERROR, "sensors", "ADXL345 init failed");
    }

    return (climate_ok || accel_ok);
}

uint8_t Sensors_UpdateAll(void) {
    uint8_t climate_result = Sensors_UpdateClimate();
    uint8_t accel_result = Sensors_UpdateAccel();
    return (climate_result || accel_result);
}

uint8_t Sensors_UpdateClimate(void) {
    if (hdc1080_read_data()) {
        climate_data.last_update = system_tick;
        return 1;
    }
    return 0;
}

uint8_t Sensors_UpdateAccel(void) {
    if (adxl345_read_data()) {
        accel_data.last_update = system_tick;
        return 1;
    }
    return 0;
}

const ClimateData_t* Sensors_GetClimate(void) {
    return &climate_data;
}

const AccelData_t* Sensors_GetAccel(void) {
    return &accel_data;
}

uint8_t Sensors_GetSystemStatus(void) {
    return (climate_data.sensor_ok || accel_data.sensor_ok);
}

const char* Sensors_GetComfortStatus(void) {
    if (!climate_data.sensor_ok) return COLOR_ERROR "Offline" COLOR_RESET;

    float temp = climate_data.temperature;
    float humidity = climate_data.humidity;

    if (temp >= 20.0f && temp <= 25.0f && humidity >= 40.0f && humidity <= 60.0f) {
        return COLOR_SUCCESS "Comfort Zone ✓" COLOR_RESET;
    } else if (temp > 25.0f) {
        return COLOR_ERROR "Too Hot" COLOR_RESET;
    } else if (temp < 20.0f) {
        return COLOR_INFO "Too Cold" COLOR_RESET;
    } else if (humidity < 40.0f) {
        return COLOR_WARNING "Too Dry" COLOR_RESET;
    } else if (humidity > 60.0f) {
        return COLOR_WARNING "Too Humid" COLOR_RESET;
    }
    return COLOR_MUTED "Unknown" COLOR_RESET;
}

const char* Sensors_GetOrientationStatus(void) {
    if (!accel_data.sensor_ok) return COLOR_ERROR "Offline" COLOR_RESET;

    float abs_x = fabsf(accel_data.x_g);
    float abs_y = fabsf(accel_data.y_g);
    float abs_z = fabsf(accel_data.z_g);

    if (abs_z > 0.8f && abs_x < 0.3f && abs_y < 0.3f) {
        if (accel_data.z_g > 0) {
            return COLOR_SUCCESS "Level (Face Up)" COLOR_RESET;
        } else {
            return COLOR_INFO "Level (Face Down)" COLOR_RESET;
        }
    } else if (abs_x > 0.7f) {
        return COLOR_WARNING "Tilted X-axis" COLOR_RESET;
    } else if (abs_y > 0.7f) {
        return COLOR_WARNING "Tilted Y-axis" COLOR_RESET;
    } else if (accel_data.magnitude > 1.5f) {
        return COLOR_ERROR "Motion/Vibration" COLOR_RESET;
    }
    return COLOR_MUTED "Tilted" COLOR_RESET;
}

void Sensors_RunAllTests(void) {
    TerminalUI_SendString(COLOR_INFO "=== Comprehensive Sensor Test ===\r\n" COLOR_RESET);
    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    // Test HDC1080
    TerminalUI_SendString(COLOR_ACCENT "Testing HDC1080 (Climate)...\r\n" COLOR_RESET);
    if (hdc1080_read_data()) {
        char msg[100];
        sprintf(msg, COLOR_SUCCESS "✓ HDC1080: %.2f°C, %.2f%% RH\r\n" COLOR_RESET,
               climate_data.temperature, climate_data.humidity);
        TerminalUI_SendString(msg);
    } else {
        TerminalUI_SendString(COLOR_ERROR "✗ HDC1080: Communication failed\r\n" COLOR_RESET);
    }

    TerminalUI_SendString("\r\n");

    // Test ADXL345
    TerminalUI_SendString(COLOR_ACCENT "Testing ADXL345 (Accelerometer)...\r\n" COLOR_RESET);
    if (adxl345_read_data()) {
        char msg[120];
        sprintf(msg, COLOR_SUCCESS "✓ ADXL345: X=%.3fg, Y=%.3fg, Z=%.3fg\r\n" COLOR_RESET,
               accel_data.x_g, accel_data.y_g, accel_data.z_g);
        TerminalUI_SendString(msg);
    } else {
        TerminalUI_SendString(COLOR_ERROR "✗ ADXL345: Communication failed\r\n" COLOR_RESET);
    }

    TerminalUI_SendString(COLOR_MUTED "───────────────────────────────────────────\r\n" COLOR_RESET);

    if (climate_data.sensor_ok && accel_data.sensor_ok) {
        TerminalUI_SendString(COLOR_SUCCESS "🎉 ALL SENSORS OPERATIONAL!\r\n" COLOR_RESET);
    } else if (climate_data.sensor_ok || accel_data.sensor_ok) {
        TerminalUI_SendString(COLOR_WARNING "⚠ PARTIAL SENSOR FUNCTIONALITY\r\n" COLOR_RESET);
    } else {
        TerminalUI_SendString(COLOR_ERROR "❌ NO SENSORS RESPONDING\r\n" COLOR_RESET);
    }
}

// Private function implementations
static uint8_t hdc1080_init(void) {
    HAL_StatusTypeDef status;
    uint8_t config_data[2];

    status = HAL_I2C_IsDeviceReady(&hi2c2, HDC1080_ADDRESS, 3, 1000);
    if (status != HAL_OK) return 0;

    config_data[0] = 0x10;
    config_data[1] = 0x00;
    status = HAL_I2C_Mem_Write(&hi2c2, HDC1080_ADDRESS, HDC1080_CONFIG_REG,
                              I2C_MEMADD_SIZE_8BIT, config_data, 2, 1000);
    if (status != HAL_OK) return 0;

    HAL_Delay(15);
    climate_data.sensor_ok = 1;
    return 1;
}

static uint8_t adxl345_init(void) {
    HAL_StatusTypeDef status;
    uint8_t device_id, config_data;

    status = HAL_I2C_IsDeviceReady(&hi2c2, ADXL345_ADDRESS, 3, 1000);
    if (status != HAL_OK) return 0;

    status = HAL_I2C_Mem_Read(&hi2c2, ADXL345_ADDRESS, ADXL345_DEVID_REG,
                             I2C_MEMADD_SIZE_8BIT, &device_id, 1, 1000);
    if (status != HAL_OK || device_id != ADXL345_DEVICE_ID) return 0;

    config_data = ADXL345_RANGE_2G | 0x08;
    status = HAL_I2C_Mem_Write(&hi2c2, ADXL345_ADDRESS, ADXL345_DATA_FORMAT,
                              I2C_MEMADD_SIZE_8BIT, &config_data, 1, 1000);
    if (status != HAL_OK) return 0;

    config_data = ADXL345_MEASURE_MODE;
    status = HAL_I2C_Mem_Write(&hi2c2, ADXL345_ADDRESS, ADXL345_POWER_CTL,
                              I2C_MEMADD_SIZE_8BIT, &config_data, 1, 1000);
    if (status != HAL_OK) return 0;

    HAL_Delay(50);
    accel_data.sensor_ok = 1;
    return 1;
}

static uint8_t hdc1080_read_data(void) {
    // Use combined measurement method from working code
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
        climate_data.sensor_ok = 0;
        return 0;
    }

    // Step 2: Wait for both conversions (temp + humidity = ~12.7ms total)
    HAL_Delay(15);

    // Step 3: Read all 4 bytes (temp MSB, temp LSB, humidity MSB, humidity LSB)
    status = HAL_I2C_Master_Receive(&hi2c2, HDC1080_ADDRESS, measurement_data, 4, 1000);
    if (status != HAL_OK) {
        climate_data.sensor_ok = 0;
        return 0;
    }

    // Step 4: Parse the data
    temp_raw = (measurement_data[0] << 8) | measurement_data[1];
    humid_raw = (measurement_data[2] << 8) | measurement_data[3];

    // Step 5: Convert to physical values
    climate_data.temperature = ((float)temp_raw / 65536.0f) * 165.0f - 40.0f;
    climate_data.humidity = ((float)humid_raw / 65536.0f) * 100.0f;
    climate_data.sensor_ok = 1;

    return 1;
}

static uint8_t adxl345_read_data(void) {
    HAL_StatusTypeDef status;
    uint8_t raw_data[6];

    status = HAL_I2C_Mem_Read(&hi2c2, ADXL345_ADDRESS, ADXL345_DATAX0,
                             I2C_MEMADD_SIZE_8BIT, raw_data, 6, 1000);

    if (status != HAL_OK) { accel_data.sensor_ok = 0; return 0; }

    accel_data.x_raw = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    accel_data.y_raw = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    accel_data.z_raw = (int16_t)((raw_data[5] << 8) | raw_data[4]);

    accel_data.x_g = (float)accel_data.x_raw / 256.0f;
    accel_data.y_g = (float)accel_data.y_raw / 256.0f;
    accel_data.z_g = (float)accel_data.z_raw / 256.0f;

    accel_data.magnitude = sqrtf(accel_data.x_g * accel_data.x_g +
                                accel_data.y_g * accel_data.y_g +
                                accel_data.z_g * accel_data.z_g);

    accel_data.tilt_x = calculate_tilt_angle(accel_data.x_g, accel_data.z_g);
    accel_data.tilt_y = calculate_tilt_angle(accel_data.y_g, accel_data.z_g);
    accel_data.sensor_ok = 1;

    return 1;
}

static float calculate_tilt_angle(float accel_axis, float accel_z) {
    if (accel_z == 0.0f) {
        return (accel_axis >= 0) ? 90.0f : -90.0f;
    }

    float angle_rad = atanf(accel_axis / accel_z);
    float angle_deg = angle_rad * 180.0f / M_PI;

    if (accel_z < 0) {
        angle_deg = (angle_deg >= 0) ? (angle_deg - 180.0f) : (angle_deg + 180.0f);
    }

    return angle_deg;  // This was missing - function needs to return the calculated angle
}

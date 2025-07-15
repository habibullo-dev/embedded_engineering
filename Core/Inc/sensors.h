/* sensors.h - Multi-Sensor Management Interface - FreeRTOS Version */
#ifndef SENSORS_H
#define SENSORS_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

// Sensor data structures
typedef struct {
    float temperature;          // Temperature in Celsius
    float humidity;             // Relative humidity in percentage
    uint8_t sensor_ok;          // Sensor status flag
    TickType_t last_update;     // FreeRTOS timestamp of last successful reading
} ClimateData_t;

typedef struct {
    int16_t x_raw, y_raw, z_raw;    // Raw acceleration values
    float x_g, y_g, z_g;            // Acceleration in g units
    float magnitude;                // Total acceleration magnitude
    float tilt_x, tilt_y;          // Tilt angles in degrees
    uint8_t sensor_ok;              // Sensor status flag
    TickType_t last_update;         // FreeRTOS timestamp of last successful reading
} AccelData_t;

// Public function declarations
uint8_t Sensors_Init(void);
uint8_t Sensors_UpdateAll(void);
uint8_t Sensors_UpdateClimate(void);
uint8_t Sensors_UpdateAccel(void);

// Data access functions
const ClimateData_t* Sensors_GetClimate(void);
const AccelData_t* Sensors_GetAccel(void);
uint8_t Sensors_GetSystemStatus(void);

// Utility functions
const char* Sensors_GetComfortStatus(void);
const char* Sensors_GetOrientationStatus(void);

// Test functions
void Sensors_RunAllTests(void);

// FreeRTOS-specific sensor functions
void Sensors_LogStatus(void);

#endif /* SENSORS_H */

/* freertos_globals.h - Global FreeRTOS Variables */
#ifndef FREERTOS_GLOBALS_H
#define FREERTOS_GLOBALS_H

#include "FreeRTOS.h"
#include "semphr.h"

// Global FreeRTOS synchronization objects
// Defined in main.c, accessible from all modules
extern SemaphoreHandle_t uartMutex;
extern SemaphoreHandle_t i2cMutex;

#endif /* FREERTOS_GLOBALS_H */

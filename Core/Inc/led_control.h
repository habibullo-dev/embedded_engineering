/* led_control.h - LED Control Module Interface - FreeRTOS Version */
#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

// Public function declarations - Original functions
void LED_Init(void);
void LED_Control(uint8_t led_num, uint8_t state);
void LED_ControlAll(uint8_t state);
void LED_SetTimer(uint8_t led_num, uint32_t duration_ms);
void LED_UpdateTimers(void);
uint8_t LED_GetState(uint8_t led_num);

// FreeRTOS-enhanced LED functions
void LED_BlinkPattern(uint8_t led_num, uint16_t on_time_ms, uint16_t off_time_ms, uint8_t count);
void LED_SequentialBlink(uint16_t delay_ms);
void LED_AllBlink(uint16_t on_time_ms, uint16_t off_time_ms, uint8_t count);
uint32_t LED_GetRemainingTime(uint8_t led_num);
void LED_ShowSystemStatus(void);
void LED_TaskHeartbeat(uint8_t led_num);
void LED_ErrorPattern(void);

// Public constants
#define LED_OFF 0
#define LED_ON  1
#define LED_1   1
#define LED_2   2
#define LED_3   3
#define LED_ALL 0xFF

#endif /* LED_CONTROL_H */

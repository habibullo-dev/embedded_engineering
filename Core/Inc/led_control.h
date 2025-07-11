/* led_control.h - LED Control Module Interface */
#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "main.h"

// Public function declarations
void LED_Init(void);
void LED_Control(uint8_t led_num, uint8_t state);
void LED_ControlAll(uint8_t state);
void LED_SetTimer(uint8_t led_num, uint32_t duration_ms);
void LED_UpdateTimers(void);
uint8_t LED_GetState(uint8_t led_num);

// Public constants
#define LED_OFF 0
#define LED_ON  1
#define LED_1   1
#define LED_2   2
#define LED_3   3
#define LED_ALL 0xFF

#endif /* LED_CONTROL_H */

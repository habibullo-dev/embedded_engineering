/* led_control.c - LED Control Module Implementation */
#include "led_control.h"
#include "system_logging.h"
#include <stdio.h>

// Private definitions
#define LED1_PIN GPIO_PIN_0
#define LED2_PIN GPIO_PIN_7
#define LED3_PIN GPIO_PIN_14
#define LED_PORT GPIOB
#define NUM_LEDS 3

// Private variables
static volatile uint32_t led_timers[NUM_LEDS] = {0, 0, 0};
static volatile uint8_t led_states[NUM_LEDS] = {0, 0, 0};

// External system tick
extern volatile uint32_t system_tick;

// Private function
static uint16_t get_led_pin(uint8_t led_num) {
    switch(led_num) {
        case LED_1: return LED1_PIN;
        case LED_2: return LED2_PIN;
        case LED_3: return LED3_PIN;
        default: return 0;
    }
}

// Public function implementations
void LED_Init(void) {
    LED_ControlAll(LED_OFF);
}

void LED_Control(uint8_t led_num, uint8_t state) {
    if (led_num < LED_1 || led_num > LED_3) return;

    uint16_t pin = get_led_pin(led_num);
    if (pin == 0) return;

    if (state == LED_ON) {
        HAL_GPIO_WritePin(LED_PORT, pin, GPIO_PIN_SET);
        led_states[led_num - 1] = 1;
    } else {
        HAL_GPIO_WritePin(LED_PORT, pin, GPIO_PIN_RESET);
        led_states[led_num - 1] = 0;
        led_timers[led_num - 1] = 0;
    }
}

void LED_ControlAll(uint8_t state) {
    for (uint8_t i = LED_1; i <= LED_3; i++) {
        LED_Control(i, state);
    }
}

void LED_SetTimer(uint8_t led_num, uint32_t duration_ms) {
    if (led_num < LED_1 || led_num > LED_3) return;

    LED_Control(led_num, LED_ON);
    led_timers[led_num - 1] = system_tick + duration_ms;
}

void LED_UpdateTimers(void) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        if (led_timers[i] > 0 && led_states[i] == 1) {
            if (system_tick >= led_timers[i]) {
                LED_Control(i + 1, LED_OFF);
                char msg[50];
                sprintf(msg, "LED%d timer expired", i + 1);
                SystemLog_Add(LOG_INFO, "led", msg);
            }
        }
    }
}

uint8_t LED_GetState(uint8_t led_num) {
    if (led_num < LED_1 || led_num > LED_3) return 0;
    return led_states[led_num - 1];
}

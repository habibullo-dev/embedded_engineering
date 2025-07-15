/* led_control.c - LED Control Module Implementation - FreeRTOS Version */
#include "led_control.h"
#include "system_logging.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// Private definitions
#define LED1_PIN GPIO_PIN_0
#define LED2_PIN GPIO_PIN_7
#define LED3_PIN GPIO_PIN_14
#define LED_PORT GPIOB
#define NUM_LEDS 3

// Private variables
static volatile TickType_t led_timers[NUM_LEDS] = {0, 0, 0};  // Use FreeRTOS tick type
static volatile uint8_t led_states[NUM_LEDS] = {0, 0, 0};

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
    SystemLog_Add(LOG_INFO, "led", "LED control initialized (FreeRTOS)");
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
        led_timers[led_num - 1] = 0;  // Clear timer when manually turned off
    }
}

void LED_ControlAll(uint8_t state) {
    for (uint8_t i = LED_1; i <= LED_3; i++) {
        LED_Control(i, state);
    }
}

void LED_SetTimer(uint8_t led_num, uint32_t duration_ms) {
    if (led_num < LED_1 || led_num > LED_3) return;

    // Turn on LED first
    LED_Control(led_num, LED_ON);

    // Set timer using FreeRTOS tick count
    TickType_t currentTick = xTaskGetTickCount();
    TickType_t timerTicks = pdMS_TO_TICKS(duration_ms);
    led_timers[led_num - 1] = currentTick + timerTicks;

    char msg[60];
    sprintf(msg, "LED%d timer set for %lums", led_num, duration_ms);
    SystemLog_Add(LOG_INFO, "led", msg);
}

void LED_UpdateTimers(void) {
    TickType_t currentTick = xTaskGetTickCount();

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        // Check if timer is active and LED is on
        if (led_timers[i] > 0 && led_states[i] == 1) {
            // Check if timer has expired (handle tick overflow properly)
            if ((currentTick - led_timers[i]) < (UINT32_MAX / 2)) {
                // Timer expired - turn off LED
                LED_Control(i + 1, LED_OFF);
                char msg[50];
                sprintf(msg, "LED%d timer expired", i + 1);
                SystemLog_Add(LOG_INFO, "led", msg);
                led_timers[i] = 0;  // Clear the timer
            }
        }
    }
}

uint8_t LED_GetState(uint8_t led_num) {
    if (led_num < LED_1 || led_num > LED_3) return 0;
    return led_states[led_num - 1];
}

// FreeRTOS-specific LED functions
void LED_BlinkPattern(uint8_t led_num, uint16_t on_time_ms, uint16_t off_time_ms, uint8_t count) {
    // Non-blocking blink pattern using FreeRTOS delays
    if (led_num < LED_1 || led_num > LED_3) return;

    for (uint8_t i = 0; i < count; i++) {
        LED_Control(led_num, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        LED_Control(led_num, LED_OFF);
        if (i < count - 1) {  // Don't delay after the last blink
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }
}

void LED_SequentialBlink(uint16_t delay_ms) {
    // Blink LEDs in sequence: 1 -> 2 -> 3 -> All Off
    for (uint8_t led = LED_1; led <= LED_3; led++) {
        LED_Control(led, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        LED_Control(led, LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(delay_ms / 2));  // Short gap between LEDs
    }
}

void LED_AllBlink(uint16_t on_time_ms, uint16_t off_time_ms, uint8_t count) {
    // Blink all LEDs together
    for (uint8_t i = 0; i < count; i++) {
        LED_ControlAll(LED_ON);
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        LED_ControlAll(LED_OFF);
        if (i < count - 1) {  // Don't delay after the last blink
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }
}

uint32_t LED_GetRemainingTime(uint8_t led_num) {
    // Get remaining time for LED timer in milliseconds
    if (led_num < LED_1 || led_num > LED_3) return 0;
    if (led_timers[led_num - 1] == 0) return 0;

    TickType_t currentTick = xTaskGetTickCount();
    TickType_t timerTick = led_timers[led_num - 1];

    if (currentTick >= timerTick) {
        return 0;  // Timer already expired
    }

    TickType_t remainingTicks = timerTick - currentTick;
    return (remainingTicks * portTICK_PERIOD_MS);
}

void LED_ShowSystemStatus(void) {
    // Use LED pattern to show system status
    // Green pattern: System OK
    // Red pattern: System Error
    // Yellow pattern: System Warning

    // For now, show "system alive" pattern
    LED_SequentialBlink(100);  // Quick sequential blink
}

void LED_TaskHeartbeat(uint8_t led_num) {
    // Simple heartbeat for task monitoring
    // Quick on/off to show task is running
    if (led_num >= LED_1 && led_num <= LED_3) {
        LED_Control(led_num, LED_ON);
        vTaskDelay(pdMS_TO_TICKS(50));
        LED_Control(led_num, LED_OFF);
    }
}

void LED_ErrorPattern(void) {
    // Error indication pattern - rapid blinking of all LEDs
    LED_AllBlink(100, 100, 5);  // 5 rapid blinks
}

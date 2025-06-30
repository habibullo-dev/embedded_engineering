/* STM32F767ZI Pure HAL LED Sequence Controller
 *
 * This implementation uses ONLY HAL functions - no BSP dependencies
 * Hardware connections for NUCLEO-F767ZI (from your pin definitions):
 * - Green LED (LD1): PB0 (active HIGH)
 * - Blue LED (LD2): PB7 (active HIGH)
 * - Red LED (LD3): PB14 (active HIGH)
 * - User Button: PC13 (active LOW, external pull-up)
 *
 * Learning Objectives:
 * - Direct HAL GPIO configuration and control
 * - Manual peripheral clock management
 * - Hardware-level understanding of GPIO operations
 * - Portable code design principles
 * - State machine implementation with non-blocking timing
 */

#include "main.h"

/* Timing Constants */
#define DEBOUNCE_TIME_MS        50    // Button debounce period
#define LED_TRANSITION_DELAY_MS 500   // Time between LED sequence steps
#define MAIN_LOOP_DELAY_MS      10    // Main loop polling interval

/* LED Sequence State Machine */
typedef enum {
    LED_SEQ_IDLE,           // All LEDs off, waiting for button press
    LED_SEQ_GREEN_ON,       // Turn on green LED (LD1)
    LED_SEQ_BLUE_ON,        // Turn on blue LED (LD2)
    LED_SEQ_RED_ON,         // Turn on red LED (LD3)
    LED_SEQ_PAUSE_ON,       // Brief pause with all LEDs on
    LED_SEQ_RED_OFF,        // Turn off red LED (LD3)
    LED_SEQ_BLUE_OFF,       // Turn off blue LED (LD2)
    LED_SEQ_GREEN_OFF,      // Turn off green LED (LD1)
    LED_SEQ_COMPLETE        // Sequence finished
} led_sequence_state_t;

/* Button State Tracking Structure */
typedef struct {
    GPIO_PinState current_state;
    GPIO_PinState previous_state;
    uint32_t last_change_time;
    uint8_t is_pressed;
    uint8_t press_detected;
} button_state_t;

/* LED Sequence Controller Structure */
typedef struct {
    led_sequence_state_t current_state;
    uint32_t last_transition_time;
    uint8_t sequence_active;
} led_sequence_t;

/* Global Variables */
button_state_t user_button;
led_sequence_t led_sequence;

/* Private Function Prototypes */
void SystemClock_Config(void);
static void GPIO_Init_Custom(void);
static void Button_Init_Polling(void);
static void LED_Set_State(GPIO_TypeDef* port, uint16_t pin, uint8_t state);
static uint8_t Button_Read_Debounced(void);
static void LED_Sequence_Start(void);
static void LED_Sequence_Stop(void);
static void LED_Sequence_Update(void);
static void LED_All_Off(void);

/**
 * @brief Main application entry point
 */
int main(void)
{
    /* Initialize HAL Library */
    HAL_Init();

    /* Configure system clock */
    SystemClock_Config();

    /* Initialize GPIO subsystem with our custom configuration */
    GPIO_Init_Custom();

    /* Initialize button state tracking */
    user_button.current_state = GPIO_PIN_SET;
    user_button.previous_state = GPIO_PIN_SET;
    user_button.last_change_time = 0;
    user_button.is_pressed = 0;
    user_button.press_detected = 0;

    /* Initialize LED sequence state machine */
    led_sequence.current_state = LED_SEQ_IDLE;
    led_sequence.last_transition_time = 0;
    led_sequence.sequence_active = 0;

    /* Ensure all LEDs start in OFF state */
    LED_All_Off();

    /* Main application loop */
    while (1)
    {
        /* Check for button press events using polling method */
        if (Button_Read_Debounced())
        {
            /* Button press detected - toggle sequence */
            if (led_sequence.sequence_active)
            {
                LED_Sequence_Stop();
            }
            else
            {
                LED_Sequence_Start();
            }
        }

        /* Update LED sequence state machine */
        LED_Sequence_Update();

        /* Small delay to prevent excessive CPU usage */
        HAL_Delay(MAIN_LOOP_DELAY_MS);
    }
}

/**
 * @brief Initialize GPIO subsystem with custom polling configuration
 * This replaces the CubeMX-generated configuration to use polling instead of interrupts
 */
static void GPIO_Init_Custom(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO port clocks - CRITICAL FIRST STEP */
    /* Each GPIO port must have its clock enabled before use */
    __HAL_RCC_GPIOB_CLK_ENABLE();  // For all three LEDs (PB0, PB7, PB14)
    __HAL_RCC_GPIOC_CLK_ENABLE();  // For user button (PC13)

    /* Configure LED pins as outputs using the symbolic names from main.h */

    /* Green LED (LD1) on PB0 */
    GPIO_InitStruct.Pin = LD1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    // Push-pull output
    GPIO_InitStruct.Pull = GPIO_NOPULL;            // No pull resistor needed
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // Low speed sufficient for LED
    HAL_GPIO_Init(LD1_GPIO_Port, &GPIO_InitStruct);

    /* Blue LED (LD2) on PB7 */
    GPIO_InitStruct.Pin = LD2_Pin;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

    /* Red LED (LD3) on PB14 */
    GPIO_InitStruct.Pin = LD3_Pin;
    HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

    /* Initialize button for polling instead of interrupt mode */
    Button_Init_Polling();

    /* Set all LEDs to OFF state initially */
    HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
}

/**
 * @brief Initialize button GPIO pin for polling mode
 * This demonstrates how to configure input pins for polling instead of interrupts
 */
static void Button_Init_Polling(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Configure User Button Pin (PC13) for polling mode */
    GPIO_InitStruct.Pin = USER_Btn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // Simple input mode (not interrupt)
    GPIO_InitStruct.Pull = GPIO_NOPULL;            // External pull-up on board
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // Speed not critical for input
    HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);
}

/**
 * @brief Set LED state using HAL GPIO functions
 * @param port: GPIO port (GPIOB in our case)
 * @param pin: GPIO pin number (LD1_Pin, LD2_Pin, LD3_Pin)
 * @param state: 1 = ON, 0 = OFF
 *
 * This function demonstrates proper HAL GPIO control
 */
static void LED_Set_State(GPIO_TypeDef* port, uint16_t pin, uint8_t state)
{
    if (state)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);    // Turn ON (high voltage)
    }
    else
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);  // Turn OFF (low voltage)
    }
}

/**
 * @brief Turn off all LEDs using the symbolic names from main.h
 * This demonstrates how symbolic names make code more readable and maintainable
 */
static void LED_All_Off(void)
{
    LED_Set_State(LD1_GPIO_Port, LD1_Pin, 0);  // Green LED off
    LED_Set_State(LD2_GPIO_Port, LD2_Pin, 0);  // Blue LED off
    LED_Set_State(LD3_GPIO_Port, LD3_Pin, 0);  // Red LED off
}

/**
 * @brief Read button state with debouncing using polling method
 * @retval 1 if button press detected, 0 otherwise
 *
 * This function implements a robust debouncing algorithm that prevents
 * false triggering from mechanical switch bounce
 */
static uint8_t Button_Read_Debounced(void)
{
    /* Read raw button state using HAL function (button is active LOW) */
    GPIO_PinState raw_state = HAL_GPIO_ReadPin(USER_Btn_GPIO_Port, USER_Btn_Pin);

    /* Update current state */
    user_button.current_state = raw_state;

    /* Detect state change - this starts the debounce timing */
    if (user_button.current_state != user_button.previous_state)
    {
        /* State changed - record time and reset detection flag */
        user_button.last_change_time = HAL_GetTick();
        user_button.press_detected = 0;
    }

    /* Check if state has been stable for debounce period */
    if ((HAL_GetTick() - user_button.last_change_time) > DEBOUNCE_TIME_MS)
    {
        /* Determine current pressed state (button is active LOW) */
        uint8_t new_pressed_state = (user_button.current_state == GPIO_PIN_RESET) ? 1 : 0;

        /* Detect button press event (transition from not pressed to pressed) */
        if (!user_button.is_pressed && new_pressed_state && !user_button.press_detected)
        {
            user_button.press_detected = 1;
            user_button.is_pressed = new_pressed_state;
            return 1;  // Button press event detected
        }

        /* Update stable button state */
        user_button.is_pressed = new_pressed_state;
    }

    /* Save current state for next comparison */
    user_button.previous_state = user_button.current_state;

    return 0;  // No button press event
}

/**
 * @brief Start LED sequence
 * This function initializes the state machine and begins the light show
 */
static void LED_Sequence_Start(void)
{
    led_sequence.current_state = LED_SEQ_GREEN_ON;
    led_sequence.last_transition_time = HAL_GetTick();
    led_sequence.sequence_active = 1;

    /* Turn on first LED immediately */
    LED_Set_State(LD1_GPIO_Port, LD1_Pin, 1);
}

/**
 * @brief Stop LED sequence and turn off all LEDs
 * This provides immediate user control over the sequence
 */
static void LED_Sequence_Stop(void)
{
    led_sequence.current_state = LED_SEQ_IDLE;
    led_sequence.sequence_active = 0;
    LED_All_Off();
}

/**
 * @brief Update LED sequence state machine
 * This function implements the sequential LED light show using non-blocking timing
 *
 * The sequence follows this pattern:
 * Green -> Green+Blue -> Green+Blue+Red -> Blue+Red -> Red -> Off
 */
static void LED_Sequence_Update(void)
{
    /* Only process if sequence is active */
    if (!led_sequence.sequence_active)
        return;

    /* Check if it's time for next transition (non-blocking timing) */
    if ((HAL_GetTick() - led_sequence.last_transition_time) < LED_TRANSITION_DELAY_MS)
        return;

    /* Time for next state - update timestamp */
    led_sequence.last_transition_time = HAL_GetTick();

    /* State machine implementation - each state knows what comes next */
    switch (led_sequence.current_state)
    {
        case LED_SEQ_GREEN_ON:
            /* Green is already on, now turn on blue while keeping green on */
            LED_Set_State(LD2_GPIO_Port, LD2_Pin, 1);
            led_sequence.current_state = LED_SEQ_BLUE_ON;
            break;

        case LED_SEQ_BLUE_ON:
            /* Green and blue are on, now turn on red while keeping others on */
            LED_Set_State(LD3_GPIO_Port, LD3_Pin, 1);
            led_sequence.current_state = LED_SEQ_RED_ON;
            break;

        case LED_SEQ_RED_ON:
            /* All LEDs on, pause briefly then start turning off */
            led_sequence.current_state = LED_SEQ_PAUSE_ON;
            break;

        case LED_SEQ_PAUSE_ON:
            /* Start turning off LEDs in reverse order - turn off green first */
            LED_Set_State(LD1_GPIO_Port, LD1_Pin, 0);
            led_sequence.current_state = LED_SEQ_GREEN_OFF;
            break;

        case LED_SEQ_GREEN_OFF:
            /* Turn off blue LED, leaving only red */
            LED_Set_State(LD2_GPIO_Port, LD2_Pin, 0);
            led_sequence.current_state = LED_SEQ_BLUE_OFF;
            break;

        case LED_SEQ_BLUE_OFF:
            /* Turn off red LED - all LEDs now off */
            LED_Set_State(LD3_GPIO_Port, LD3_Pin, 0);
            led_sequence.current_state = LED_SEQ_RED_OFF;
            break;

        case LED_SEQ_RED_OFF:
            /* Sequence complete */
            led_sequence.current_state = LED_SEQ_COMPLETE;
            break;

        case LED_SEQ_COMPLETE:
            /* Return to idle state, ready for next button press */
            led_sequence.current_state = LED_SEQ_IDLE;
            led_sequence.sequence_active = 0;
            break;

        case LED_SEQ_IDLE:
        default:
            /* Should not reach here during active sequence */
            led_sequence.sequence_active = 0;
            break;
    }
}

/**
 * @brief System Clock Configuration
 * Generated by CubeMX - this configures the microcontroller clocks
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure LSE Drive Capability */
    HAL_PWR_EnableBkUpAccess();

    /** Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /** Initializes the RCC Oscillators according to the specified parameters */
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

    /** Activate the Over-Drive mode */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks */
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
 * @brief Error Handler
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif

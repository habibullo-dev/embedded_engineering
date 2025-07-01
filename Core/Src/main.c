/* STM32F767ZI Interrupt-Driven LED Sequence Controller - COMPLETE VERSION
 *
 * Hardware connections for NUCLEO-F767ZI:
 * - Green LED (LD1): PB0 (active HIGH)
 * - Blue LED (LD2): PB7 (active HIGH)
 * - Red LED (LD3): PB14 (active HIGH)
 * - User Button: PC13 (active LOW, external pull-up)
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

/* LED Sequence Controller Structure */
typedef struct {
    led_sequence_state_t current_state;
    uint32_t last_transition_time;
    uint8_t sequence_active;
} led_sequence_t;

/* Interrupt Communication Structure */
typedef struct {
    volatile uint32_t last_interrupt_time;
    volatile uint8_t button_event_pending;
} interrupt_comm_t;

/* Global Variables */
led_sequence_t led_sequence;
interrupt_comm_t button_comm;

/* Private Function Prototypes */
void SystemClock_Config(void);
static void GPIO_Init_Interrupt(void);
static void LED_Set_State(GPIO_TypeDef* port, uint16_t pin, uint8_t state);
static void LED_Sequence_Start(void);
static void LED_Sequence_Stop(void);
static void LED_Sequence_Update(void);
static void LED_All_Off(void);
static void Process_Button_Event(void);

/**
 * @brief Main application entry point
 */
int main(void)
{
    /* Initialize HAL Library */
    HAL_Init();

    /* Configure system clock */
    SystemClock_Config();

    /* Initialize GPIO subsystem with interrupt configuration */
    GPIO_Init_Interrupt();

    /* Initialize LED sequence state machine */
    led_sequence.current_state = LED_SEQ_IDLE;
    led_sequence.last_transition_time = 0;
    led_sequence.sequence_active = 0;

    /* Initialize interrupt communication structure */
    button_comm.last_interrupt_time = 0;
    button_comm.button_event_pending = 0;

    /* Ensure all LEDs start in OFF state */
    LED_All_Off();

    /* Main application loop */
    while (1)
    {
        /* Process button events signaled by interrupt service routine */
        Process_Button_Event();

        /* Update LED sequence state machine */
        LED_Sequence_Update();

        /* Background processing delay */
        HAL_Delay(MAIN_LOOP_DELAY_MS);
    }
}

/**
 * @brief Initialize GPIO subsystem with interrupt configuration
 */
static void GPIO_Init_Interrupt(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO port clocks */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Configure LED pins as outputs */
    GPIO_InitStruct.Pin = LD1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LD2_Pin;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LD3_Pin;
    HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

    /* Configure button for interrupt operation */
    GPIO_InitStruct.Pin = USER_Btn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

    /* Configure and enable EXTI interrupt */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    /* Set all LEDs to OFF state initially */
    HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
}

/**
 * @brief Set LED state using HAL GPIO functions
 */
static void LED_Set_State(GPIO_TypeDef* port, uint16_t pin, uint8_t state)
{
    if (state)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief Turn off all LEDs
 */
static void LED_All_Off(void)
{
    LED_Set_State(LD1_GPIO_Port, LD1_Pin, 0);
    LED_Set_State(LD2_GPIO_Port, LD2_Pin, 0);
    LED_Set_State(LD3_GPIO_Port, LD3_Pin, 0);
}

/**
 * @brief Process button events signaled by interrupt service routine
 */
static void Process_Button_Event(void)
{
    if (button_comm.button_event_pending)
    {
        uint32_t time_since_interrupt = HAL_GetTick() - button_comm.last_interrupt_time;

        if (time_since_interrupt >= DEBOUNCE_TIME_MS)
        {
            if (led_sequence.sequence_active)
            {
                LED_Sequence_Stop();
            }
            else
            {
                LED_Sequence_Start();
            }

            button_comm.button_event_pending = 0;
        }
    }
}

/**
 * @brief Start LED sequence
 */
static void LED_Sequence_Start(void)
{
    led_sequence.current_state = LED_SEQ_GREEN_ON;
    led_sequence.last_transition_time = HAL_GetTick();
    led_sequence.sequence_active = 1;

    LED_Set_State(LD1_GPIO_Port, LD1_Pin, 1);
}

/**
 * @brief Stop LED sequence and turn off all LEDs
 */
static void LED_Sequence_Stop(void)
{
    led_sequence.current_state = LED_SEQ_IDLE;
    led_sequence.sequence_active = 0;
    LED_All_Off();
}

/**
 * @brief Update LED sequence state machine
 */
static void LED_Sequence_Update(void)
{
    if (!led_sequence.sequence_active)
        return;

    if ((HAL_GetTick() - led_sequence.last_transition_time) < LED_TRANSITION_DELAY_MS)
        return;

    led_sequence.last_transition_time = HAL_GetTick();

    switch (led_sequence.current_state)
    {
        case LED_SEQ_GREEN_ON:
            LED_Set_State(LD2_GPIO_Port, LD2_Pin, 1);
            led_sequence.current_state = LED_SEQ_BLUE_ON;
            break;

        case LED_SEQ_BLUE_ON:
            LED_Set_State(LD3_GPIO_Port, LD3_Pin, 1);
            led_sequence.current_state = LED_SEQ_RED_ON;
            break;

        case LED_SEQ_RED_ON:
            led_sequence.current_state = LED_SEQ_PAUSE_ON;
            break;

        case LED_SEQ_PAUSE_ON:
            LED_Set_State(LD1_GPIO_Port, LD1_Pin, 0);
            led_sequence.current_state = LED_SEQ_GREEN_OFF;
            break;

        case LED_SEQ_GREEN_OFF:
            LED_Set_State(LD2_GPIO_Port, LD2_Pin, 0);
            led_sequence.current_state = LED_SEQ_BLUE_OFF;
            break;

        case LED_SEQ_BLUE_OFF:
            LED_Set_State(LD3_GPIO_Port, LD3_Pin, 0);
            led_sequence.current_state = LED_SEQ_RED_OFF;
            break;

        case LED_SEQ_RED_OFF:
            led_sequence.current_state = LED_SEQ_COMPLETE;
            break;

        case LED_SEQ_COMPLETE:
            led_sequence.current_state = LED_SEQ_IDLE;
            led_sequence.sequence_active = 0;
            break;

        case LED_SEQ_IDLE:
        default:
            led_sequence.sequence_active = 0;
            break;
    }
}

/**
 * @brief EXTI line 15-10 interrupt handler - THIS WAS MISSING!
 */
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(USER_Btn_Pin);
}

/**
 * @brief GPIO EXTI Interrupt Service Routine
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == USER_Btn_Pin)
    {
        button_comm.last_interrupt_time = HAL_GetTick();
        button_comm.button_event_pending = 1;
    }
}

/**
 * @brief System Clock Configuration
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

    /** Initializes the RCC Oscillators */
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

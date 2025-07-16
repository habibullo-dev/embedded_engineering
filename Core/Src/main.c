/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body with FreeRTOS Implementation
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "eth.h"
#include "i2c.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

// Application modules
#include "terminal_ui.h"
#include "sensors.h"
#include "led_control.h"
#include "system_logging.h"
#include "system_config.h"
#include "persistent_logging.h"  // Add persistent logging
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TERMINAL_TASK_PRIORITY    3
#define SENSOR_TASK_PRIORITY      2
#define LED_TASK_PRIORITY         2
#define SYSTEM_TASK_PRIORITY      1

#define TERMINAL_STACK_SIZE       1024
#define SENSOR_STACK_SIZE         512
#define LED_STACK_SIZE            256
#define SYSTEM_STACK_SIZE         256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

// Global FreeRTOS Objects
SemaphoreHandle_t uartMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;

// Task handles
TaskHandle_t terminalTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t systemTaskHandle = NULL;

// Timers
TimerHandle_t sensorTimer = NULL;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);

/* USER CODE BEGIN PFP */
// Task function prototypes
void TerminalTask(void *argument);
void SensorTask(void *argument);
void LEDTask(void *argument);
void SystemTask(void *argument);

// Timer callback
void SensorTimerCallback(TimerHandle_t xTimer);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ETH_Init();
  MX_UART4_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_I2C2_Init();

  /* USER CODE BEGIN 2 */

  // Verify UART4 interrupt priority is FreeRTOS compatible
  HAL_NVIC_SetPriority(UART4_IRQn, 5, 0);  // CRITICAL: Must be >= 5 for FreeRTOS

  // Create FreeRTOS objects BEFORE starting scheduler
  uartMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  if (uartMutex == NULL || i2cMutex == NULL) {
    HAL_UART_Transmit(&huart4, (uint8_t*)"FATAL: Mutex creation failed\r\n", 30, 1000);
    Error_Handler();
  }

  // Initialize basic modules
  LED_Init();

  // Initialize terminal UI (sets up UART interrupt)
  TerminalUI_Init();

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

/* USER CODE BEGIN 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  // Mutexes already created in main()
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  // Create software timers
  sensorTimer = xTimerCreate("SensorTimer",
                            pdMS_TO_TICKS(5000),  // 5 second timer
                            pdTRUE,  // Auto-reload
                            (void*)0,
                            SensorTimerCallback);

  if (sensorTimer != NULL) {
    xTimerStart(sensorTimer, 0);
  }
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* USER CODE BEGIN RTOS_THREADS */

  // Terminal Task - Highest priority for responsiveness
  BaseType_t result1 = xTaskCreate(TerminalTask,
                                  "TerminalTask",
                                  TERMINAL_STACK_SIZE,
                                  NULL,
                                  TERMINAL_TASK_PRIORITY,
                                  &terminalTaskHandle);

  // Sensor Task - Normal priority
  BaseType_t result2 = xTaskCreate(SensorTask,
                                  "SensorTask",
                                  SENSOR_STACK_SIZE,
                                  NULL,
                                  SENSOR_TASK_PRIORITY,
                                  &sensorTaskHandle);

  // LED Task - Normal priority
  BaseType_t result3 = xTaskCreate(LEDTask,
                                  "LEDTask",
                                  LED_STACK_SIZE,
                                  NULL,
                                  LED_TASK_PRIORITY,
                                  &ledTaskHandle);

  // System monitoring task - Lower priority
  BaseType_t result4 = xTaskCreate(SystemTask,
                                  "SystemTask",
                                  SYSTEM_STACK_SIZE,
                                  NULL,
                                  SYSTEM_TASK_PRIORITY,
                                  &systemTaskHandle);

  // Use direct UART instead of SystemLog during initialization
  if (result1 != pdPASS || result2 != pdPASS ||
      result3 != pdPASS || result4 != pdPASS) {
    HAL_UART_Transmit(&huart4, (uint8_t*)"FATAL: Task creation failed\r\n", 29, 1000);
  }

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */
}

// Task Implementations
void TerminalTask(void *argument) {
  // Add small delay to ensure system is stable
  vTaskDelay(pdMS_TO_TICKS(100));

  // Initialize modules that need FreeRTOS
  SystemLog_Init();
  PersistentLog_Init();  // Initialize persistent logging
  Sensors_Init();

  // Log system startup to persistent storage
  PersistentLog_Add(LOG_INFO, "system", "System started successfully");

  // Now safe to use terminal functions with mutexes
  TerminalUI_ShowBanner();

  for(;;) {
    TerminalUI_ProcessInput();
    TerminalUI_CheckTimeout();
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms polling
  }
}

void SensorTask(void *argument) {
  static uint32_t log_counter = 0;
  for(;;) {
    // Wait for sensor timer notification
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));

    if (Sensors_UpdateAll()) {
      // Only log every 10th sensor update to save flash space
      log_counter++;
      if (log_counter >= 10) {
        PersistentLog_Add(LOG_SENSOR, "sensors", "Periodic sensor update");
        log_counter = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void LEDTask(void *argument) {
  for(;;) {
    LED_UpdateTimers();
    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms LED update
  }
}

void SystemTask(void *argument) {
  for(;;) {
    // System monitoring - heap, stack, tasks
    size_t freeHeap = xPortGetFreeHeapSize();
    if (freeHeap < 1000) {
      PersistentLog_Add(LOG_WARNING, "system", "Low heap memory");
    }

    // Check stack usage
    if (terminalTaskHandle != NULL) {
      UBaseType_t terminalStack = uxTaskGetStackHighWaterMark(terminalTaskHandle);
      if (terminalStack < 50) {
        PersistentLog_Add(LOG_WARNING, "system", "Terminal task low stack");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
  }
}

// Timer Callback
void SensorTimerCallback(TimerHandle_t xTimer) {
  // Notify sensor task to update
  if (sensorTaskHandle != NULL) {
    xTaskNotifyGive(sensorTaskHandle);
  }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  // Log critical error to persistent storage before halting
  if (uartMutex != NULL) {
    PersistentLog_Add(LOG_ERROR, "system", "Critical error - system halted");
  }
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// distinguish short vs long cycle
typedef enum
{
  LED_STATE_IDLE = 0,          // No cycle running
  LED_STATE_CYCLE_RUNNING,     // UV/cycle in progress
  LED_STATE_ERROR              // Reserved for future error/low-batt, etc.
} LedState_t;

typedef enum
{
  CYCLE_MODE_NONE = 0,
  CYCLE_MODE_SHORT,   // 1-minute (yellow)
  CYCLE_MODE_LONG     // 3-minute (blue)
} CycleMode_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CYCLE_DURATION_SHORT_MS   (60u * 1000u)           // 1-minute cycle
#define CYCLE_DURATION_LONG_MS    (3u  * 60u * 1000u)     // 3-minute cycle
#define DOUBLE_TAP_WINDOW_MS      400u                    // max gap between taps for "double"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint8_t  g_load_enabled  = 0;   // start OFF for safety
volatile uint32_t g_last_press_ms = 0;

volatile LedState_t   g_led_state    = LED_STATE_IDLE;
volatile CycleMode_t  g_cycle_mode   = CYCLE_MODE_NONE;
static   uint32_t     g_pwm_period   = 0;   // cached TIM2 ARR for LED PWM

// timing + tap tracking
volatile uint32_t g_cycle_end_ms = 0;      // when current cycle should auto-stop
volatile uint8_t  g_pending_tap  = 0;      // 1 = waiting to see if this becomes a double tap
volatile uint32_t g_first_tap_ms = 0;      // time of first tap in a potential double-tap sequence
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static void RGB_Set(uint16_t r, uint16_t g, uint16_t b);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void RGB_Init(void)
{
  // Cache the auto-reload value (ARR) set in MX_TIM2_Init (999 in your config)
  g_pwm_period = __HAL_TIM_GET_AUTORELOAD(&htim2);

  // Start PWM on all three channels
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  // Ensure LED is fully OFF (brightness = 0 → CCR = 0 → output HIGH all the time)
  RGB_Set(0, 0, 0);
}

// brightness arguments are 0..g_pwm_period
//   0   = OFF
//   max = brightest
static void RGB_Set(uint16_t r, uint16_t g, uint16_t b)
{
  if (r > g_pwm_period) r = g_pwm_period;
  if (g > g_pwm_period) g = g_pwm_period;
  if (b > g_pwm_period) b = g_pwm_period;

  // Common-anode LED, active-low PWM:
  //   - TIM2 CH4 (PA3) → RED
  //   - TIM2 CH2 (PA1) → GREEN
  //   - TIM2 CH1 (PA0) → BLUE
  //
  // With PWM1 + OCPOLARITY_LOW:
  //   CCR = 0            → output HIGH 100% → LED OFF
  //   CCR > 0            → output LOW for (CCR/ARR) → LED ON fractionally
  //   CCR = g_pwm_period → ~full brightness
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, r); // RED
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, g); // GREEN
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, b); // BLUE
}

// Map logical LED states to actual RGB colors
static void LED_ApplyState(void)
{
  uint16_t r = 0, g = 0, b = 0;
  uint16_t mid = g_pwm_period / 2;  // ~50% brightness

  switch (g_led_state)
  {
    case LED_STATE_IDLE:
      r = g = b = 0;           // OFF
      break;

    case LED_STATE_CYCLE_RUNNING:
      if (g_cycle_mode == CYCLE_MODE_LONG)
      {
        // Long cycle (3 min) → BLUE
        r = 0;
        g = 0;
        b = (g_pwm_period * 3) / 4;     // ~75% blue
      }
      else
      {
        // Short cycle (1 min) → warm amber
        r = (g_pwm_period * 3)    / 4;   //  ~75% of full scale
        g = (g_pwm_period * 3)    / 16;  //  ~18–20% of full scale
        b = 0;
      }
      break;

    case LED_STATE_ERROR:
    default:
      r = mid; g = 0; b = 0;   // RED
      break;
  }

  RGB_Set(r, g, b);
}

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
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  // Ensure load is OFF at boot for safety (TPS22918 ON low)
  g_load_enabled  = 0;
  g_cycle_mode   = CYCLE_MODE_NONE;
  g_cycle_end_ms = 0;
  g_pending_tap  = 0;
  HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_RESET);

  // Initialize RGB status LED (TIM2 PWM on PA0/PA1/PA3)
  RGB_Init();
  g_led_state = LED_STATE_IDLE;
  LED_ApplyState();

  const uint16_t frame_delay_ms = 100; // reused as active-loop delay

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Reed safety: if reed opens, force load OFF
    GPIO_PinState reed_state = HAL_GPIO_ReadPin(REED_SW_GPIO_Port, REED_SW_Pin);
    uint8_t reed_closed      = (reed_state == GPIO_PIN_RESET); // LOW = closed, magnet present

    if (!reed_closed && g_load_enabled)
    {
      g_load_enabled = 0;
      g_cycle_mode   = CYCLE_MODE_NONE;   // clear cycle mode on unsafe
      g_pending_tap  = 0;                 // clear pending tap
      HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_RESET);
    }

    // Promote a single tap to a short cycle if no second tap arrived
    if (!g_load_enabled && g_pending_tap)
    {
      uint32_t now_ms = HAL_GetTick();

      if ((now_ms - g_first_tap_ms) > DOUBLE_TAP_WINDOW_MS)
      {
        g_pending_tap  = 0;
        g_cycle_mode   = CYCLE_MODE_SHORT;
        g_load_enabled = 1;
        g_cycle_end_ms = now_ms + CYCLE_DURATION_SHORT_MS;

        HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_SET);
      }
    }

    // Update RGB LED state based on current system condition
    if (g_load_enabled && reed_closed)
    {
      // Cycle in progress → yellow/blue by mode
      if (g_led_state != LED_STATE_CYCLE_RUNNING)
      {
        g_led_state = LED_STATE_CYCLE_RUNNING;
        LED_ApplyState();
      }
    }
    else
    {
      // No cycle / unsafe → idle (off, for now)
      if (g_led_state != LED_STATE_IDLE)
      {
        g_led_state = LED_STATE_IDLE;
        LED_ApplyState();
      }
    }

    // Auto-stop when cycle time is over
    if (g_load_enabled)
    {
      uint32_t now_ms = HAL_GetTick();
      if ((g_cycle_mode != CYCLE_MODE_NONE) && (now_ms >= g_cycle_end_ms))
      {
        g_load_enabled = 0;
        g_cycle_mode   = CYCLE_MODE_NONE;
        g_pending_tap  = 0;   // no tap pending after a finished cycle
        HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_RESET);
        // LED will fall back to IDLE on next loop iteration
      }

      // While active, delay at the same rate we used for the animation
      HAL_Delay(frame_delay_ms);
    }
    else
    {
      // Load is off
      HAL_Delay(10);
    }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 31;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;   // active-low PWM for common-anode RGB
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RST_Pin|CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RST_Pin Control_Pin CS_Pin */
  GPIO_InitStruct.Pin = RST_Pin|Control_Pin|CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : DC_Pin */
  GPIO_InitStruct.Pin = DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : REED_SW_Pin */
  GPIO_InitStruct.Pin = REED_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(REED_SW_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : B2_Pin */
  GPIO_InitStruct.Pin = B2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(B2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B2_Pin)  // user button interrupt
  {
    uint32_t now = HAL_GetTick();

    // Simple debounce: ignore events within 250 ms
    if ((now - g_last_press_ms) < 250)
    {
      return;
    }
    g_last_press_ms = now;

    // Read reed: LOW = closed (magnet present, safe)
    GPIO_PinState reed_state = HAL_GPIO_ReadPin(REED_SW_GPIO_Port, REED_SW_Pin);
    uint8_t reed_closed      = (reed_state == GPIO_PIN_RESET);

    // If reed is open (unsafe), force load OFF and ignore "turn on" requests
    if (!reed_closed)
    {
      g_load_enabled = 0;
      g_cycle_mode   = CYCLE_MODE_NONE;
      g_pending_tap  = 0;
      HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_RESET);
      return;
    }

    // If a cycle is currently running, treat this tap as "cancel"
    if (g_load_enabled)
    {
      g_load_enabled = 0;
      g_cycle_mode   = CYCLE_MODE_NONE;
      g_pending_tap  = 0;

      HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_RESET);
      return;
    }

    // No cycle running here → handle single vs double tap

    if (!g_pending_tap)
    {
      // First tap: start the double-tap window
      g_pending_tap  = 1;
      g_first_tap_ms = now;
    }
    else
    {
      // Second tap within the window?
      if ((now - g_first_tap_ms) <= DOUBLE_TAP_WINDOW_MS)
      {
        // Recognize as double tap → start LONG (3-minute) cycle immediately
        g_pending_tap  = 0;
        g_cycle_mode   = CYCLE_MODE_LONG;
        g_load_enabled = 1;
        g_cycle_end_ms = now + CYCLE_DURATION_LONG_MS;

        HAL_GPIO_WritePin(Control_GPIO_Port, Control_Pin, GPIO_PIN_SET);
      }
      else
      {
        // Too slow; treat this tap as a new "first tap"
        g_first_tap_ms = now;
        // g_pending_tap stays 1
      }
    }
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
  /* User can add his own implementation to report the file name and line number,
     (void)file;
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_REFERENCE_MV 3300U
#define ADC_FULL_SCALE   4095U
#define CURRENT_SETPOINT_MIN_MA 0U
#define CURRENT_SETPOINT_MAX_MA 600U
#define CURRENT_ALARM_THRESHOLD_MA 550U
#define KEY_SCAN_PERIOD_MS 1U
#define KEY_DEBOUNCE_MS 30U
#define ADC_PRINT_PERIOD_MS 500U
#define DISPLAY_REFRESH_PERIOD_MS 50U
#define OLED_I2C_ADDR (0x3CU << 1)
#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_HEIGHT / 8U)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint16_t adc1_value = 0;
uint16_t adc2_value = 0;
uint32_t adc1_voltage_mv = 0;
uint32_t adc2_voltage_mv = 0;
volatile uint16_t current_setpoint_ma = 0;
uint16_t displayed_current_setpoint_ma = 0xFFFFU;
uint8_t oled_ready = 0;
uint8_t oled_buffer[OLED_BUFFER_SIZE] = {0};
char uart_message[128] = "";

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef Read_Dual_ADC_Once(void);
static void Print_Dual_ADC_Values(void);
static void Process_Current_Set_Keys(void);
static void Process_Encoder_Transition(uint16_t GPIO_Pin);
static void Update_Buzzer(void);
static HAL_StatusTypeDef OLED_Init(void);
static void OLED_Show_CurrentSetpoint(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef Read_Dual_ADC_Once(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t dual_adc_value = 0;

  adc1_value = 0;
  adc2_value = 0;
  adc1_voltage_mv = 0;
  adc2_voltage_mv = 0;

  __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC);
  __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_EOC);

  status = HAL_ADC_Start(&hadc2);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADC_Start(&hadc1);
  if (status != HAL_OK)
  {
    HAL_ADC_Stop(&hadc2);
    return status;
  }

  status = HAL_ADC_PollForConversion(&hadc1, 10);
  if (status != HAL_OK)
  {
    HAL_ADC_Stop(&hadc1);
    HAL_ADC_Stop(&hadc2);
    return status;
  }

  dual_adc_value = HAL_ADCEx_MultiModeGetValue(&hadc1);
  adc1_value = (uint16_t)(dual_adc_value & 0xFFFFU);
  adc2_value = (uint16_t)((dual_adc_value >> 16U) & 0xFFFFU);

  adc1_voltage_mv = ((uint32_t)adc1_value * ADC_REFERENCE_MV) / ADC_FULL_SCALE;
  adc2_voltage_mv = ((uint32_t)adc2_value * ADC_REFERENCE_MV) / ADC_FULL_SCALE;

  HAL_ADC_Stop(&hadc1);
  HAL_ADC_Stop(&hadc2);

  return HAL_OK;
}

static void Print_Dual_ADC_Values(void)
{
  int length = snprintf(uart_message,
                        sizeof(uart_message),
                        "Set: %umA | ADC1: %u, %lu.%03luV | ADC2: %u, %lu.%03luV\r\n",
                        current_setpoint_ma,
                        adc1_value,
                        adc1_voltage_mv / 1000UL,
                        adc1_voltage_mv % 1000UL,
                        adc2_value,
                        adc2_voltage_mv / 1000UL,
                        adc2_voltage_mv % 1000UL);

  if (length > 0)
  {
    if ((uint32_t)length >= sizeof(uart_message))
    {
      length = sizeof(uart_message) - 1;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
  }
}

static void Adjust_Current_Setpoint(int16_t delta_ma)
{
  int32_t next_setpoint = (int32_t)current_setpoint_ma + delta_ma;

  if (next_setpoint < (int32_t)CURRENT_SETPOINT_MIN_MA)
  {
    next_setpoint = CURRENT_SETPOINT_MIN_MA;
  }
  else if (next_setpoint > (int32_t)CURRENT_SETPOINT_MAX_MA)
  {
    next_setpoint = CURRENT_SETPOINT_MAX_MA;
  }

  current_setpoint_ma = (uint16_t)next_setpoint;
}

static void Handle_Key(GPIO_TypeDef *gpio_port,
                       uint16_t gpio_pin,
                       GPIO_PinState *last_raw_state,
                       GPIO_PinState *stable_state,
                       uint32_t *last_change_tick,
                       int16_t delta_ma)
{
  GPIO_PinState raw_state = HAL_GPIO_ReadPin(gpio_port, gpio_pin);
  uint32_t now_tick = HAL_GetTick();

  if (raw_state != *last_raw_state)
  {
    *last_raw_state = raw_state;
    *last_change_tick = now_tick;
  }

  if ((now_tick - *last_change_tick) >= KEY_DEBOUNCE_MS)
  {
    if (raw_state != *stable_state)
    {
      *stable_state = raw_state;
      if (*stable_state == GPIO_PIN_RESET)
      {
        Adjust_Current_Setpoint(delta_ma);
      }
    }
  }
}

static void Process_Current_Set_Keys(void)
{
  static GPIO_PinState up_last_raw_state = GPIO_PIN_SET;
  static GPIO_PinState up_stable_state = GPIO_PIN_SET;
  static uint32_t up_last_change_tick = 0;
  static GPIO_PinState down_last_raw_state = GPIO_PIN_SET;
  static GPIO_PinState down_stable_state = GPIO_PIN_SET;
  static uint32_t down_last_change_tick = 0;

  Handle_Key(UP_GPIO_Port,
             UP_Pin,
             &up_last_raw_state,
             &up_stable_state,
             &up_last_change_tick,
             1);
  Handle_Key(DOWN_GPIO_Port,
             DOWN_Pin,
             &down_last_raw_state,
             &down_stable_state,
             &down_last_change_tick,
             -1);
}

static void Process_Encoder_Transition(uint16_t GPIO_Pin)
{
  if (GPIO_Pin != ENC_A_Pin)
  {
    return;
  }

  if (HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin) == GPIO_PIN_RESET)
  {
    if (HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin) == GPIO_PIN_SET)
    {
      Adjust_Current_Setpoint(1);
    }
    else
    {
      Adjust_Current_Setpoint(-1);
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if ((GPIO_Pin == ENC_A_Pin) || (GPIO_Pin == ENC_B_Pin))
  {
    Process_Encoder_Transition(GPIO_Pin);
  }
}

static void Update_Buzzer(void)
{
  GPIO_PinState buzzer_state = (current_setpoint_ma > CURRENT_ALARM_THRESHOLD_MA) ? GPIO_PIN_SET : GPIO_PIN_RESET;

  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, buzzer_state);
}

static const uint8_t *OLED_GetFont5x7(char character)
{
  static const uint8_t space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t exclamation[5] = {0x00, 0x00, 0x5F, 0x00, 0x00};
  static const uint8_t digit0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t digit1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t digit2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t digit3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
  static const uint8_t digit4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t digit5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t digit6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t digit7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t digit8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t digit9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t letterA[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  static const uint8_t letterC[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t letterE[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t letterM[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
  static const uint8_t letterR[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t letterS[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t letterT[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t letterm[5] = {0x7C, 0x04, 0x18, 0x04, 0x78};

  switch (character)
  {
    case '!': return exclamation;
    case '0': return digit0;
    case '1': return digit1;
    case '2': return digit2;
    case '3': return digit3;
    case '4': return digit4;
    case '5': return digit5;
    case '6': return digit6;
    case '7': return digit7;
    case '8': return digit8;
    case '9': return digit9;
    case 'A': return letterA;
    case 'C': return letterC;
    case 'E': return letterE;
    case 'M': return letterM;
    case 'R': return letterR;
    case 'S': return letterS;
    case 'T': return letterT;
    case 'm': return letterm;
    default: return space;
  }
}

static HAL_StatusTypeDef OLED_WriteCommand(uint8_t command)
{
  uint8_t control = 0x00;

  return HAL_I2C_Mem_Write(&hi2c2,
                           OLED_I2C_ADDR,
                           control,
                           I2C_MEMADD_SIZE_8BIT,
                           &command,
                           1,
                           100);
}

static HAL_StatusTypeDef OLED_WriteData(uint8_t *data, uint16_t size)
{
  uint8_t payload[17] = {0};
  uint16_t offset = 0;

  payload[0] = 0x40;
  while (offset < size)
  {
    uint16_t chunk_size = (uint16_t)(size - offset);
    if (chunk_size > 16U)
    {
      chunk_size = 16U;
    }

    for (uint16_t i = 0; i < chunk_size; i++)
    {
      payload[i + 1U] = data[offset + i];
    }

    if (HAL_I2C_Master_Transmit(&hi2c2, OLED_I2C_ADDR, payload, (uint16_t)(chunk_size + 1U), 100) != HAL_OK)
    {
      return HAL_ERROR;
    }

    offset = (uint16_t)(offset + chunk_size);
  }

  return HAL_OK;
}

static void OLED_ClearBuffer(void)
{
  for (uint16_t i = 0; i < OLED_BUFFER_SIZE; i++)
  {
    oled_buffer[i] = 0x00;
  }
}

static void OLED_DrawChar(uint8_t x, uint8_t page, char character)
{
  const uint8_t *font = OLED_GetFont5x7(character);
  uint16_t index = (uint16_t)page * OLED_WIDTH + x;

  if ((page >= 8U) || (x > (OLED_WIDTH - 6U)))
  {
    return;
  }

  for (uint8_t column = 0; column < 5U; column++)
  {
    oled_buffer[index + column] = font[column];
  }
  oled_buffer[index + 5U] = 0x00;
}

static void OLED_DrawString(uint8_t x, uint8_t page, const char *text)
{
  while ((*text != '\0') && (x <= (OLED_WIDTH - 6U)))
  {
    OLED_DrawChar(x, page, *text);
    x = (uint8_t)(x + 6U);
    text++;
  }
}

static HAL_StatusTypeDef OLED_UpdateScreen(void)
{
  for (uint8_t page = 0; page < 8U; page++)
  {
    if (OLED_WriteCommand((uint8_t)(0xB0U + page)) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (OLED_WriteCommand(0x00) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (OLED_WriteCommand(0x10) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (OLED_WriteData(&oled_buffer[(uint16_t)page * OLED_WIDTH], OLED_WIDTH) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  return HAL_OK;
}

static HAL_StatusTypeDef OLED_Init(void)
{
  const uint8_t init_commands[] = {
    0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
    0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
    0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
    0x40, 0x8D, 0x14, 0xAF
  };

  HAL_Delay(100);

  if (HAL_I2C_IsDeviceReady(&hi2c2, OLED_I2C_ADDR, 3, 100) != HAL_OK)
  {
    return HAL_ERROR;
  }

  for (uint8_t i = 0; i < sizeof(init_commands); i++)
  {
    if (OLED_WriteCommand(init_commands[i]) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  OLED_ClearBuffer();
  return OLED_UpdateScreen();
}

static void OLED_Show_CurrentSetpoint(void)
{
  char setpoint_text[16] = "";
  uint16_t setpoint_to_show = current_setpoint_ma;

  if (oled_ready == 0U)
  {
    return;
  }

  OLED_ClearBuffer();
  OLED_DrawString(0, 0, "SET CURRENT");
  snprintf(setpoint_text, sizeof(setpoint_text), "%03umA", setpoint_to_show);
  OLED_DrawString(0, 3, setpoint_text);

  if (setpoint_to_show > CURRENT_ALARM_THRESHOLD_MA)
  {
    OLED_DrawString(0, 6, "ALARM!");
  }

  if (OLED_UpdateScreen() == HAL_OK)
  {
    displayed_current_setpoint_ma = setpoint_to_show;
  }
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
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_ADCEx_Calibration_Start(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_UART_Transmit(&huart1, (uint8_t *)"ADC and PWM started\r\n", 21, 100);
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
  oled_ready = (OLED_Init() == HAL_OK) ? 1U : 0U;
  OLED_Show_CurrentSetpoint();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_adc_print_tick = 0;
  uint32_t last_display_refresh_tick = 0;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Process_Current_Set_Keys();
    Update_Buzzer();
    if ((current_setpoint_ma != displayed_current_setpoint_ma) &&
        ((HAL_GetTick() - last_display_refresh_tick) >= DISPLAY_REFRESH_PERIOD_MS))
    {
      last_display_refresh_tick = HAL_GetTick();
      OLED_Show_CurrentSetpoint();
    }

    if ((HAL_GetTick() - last_adc_print_tick) >= ADC_PRINT_PERIOD_MS)
    {
      last_adc_print_tick = HAL_GetTick();
      if (Read_Dual_ADC_Once() == HAL_OK)
      {
        Print_Dual_ADC_Values();
      }
      else
      {
        HAL_UART_Transmit(&huart1, (uint8_t *)"ADC read failed\r\n", 17, 100);
      }
    }

    HAL_Delay(KEY_SCAN_PERIOD_MS);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_DUALMODE_REGSIMULT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 72-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 100-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 50;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : ENC_A_Pin ENC_B_Pin */
  GPIO_InitStruct.Pin = ENC_A_Pin|ENC_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : UP_Pin DOWN_Pin ENC_SW_Pin */
  GPIO_InitStruct.Pin = UP_Pin|DOWN_Pin|ENC_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZZER_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

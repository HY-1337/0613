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
#include <stdlib.h>
#include <string.h>

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
#define ENC_SW_DEBOUNCE_MS 8U
#define ENCODER_STEP_DEBOUNCE_MS 3U
#define ADC_PRINT_PERIOD_MS 500U
#define DISPLAY_REFRESH_PERIOD_MS 500U
#define LCD_RECOVERY_PERIOD_MS 3000U
#define PWM_DUTY_DEFAULT_LIMIT_PERMILLE 900U
#define PWM_DUTY_HARD_LIMIT_PERMILLE 900U
#define CURRENT_SENSE_RESISTOR_MOHM 5620L
#define ADC1_TRUE_NUMERATOR 11UL
#define ADC1_TRUE_DENOMINATOR 10UL
#define ADC2_TRUE_NUMERATOR 11UL
#define ADC2_TRUE_DENOMINATOR 1UL
#define ADC_AVERAGE_SAMPLES 32U
#define ADC_TRIM_DISCARD_SAMPLES 8U
#define ADC_SAMPLE_SPACING_MS 1U
#define ADC_CURRENT_FIT_SCALE 1000000LL
#define ADC_CURRENT_FIT_GAIN 924258LL
#define ADC_CURRENT_FIT_OFFSET_SCALED 1041609LL
#define ADC_LOW_CURRENT_THRESHOLD_UA 20000UL
#define ADC_VERY_LOW_CURRENT_THRESHOLD_UA 10000UL
#define ADC_VERY_LOW_CURRENT_CORRECTION_NUMERATOR 940UL
#define ADC_VERY_LOW_CURRENT_CORRECTION_DENOMINATOR 1000UL
#define ADC_LOW_CURRENT_FIT_GAIN 915000LL
#define ADC_LOW_CURRENT_FIT_OFFSET_SCALED 0LL
#define ADC_HIGH_CORRECTION_START_UA 100000UL
#define ADC_HIGH_CORRECTION_NUMERATOR 2170UL
#define ADC_HIGH_CORRECTION_DENOMINATOR 100000UL
#define PID_CONTROL_PERIOD_MS 50U
#define PID_STEP_LIMIT_PERMILLE 20L
#define PID_LOW_CURRENT_STEP_LIMIT_PERMILLE 20L
#define PID_MEAS_FILTER_SHIFT 3U
#define PID_LOW_CURRENT_MEAS_FILTER_SHIFT 1U
#define PID_VERY_LOW_CURRENT_MEAS_FILTER_SHIFT 1U
#define PID_DUTY_FINE_SCALE 1000L
#define PID_USE_FEEDFORWARD 0U
#define TIM1_FINE_PWM_PRESCALER 8U
#define TIM1_FINE_PWM_PERIOD_COUNTS 6000U
#define FEEDFORWARD_TABLE_SIZE 51U
#define LCD_COLUMNS 16U
#define LCD_ROWS 2U
#define LCD_I2C_BACKLIGHT 0x08U
#define LCD_I2C_ENABLE 0x04U
#define LCD_I2C_READ_WRITE 0x02U
#define LCD_I2C_REGISTER_SELECT 0x01U
#define LCD_MAP_TEST_MODE 0U
#define LCD_MAP_TEST_HOLD_MS 2500U

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
typedef struct
{
  uint8_t rs;
  uint8_t rw;
  uint8_t en;
  uint8_t bl;
  uint8_t d4;
  uint8_t d5;
  uint8_t d6;
  uint8_t d7;
  const char *name;
} LCD_I2C_MapTypeDef;

const LCD_I2C_MapTypeDef lcd_i2c_maps[] = {
  {0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, "P0RS P4D4"},
  {0x80U, 0x40U, 0x20U, 0x10U, 0x01U, 0x02U, 0x04U, 0x08U, "P7RS P0D4"},
  {0x04U, 0x02U, 0x01U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, "P2RS P4D4"},
  {0x01U, 0x02U, 0x04U, 0x80U, 0x10U, 0x20U, 0x40U, 0x08U, "P0RS D7P3"}
};
const uint8_t lcd_i2c_map_count = (uint8_t)(sizeof(lcd_i2c_maps) / sizeof(lcd_i2c_maps[0]));
uint8_t lcd_i2c_map_index = 0U;
uint16_t adc1_value = 0;
uint16_t adc2_value = 0;
uint32_t adc1_voltage_mv = 0;
uint32_t adc2_voltage_mv = 0;
volatile uint16_t current_setpoint_ma = 0;
uint16_t displayed_current_setpoint_ma = 0xFFFFU;
uint8_t lcd_ready = 0;
uint8_t lcd_test_mode = 0U;
uint16_t lcd_i2c_addr = 0U;
char uart_message[512] = "";
char uart_rx_line[48] = "";
uint8_t uart_rx_line_index = 0;
uint8_t uart_rx_byte = 0;
volatile uint8_t uart_command_ready = 0;
char uart_command_line[48] = "";
int32_t pid_kp_milli = 50;
int32_t pid_ki_milli = 80;
int32_t pid_kd_milli = 0;
uint8_t pid_enabled = 0;
int32_t pid_integral_permille_fine = 0;
int32_t pid_last_error_ua = 0;
int32_t pid_measured_current_ma = 0;
int32_t pid_error_ma = 0;
int32_t pid_measured_current_ua = 0;
int32_t pid_error_ua = 0;
int32_t pid_filtered_current_ua = 0;
uint8_t pid_filter_initialized = 0U;
int32_t pid_feedforward_permille = 0;
int32_t pid_trim_permille = 0;
int32_t pid_p_term_permille = 0;
int32_t pid_p_term_permille_fine = 0;
uint16_t pwm_duty_permille = 0;
uint16_t pwm_duty_limit_permille = PWM_DUTY_DEFAULT_LIMIT_PERMILLE;
volatile uint8_t encoder_switch_press_events = 0;
volatile GPIO_PinState encoder_switch_stable_state = GPIO_PIN_SET;
volatile int16_t encoder_delta_steps = 0;
const uint16_t feedforward_current_ma[FEEDFORWARD_TABLE_SIZE] = {
  0U, 10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U, 90U,
  100U, 110U, 120U, 130U, 140U, 150U, 160U, 170U, 180U, 190U,
  200U, 210U, 220U, 230U, 240U, 250U, 260U, 270U, 280U, 290U,
  300U, 310U, 320U, 330U, 340U, 350U, 360U, 370U, 380U, 390U,
  400U, 410U, 420U, 430U, 440U, 450U, 460U, 470U, 480U, 490U,
  500U
};
const uint16_t feedforward_duty_permille[FEEDFORWARD_TABLE_SIZE] = {
  0U, 18U, 36U, 54U, 71U, 87U, 104U, 120U, 137U, 153U,
  170U, 186U, 202U, 219U, 235U, 251U, 268U, 284U, 301U, 317U,
  333U, 350U, 364U, 378U, 392U, 407U, 424U, 440U, 456U, 472U,
  488U, 504U, 520U, 537U, 553U, 569U, 585U, 602U, 618U, 634U,
  650U, 666U, 682U, 698U, 714U, 730U, 746U, 762U, 778U, 794U,
  810U
};

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
static uint16_t ADC_Trimmed_Average(uint16_t *samples, uint8_t count, uint8_t trim_discard);
static void Print_Dual_ADC_Values(void);
static void Process_Current_Set_Keys(void);
static void Process_Encoder_Transition(uint16_t GPIO_Pin);
static void Process_Encoder_Delta(void);
static void Process_Encoder_Switch_Events(void);
static void Process_UART_Command_Bytes(void);
static void Set_PWM_Duty_Permille(uint16_t duty_permille);
static uint16_t Estimate_Feedforward_Duty_Permille(uint16_t setpoint_ma);
static uint32_t Get_Measured_Current_uA(void);
static uint16_t Get_Measured_Current_Centi_mA(void);
static void Reset_PID_State(void);
static void Prepare_PID_Start(void);
static void Apply_Current_Setpoint(void);
static void Update_PID_Control(void);
static void Update_Buzzer(void);
static HAL_StatusTypeDef LCD_Init(void);
static void LCD_Show_Status(void);
static void LCD_Show_Map_Test(void);
static void LCD_Print_Diagnostic(const char *tag);
static void LCD_Print_I2C_Scan(void);
static void LCD_Clear(void);
void Encoder_Switch_1ms_Task(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef Read_Dual_ADC_Once(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t dual_adc_value = 0;
  uint16_t adc1_samples[ADC_AVERAGE_SAMPLES] = {0};
  uint16_t adc2_samples[ADC_AVERAGE_SAMPLES] = {0};
  uint16_t adc1_trimmed = 0;
  uint16_t adc2_trimmed = 0;
  uint8_t sample = 0;

  adc1_value = 0;
  adc2_value = 0;
  adc1_voltage_mv = 0;
  adc2_voltage_mv = 0;

  for (sample = 0U; sample < ADC_AVERAGE_SAMPLES; sample++)
  {
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
    adc1_samples[sample] = (uint16_t)(dual_adc_value & 0xFFFFU);
    adc2_samples[sample] = (uint16_t)((dual_adc_value >> 16U) & 0xFFFFU);

    HAL_ADC_Stop(&hadc1);
    HAL_ADC_Stop(&hadc2);

    if ((sample + 1U) < ADC_AVERAGE_SAMPLES)
    {
      HAL_Delay(ADC_SAMPLE_SPACING_MS);
    }
  }

  adc1_trimmed = ADC_Trimmed_Average(adc1_samples, ADC_AVERAGE_SAMPLES, ADC_TRIM_DISCARD_SAMPLES);
  adc2_trimmed = ADC_Trimmed_Average(adc2_samples, ADC_AVERAGE_SAMPLES, ADC_TRIM_DISCARD_SAMPLES);

  adc1_value = adc1_trimmed;
  adc2_value = adc2_trimmed;

  adc1_voltage_mv = (((uint32_t)adc1_value * ADC_REFERENCE_MV) / ADC_FULL_SCALE);
  adc2_voltage_mv = (((uint32_t)adc2_value * ADC_REFERENCE_MV) / ADC_FULL_SCALE);
  adc1_voltage_mv = (adc1_voltage_mv * ADC1_TRUE_NUMERATOR) / ADC1_TRUE_DENOMINATOR;
  adc2_voltage_mv = (adc2_voltage_mv * ADC2_TRUE_NUMERATOR) / ADC2_TRUE_DENOMINATOR;

  return HAL_OK;
}

static void Print_Dual_ADC_Values(void)
{
  int length = snprintf(uart_message,
                        sizeof(uart_message),
                        "Set:%umA IMEAS:%ld.%03ldmA ERR:%c%ld.%03ldmA | ADC1:%u,%lu.%03luV ADC2:%u,%lu.%03luV | FF:%ld.%ld%% TRIM:%ld.%ld%% P:%ld.%ld%% I:%ld.%ld%% OUT:%u.%u%% LIM:%u.%u%% | SW:%u EVT:%u | I2C:0x%02X | PID:%s KP:%ld.%03ld KI:%ld.%03ld KD:%ld.%03ld\r\n",
                        current_setpoint_ma,
                        pid_measured_current_ua / 1000L,
                        labs(pid_measured_current_ua % 1000L),
                        (pid_error_ua < 0L) ? '-' : '+',
                        labs(pid_error_ua / 1000L),
                        labs(pid_error_ua % 1000L),
                        adc1_value,
                        adc1_voltage_mv / 1000UL,
                        adc1_voltage_mv % 1000UL,
                        adc2_value,
                        adc2_voltage_mv / 1000UL,
                        adc2_voltage_mv % 1000UL,
                        pid_feedforward_permille / 10L,
                        labs(pid_feedforward_permille % 10L),
                        pid_trim_permille / 10L,
                        labs(pid_trim_permille % 10L),
                        pid_p_term_permille / 10L,
                        labs(pid_p_term_permille % 10L),
                        (pid_integral_permille_fine / PID_DUTY_FINE_SCALE) / 10L,
                        labs((pid_integral_permille_fine / PID_DUTY_FINE_SCALE) % 10L),
                        pwm_duty_permille / 10U,
                        pwm_duty_permille % 10U,
                        pwm_duty_limit_permille / 10U,
                        pwm_duty_limit_permille % 10U,
                        (encoder_switch_stable_state == GPIO_PIN_RESET) ? 0U : 1U,
                        encoder_switch_press_events,
                        (unsigned int)(lcd_i2c_addr >> 1U),
                        pid_enabled ? "ON" : "OFF",
                        pid_kp_milli / 1000L,
                        pid_kp_milli % 1000L,
                        pid_ki_milli / 1000L,
                        pid_ki_milli % 1000L,
                        pid_kd_milli / 1000L,
                        pid_kd_milli % 1000L);

  if (length > 0)
  {
    if ((uint32_t)length >= sizeof(uart_message))
    {
      length = sizeof(uart_message) - 1;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
  }
}

static void UART_SendLine(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
  HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 100);
}

static int32_t Parse_Fixed3(const char *text)
{
  int32_t sign = 1;
  int32_t integer_part = 0;
  int32_t fraction_part = 0;
  uint8_t fraction_digits = 0;

  if (*text == '-')
  {
    sign = -1;
    text++;
  }

  while ((*text >= '0') && (*text <= '9'))
  {
    integer_part = integer_part * 10 + (*text - '0');
    text++;
  }

  if (*text == '.')
  {
    text++;
    while ((*text >= '0') && (*text <= '9') && (fraction_digits < 3U))
    {
      fraction_part = fraction_part * 10 + (*text - '0');
      fraction_digits++;
      text++;
    }
  }

  while (fraction_digits < 3U)
  {
    fraction_part *= 10;
    fraction_digits++;
  }

  return sign * (integer_part * 1000 + fraction_part);
}

static uint16_t Parse_U16_Limited(const char *text, uint16_t min_value, uint16_t max_value)
{
  uint32_t value = 0;

  while ((*text >= '0') && (*text <= '9'))
  {
    value = value * 10U + (uint32_t)(*text - '0');
    text++;
  }

  if (value < min_value)
  {
    value = min_value;
  }
  else if (value > max_value)
  {
    value = max_value;
  }

  return (uint16_t)value;
}

static void Send_PID_Status(void)
{
  int length = snprintf(uart_message,
                        sizeof(uart_message),
                        "STATUS PID:%s SET:%umA DUTY:%u.%u%% LIM:%u.%u%% SW:%u EVT:%u KP:%ld.%03ld KI:%ld.%03ld KD:%ld.%03ld",
                        pid_enabled ? "ON" : "OFF",
                        current_setpoint_ma,
                        pwm_duty_permille / 10U,
                        pwm_duty_permille % 10U,
                        pwm_duty_limit_permille / 10U,
                        pwm_duty_limit_permille % 10U,
                        (encoder_switch_stable_state == GPIO_PIN_RESET) ? 0U : 1U,
                        encoder_switch_press_events,
                        pid_kp_milli / 1000L,
                        pid_kp_milli % 1000L,
                        pid_ki_milli / 1000L,
                        pid_ki_milli % 1000L,
                        pid_kd_milli / 1000L,
                        pid_kd_milli % 1000L);

  if (length > 0)
  {
    if ((uint32_t)length >= sizeof(uart_message))
    {
      length = sizeof(uart_message) - 1;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
    HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 100);
  }
}

static void Process_UART_Command_Line(char *line)
{
  if (strncmp(line, "KP=", 3) == 0)
  {
    pid_kp_milli = Parse_Fixed3(&line[3]);
    UART_SendLine("OK KP");
  }
  else if (strncmp(line, "KI=", 3) == 0)
  {
    pid_ki_milli = Parse_Fixed3(&line[3]);
    UART_SendLine("OK KI");
  }
  else if (strncmp(line, "KD=", 3) == 0)
  {
    pid_kd_milli = Parse_Fixed3(&line[3]);
    UART_SendLine("OK KD");
  }
  else if (strncmp(line, "SET=", 4) == 0)
  {
    current_setpoint_ma = Parse_U16_Limited(&line[4], CURRENT_SETPOINT_MIN_MA, CURRENT_SETPOINT_MAX_MA);
    Apply_Current_Setpoint();
    LCD_Show_Status();
    UART_SendLine("OK SET");
  }
  else if (strncmp(line, "DUTY=", 5) == 0)
  {
    int32_t duty_permille = Parse_Fixed3(&line[5]) / 100L;
    if (duty_permille < 0)
    {
      duty_permille = 0;
    }
    pid_enabled = 0U;
    Set_PWM_Duty_Permille((uint16_t)duty_permille);
    Reset_PID_State();
    LCD_Clear();
    LCD_Show_Status();
    UART_SendLine("OK DUTY");
  }
  else if (strncmp(line, "DUTYMAX=", 8) == 0)
  {
    uint16_t requested_limit = (uint16_t)(Parse_Fixed3(&line[8]) / 100L);
    if (requested_limit > PWM_DUTY_HARD_LIMIT_PERMILLE)
    {
      requested_limit = PWM_DUTY_HARD_LIMIT_PERMILLE;
    }
    pwm_duty_limit_permille = requested_limit;
    if (pwm_duty_permille > pwm_duty_limit_permille)
    {
      Set_PWM_Duty_Permille(pwm_duty_limit_permille);
    }
    UART_SendLine("OK DUTYMAX");
  }
  else if (strcmp(line, "START") == 0)
  {
    Prepare_PID_Start();
    pid_enabled = 1U;
    UART_SendLine("OK START");
  }
  else if (strcmp(line, "STOP") == 0)
  {
    pid_enabled = 0U;
    Set_PWM_Duty_Permille(0);
    Reset_PID_State();
    UART_SendLine("OK STOP");
  }
  else if (strcmp(line, "PIDRESET") == 0)
  {
    Reset_PID_State();
    UART_SendLine("OK PIDRESET");
  }
  else if (strcmp(line, "STATUS") == 0)
  {
    Send_PID_Status();
  }
  else if (strcmp(line, "HELP") == 0)
  {
    UART_SendLine("CMD: KP=0.020 KI=0.001 KD=0.000 SET=100 DUTY=5.0 DUTYMAX=90.0 START STOP PIDRESET STATUS");
  }
  else
  {
    UART_SendLine("ERR UNKNOWN CMD");
  }
}

static void Process_UART_Command_Bytes(void)
{
  if (uart_command_ready != 0U)
  {
    __disable_irq();
    uart_command_ready = 0U;
    __enable_irq();
    Process_UART_Command_Line(uart_command_line);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if ((uart_rx_byte == '\r') || (uart_rx_byte == '\n'))
    {
      if ((uart_rx_line_index > 0U) && (uart_command_ready == 0U))
      {
        uart_rx_line[uart_rx_line_index] = '\0';
        for (uint8_t i = 0; i <= uart_rx_line_index; i++)
        {
          uart_command_line[i] = uart_rx_line[i];
        }
        uart_command_ready = 1U;
        uart_rx_line_index = 0U;
      }
    }
    else if (uart_rx_line_index < (sizeof(uart_rx_line) - 1U))
    {
      uart_rx_line[uart_rx_line_index] = (char)uart_rx_byte;
      uart_rx_line_index++;
    }
    else
    {
      uart_rx_line_index = 0U;
    }

    HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
  }
}

static void Set_PWM_Duty_Permille(uint16_t duty_permille)
{
  uint32_t timer_period_counts = (__HAL_TIM_GET_AUTORELOAD(&htim1) + 1U);
  uint32_t compare_value = 0;

  if (duty_permille > pwm_duty_limit_permille)
  {
    duty_permille = pwm_duty_limit_permille;
  }

  pwm_duty_permille = duty_permille;
  compare_value = (timer_period_counts * pwm_duty_permille) / 1000U;
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare_value);
}

static uint16_t Estimate_Feedforward_Duty_Permille(uint16_t setpoint_ma)
{
  uint8_t index = 0;
  uint32_t duty = 0;
  uint32_t current_span = 0;
  uint32_t duty_span = 0;
  uint32_t current_offset = 0;

  if (PID_USE_FEEDFORWARD == 0U)
  {
    (void)setpoint_ma;
    return 0U;
  }

  if (setpoint_ma <= feedforward_current_ma[0])
  {
    return feedforward_duty_permille[0];
  }

  for (index = 1U; index < FEEDFORWARD_TABLE_SIZE; index++)
  {
    if (setpoint_ma <= feedforward_current_ma[index])
    {
      current_span = (uint32_t)feedforward_current_ma[index] - feedforward_current_ma[index - 1U];
      duty_span = (uint32_t)feedforward_duty_permille[index] - feedforward_duty_permille[index - 1U];
      current_offset = (uint32_t)setpoint_ma - feedforward_current_ma[index - 1U];
      duty = (uint32_t)feedforward_duty_permille[index - 1U] +
             ((duty_span * current_offset) / current_span);
      return (uint16_t)duty;
    }
  }

  return feedforward_duty_permille[FEEDFORWARD_TABLE_SIZE - 1U];
}

static uint32_t Get_Measured_Current_uA(void)
{
  if (pwm_duty_permille == 0U)
  {
    return 0UL;
  }

  uint64_t adc_voltage_uv = ((uint64_t)adc1_value * 3300000ULL * (uint64_t)ADC1_TRUE_NUMERATOR) /
                            ((uint64_t)ADC_FULL_SCALE * (uint64_t)ADC1_TRUE_DENOMINATOR);
  uint32_t measured_current_ua = (uint32_t)((adc_voltage_uv * 1000ULL +
                                            ((uint64_t)CURRENT_SENSE_RESISTOR_MOHM / 2ULL)) /
                                           (uint64_t)CURRENT_SENSE_RESISTOR_MOHM);

  /* DM3058-calibrated piecewise correction */
  if ((measured_current_ua > 0UL) && (measured_current_ua <= ADC_VERY_LOW_CURRENT_THRESHOLD_UA))
  {
    /* <10mA: ADC over-reads raw; 0.940 correction validated at SET=5mA */
    measured_current_ua = (uint32_t)(((uint64_t)measured_current_ua *
                                      (uint64_t)ADC_VERY_LOW_CURRENT_CORRECTION_NUMERATOR +
                                      ((uint64_t)ADC_VERY_LOW_CURRENT_CORRECTION_DENOMINATOR / 2ULL)) /
                                     (uint64_t)ADC_VERY_LOW_CURRENT_CORRECTION_DENOMINATOR);
  }
  else if (measured_current_ua > 12000UL)
  {
    /* >12mA: ADC under-reads by ~2% across range; global boost */
    measured_current_ua = (uint32_t)(((uint64_t)measured_current_ua * 1020ULL + 500ULL) / 1000ULL);
  }

  if (measured_current_ua > 65535000UL)
  {
    measured_current_ua = 65535000UL;
  }

  return measured_current_ua;
}

static uint16_t Get_Measured_Current_Centi_mA(void)
{
  uint32_t measured_current_centi_ma = (Get_Measured_Current_uA() + 5UL) / 10UL;

  if (measured_current_centi_ma > 65535UL)
  {
    measured_current_centi_ma = 65535UL;
  }

  return (uint16_t)measured_current_centi_ma;
}

static void Reset_PID_State(void)
{
  pid_integral_permille_fine = ((int32_t)pwm_duty_permille - pid_feedforward_permille) * PID_DUTY_FINE_SCALE;
  pid_last_error_ua = 0;
  pid_p_term_permille = 0;
  pid_p_term_permille_fine = 0;
  pid_filtered_current_ua = 0;
  pid_filter_initialized = 0U;
  pid_trim_permille = (int32_t)pwm_duty_permille - pid_feedforward_permille;
}

static void Prepare_PID_Start(void)
{
  uint16_t feedforward_duty_permille = Estimate_Feedforward_Duty_Permille(current_setpoint_ma);

  pid_feedforward_permille = (int32_t)feedforward_duty_permille;
  pid_integral_permille_fine = ((int32_t)pwm_duty_permille - pid_feedforward_permille) * PID_DUTY_FINE_SCALE;
  pid_trim_permille = (int32_t)pwm_duty_permille - pid_feedforward_permille;
  pid_p_term_permille = 0;
  pid_p_term_permille_fine = 0;
  pid_last_error_ua = 0;
  pid_filtered_current_ua = 0;
  pid_filter_initialized = 0U;
}

static void Apply_Current_Setpoint(void)
{
  if (current_setpoint_ma == 0U)
  {
    pid_enabled = 0U;
    Set_PWM_Duty_Permille(0);
    Reset_PID_State();
  }
  else
  {
    if (pid_enabled == 0U)
    {
      Prepare_PID_Start();
      pid_enabled = 1U;
    }
    else
    {
      pid_feedforward_permille = (int32_t)Estimate_Feedforward_Duty_Permille(current_setpoint_ma);
      pid_integral_permille_fine = ((int32_t)pwm_duty_permille - pid_feedforward_permille) * PID_DUTY_FINE_SCALE;
      pid_trim_permille = (int32_t)pwm_duty_permille - pid_feedforward_permille;
      pid_p_term_permille = 0;
      pid_p_term_permille_fine = 0;
      pid_last_error_ua = 0;
      pid_filtered_current_ua = 0;
      pid_filter_initialized = 0U;
    }
  }
}

static void Update_PID_Control(void)
{
  int32_t raw_current_ua = (int32_t)Get_Measured_Current_uA();
  int32_t measured_current_ua = raw_current_ua;
  int32_t measured_current_ma = (measured_current_ua + 500L) / 1000L;
  int32_t error_ua = ((int32_t)current_setpoint_ma * 1000L) - measured_current_ua;
  int32_t error_ma = (error_ua >= 0L) ? ((error_ua + 500L) / 1000L) : -(((-error_ua) + 500L) / 1000L);
  int32_t effective_kp_milli = pid_kp_milli;
  int32_t effective_ki_milli = pid_ki_milli;
  int32_t p_term_permille_fine = 0;
  int32_t i_delta_permille_fine = 0;
  int32_t requested_permille_fine = 0;
  int32_t requested_permille = 0;
  int32_t step_permille = 0;
  int32_t active_step_limit_permille = PID_STEP_LIMIT_PERMILLE;
  uint8_t meas_filter_shift = PID_MEAS_FILTER_SHIFT;
  int32_t trim_min_permille_fine = -pid_feedforward_permille * PID_DUTY_FINE_SCALE;
  int32_t trim_max_permille_fine = ((int32_t)pwm_duty_limit_permille - pid_feedforward_permille) * PID_DUTY_FINE_SCALE;

  if (current_setpoint_ma <= 10U)
  {
    meas_filter_shift = PID_VERY_LOW_CURRENT_MEAS_FILTER_SHIFT;
  }
  else if (current_setpoint_ma <= 20U)
  {
    meas_filter_shift = PID_LOW_CURRENT_MEAS_FILTER_SHIFT;
  }

  if (pid_filter_initialized == 0U)
  {
    pid_filtered_current_ua = raw_current_ua;
    pid_filter_initialized = 1U;
  }
  else
  {
    if (meas_filter_shift == 0U)
    {
      pid_filtered_current_ua = raw_current_ua;
    }
    else
    {
      pid_filtered_current_ua += (raw_current_ua - pid_filtered_current_ua) >> meas_filter_shift;
    }
  }

  measured_current_ua = pid_filtered_current_ua;
  measured_current_ma = (measured_current_ua + 500L) / 1000L;
  error_ua = ((int32_t)current_setpoint_ma * 1000L) - measured_current_ua;
  error_ma = (error_ua >= 0L) ? ((error_ua + 500L) / 1000L) : -(((-error_ua) + 500L) / 1000L);

  if (current_setpoint_ma < 20U)
  {
    effective_kp_milli = pid_kp_milli;
    effective_ki_milli = pid_ki_milli;
    active_step_limit_permille = PID_LOW_CURRENT_STEP_LIMIT_PERMILLE;
  }
  else if (current_setpoint_ma < 50U)
  {
    effective_kp_milli = pid_kp_milli;
    effective_ki_milli = pid_ki_milli;
    active_step_limit_permille = 10L;
  }

  if ((pid_kp_milli > 0L) && (effective_kp_milli < 1L))
  {
    effective_kp_milli = 1L;
  }
  if ((pid_ki_milli > 0L) && (effective_ki_milli < 1L))
  {
    effective_ki_milli = 1L;
  }

  p_term_permille_fine = (effective_kp_milli * error_ua) / 100L;
  i_delta_permille_fine = (effective_ki_milli * error_ua) / 100L;

  if (pid_enabled == 0U)
  {
    pid_measured_current_ma = measured_current_ma;
    pid_error_ma = error_ma;
    pid_measured_current_ua = measured_current_ua;
    pid_error_ua = error_ua;
    pid_p_term_permille = 0;
    pid_p_term_permille_fine = 0;
    pid_trim_permille = (int32_t)pwm_duty_permille - pid_feedforward_permille;
    pid_last_error_ua = error_ua;
    return;
  }

  pid_measured_current_ma = measured_current_ma;
  pid_error_ma = error_ma;
  pid_measured_current_ua = measured_current_ua;
  pid_error_ua = error_ua;
  pid_p_term_permille_fine = p_term_permille_fine;
  pid_p_term_permille = (p_term_permille_fine >= 0L) ?
                        ((p_term_permille_fine + (PID_DUTY_FINE_SCALE / 2L)) / PID_DUTY_FINE_SCALE) :
                        -(((-p_term_permille_fine) + (PID_DUTY_FINE_SCALE / 2L)) / PID_DUTY_FINE_SCALE);
  pid_integral_permille_fine += i_delta_permille_fine;

  if (pid_integral_permille_fine < trim_min_permille_fine)
  {
    pid_integral_permille_fine = trim_min_permille_fine;
  }
  else if (pid_integral_permille_fine > trim_max_permille_fine)
  {
    pid_integral_permille_fine = trim_max_permille_fine;
  }

  requested_permille_fine = (pid_feedforward_permille * PID_DUTY_FINE_SCALE) +
                            p_term_permille_fine +
                            pid_integral_permille_fine;

  if (requested_permille_fine < 0L)
  {
    requested_permille_fine = 0L;
  }
  else if (requested_permille_fine > ((int32_t)pwm_duty_limit_permille * PID_DUTY_FINE_SCALE))
  {
    requested_permille_fine = (int32_t)pwm_duty_limit_permille * PID_DUTY_FINE_SCALE;
  }

  requested_permille = (requested_permille_fine + (PID_DUTY_FINE_SCALE / 2L)) / PID_DUTY_FINE_SCALE;
  step_permille = requested_permille - (int32_t)pwm_duty_permille;
  if (step_permille > active_step_limit_permille)
  {
    requested_permille = (int32_t)pwm_duty_permille + active_step_limit_permille;
  }
  else if (step_permille < -active_step_limit_permille)
  {
    requested_permille = (int32_t)pwm_duty_permille - active_step_limit_permille;
  }

  Set_PWM_Duty_Permille((uint16_t)requested_permille);
  pid_trim_permille = (int32_t)pwm_duty_permille - pid_feedforward_permille;
  pid_last_error_ua = error_ua;
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
  Apply_Current_Setpoint();
  LCD_Show_Status();
}

void Encoder_Switch_1ms_Task(void)
{
  static GPIO_PinState last_raw_state = GPIO_PIN_SET;
  static GPIO_PinState debounced_state = GPIO_PIN_SET;
  static uint8_t stable_time_ms = 0;
  GPIO_PinState raw_state = HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin);

  if (raw_state == last_raw_state)
  {
    if (stable_time_ms < ENC_SW_DEBOUNCE_MS)
    {
      stable_time_ms++;
    }
  }
  else
  {
    stable_time_ms = 0;
    last_raw_state = raw_state;
  }

  if ((stable_time_ms >= ENC_SW_DEBOUNCE_MS) && (raw_state != debounced_state))
  {
    debounced_state = raw_state;
    encoder_switch_stable_state = debounced_state;

    if (debounced_state == GPIO_PIN_RESET)
    {
      if (encoder_switch_press_events < 10U)
      {
        encoder_switch_press_events++;
      }
    }
  }
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

static void Process_Encoder_Switch_Events(void)
{
  encoder_switch_press_events = 0U;
}

static void Process_Encoder_Transition(uint16_t GPIO_Pin)
{
  static uint32_t last_step_tick = 0U;
  uint32_t now_tick = HAL_GetTick();

  if (GPIO_Pin != ENC_A_Pin)
  {
    return;
  }

  if (HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin) != GPIO_PIN_RESET)
  {
    return;
  }

  if ((now_tick - last_step_tick) < ENCODER_STEP_DEBOUNCE_MS)
  {
    return;
  }

  last_step_tick = now_tick;
  if (HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin) == GPIO_PIN_SET)
  {
    if (encoder_delta_steps < 20)
    {
      encoder_delta_steps++;
    }
  }
  else
  {
    if (encoder_delta_steps > -20)
    {
      encoder_delta_steps--;
    }
  }
}

static void Process_Encoder_Delta(void)
{
  int16_t delta = 0;

  __disable_irq();
  delta = encoder_delta_steps;
  encoder_delta_steps = 0;
  __enable_irq();

  if (delta != 0)
  {
    Adjust_Current_Setpoint(delta);
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
  GPIO_PinState buzzer_state = (current_setpoint_ma >= CURRENT_ALARM_THRESHOLD_MA) ? GPIO_PIN_RESET : GPIO_PIN_SET;

  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, buzzer_state);
}

static HAL_StatusTypeDef LCD_I2C_WriteExpander(uint8_t data)
{
  return HAL_I2C_Master_Transmit(&hi2c2, lcd_i2c_addr, &data, 1U, 100U);
}

static uint8_t LCD_I2C_BuildByte(uint8_t nibble, uint8_t control)
{
  const LCD_I2C_MapTypeDef *map = &lcd_i2c_maps[lcd_i2c_map_index];
  uint8_t data = map->bl;

  if ((control & LCD_I2C_REGISTER_SELECT) != 0U)
  {
    data |= map->rs;
  }
  if ((control & LCD_I2C_READ_WRITE) != 0U)
  {
    data |= map->rw;
  }
  if ((nibble & 0x01U) != 0U)
  {
    data |= map->d4;
  }
  if ((nibble & 0x02U) != 0U)
  {
    data |= map->d5;
  }
  if ((nibble & 0x04U) != 0U)
  {
    data |= map->d6;
  }
  if ((nibble & 0x08U) != 0U)
  {
    data |= map->d7;
  }

  return data;
}

static HAL_StatusTypeDef LCD_I2C_Write4Bits(uint8_t nibble, uint8_t control)
{
  const LCD_I2C_MapTypeDef *map = &lcd_i2c_maps[lcd_i2c_map_index];
  uint8_t data = LCD_I2C_BuildByte((uint8_t)(nibble & 0x0FU), control);

  if (LCD_I2C_WriteExpander((uint8_t)(data | map->en)) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(1);
  if (LCD_I2C_WriteExpander((uint8_t)(data & (uint8_t)~map->en)) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(1);

  return HAL_OK;
}

static HAL_StatusTypeDef LCD_WriteByte(uint8_t value, uint8_t control)
{
  if (LCD_I2C_Write4Bits((uint8_t)(value >> 4U), control) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (LCD_I2C_Write4Bits((uint8_t)(value & 0x0FU), control) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef LCD_WriteCommand(uint8_t command)
{
  if (LCD_WriteByte(command, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((command == 0x01U) || (command == 0x02U))
  {
    HAL_Delay(2);
  }
  else
  {
    HAL_Delay(1);
  }

  return HAL_OK;
}

static HAL_StatusTypeDef LCD_WriteData(uint8_t data)
{
  if (LCD_WriteByte(data, LCD_I2C_REGISTER_SELECT) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(1);

  return HAL_OK;
}

static void LCD_Clear(void)
{
  if (lcd_ready != 0U)
  {
    (void)LCD_WriteCommand(0x01U);
    HAL_Delay(2);
  }
}

static void LCD_SetCursor(uint8_t row, uint8_t column)
{
  static const uint8_t row_offsets[LCD_ROWS] = {0x00U, 0x40U};

  if (row >= LCD_ROWS)
  {
    row = 0U;
  }
  if (column >= LCD_COLUMNS)
  {
    column = 0U;
  }

  (void)LCD_WriteCommand((uint8_t)(0x80U | (row_offsets[row] + column)));
}

static void LCD_WriteLine(uint8_t row, const char *text)
{
  uint8_t column = 0;

  LCD_SetCursor(row, 0U);
  while ((column < LCD_COLUMNS) && (*text != '\0'))
  {
    (void)LCD_WriteData((uint8_t)*text);
    text++;
    column++;
  }
  while (column < LCD_COLUMNS)
  {
    (void)LCD_WriteData((uint8_t)' ');
    column++;
  }
}

static HAL_StatusTypeDef LCD_Init_Selected_Map(void)
{
  HAL_Delay(50);
  if (LCD_I2C_Write4Bits(0x03U, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(10);
  if (LCD_I2C_Write4Bits(0x03U, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(10);
  if (LCD_I2C_Write4Bits(0x03U, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(10);
  if (LCD_I2C_Write4Bits(0x02U, 0U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(10);

  if (LCD_WriteCommand(0x28U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(5);
  if (LCD_WriteCommand(0x0CU) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(5);
  if (LCD_WriteCommand(0x06U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(5);
  if (LCD_WriteCommand(0x01U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(5);

  return HAL_OK;
}

static HAL_StatusTypeDef LCD_Init(void)
{
  const uint16_t common_addresses[] = {
    (uint16_t)(0x27U << 1U),
    (uint16_t)(0x3FU << 1U),
    (uint16_t)(0x20U << 1U),
    (uint16_t)(0x21U << 1U),
    (uint16_t)(0x22U << 1U),
    (uint16_t)(0x23U << 1U),
    (uint16_t)(0x24U << 1U),
    (uint16_t)(0x25U << 1U),
    (uint16_t)(0x26U << 1U),
    (uint16_t)(0x38U << 1U),
    (uint16_t)(0x39U << 1U),
    (uint16_t)(0x3AU << 1U),
    (uint16_t)(0x3BU << 1U),
    (uint16_t)(0x3CU << 1U),
    (uint16_t)(0x3DU << 1U),
    (uint16_t)(0x3EU << 1U)
  };

  lcd_i2c_addr = 0U;
  for (uint8_t i = 0U; i < (uint8_t)(sizeof(common_addresses) / sizeof(common_addresses[0])); i++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c2, common_addresses[i], 2U, 100U) == HAL_OK)
    {
      lcd_i2c_addr = common_addresses[i];
      break;
    }
  }

  if (lcd_i2c_addr == 0U)
  {
    return HAL_ERROR;
  }

  if (LCD_Init_Selected_Map() != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static uint16_t ADC_Trimmed_Average(uint16_t *samples, uint8_t count, uint8_t trim_discard)
{
  uint8_t i = 0U;
  uint8_t j = 0U;
  uint8_t start = trim_discard;
  uint8_t end = count - trim_discard;
  uint32_t sum = 0UL;
  uint8_t used_count = 0U;

  for (i = 1U; i < count; i++)
  {
    uint16_t key = samples[i];
    j = i;
    while ((j > 0U) && (samples[j - 1U] > key))
    {
      samples[j] = samples[j - 1U];
      j--;
    }
    samples[j] = key;
  }

  if (end <= start)
  {
    start = 0U;
    end = count;
  }

  for (i = start; i < end; i++)
  {
    sum += samples[i];
    used_count++;
  }

  if (used_count == 0U)
  {
    return 0U;
  }

  return (uint16_t)((sum + ((uint32_t)used_count / 2UL)) / (uint32_t)used_count);
}

static void LCD_Show_Status(void)
{
  char line0[17] = "";
  char line1[24] = "";
  uint16_t measured_current_centi_ma = 0U;

  if (lcd_ready == 0U)
  {
    return;
  }

  snprintf(line0,
           sizeof(line0),
           "Set:%3umA",
           current_setpoint_ma);
  if (pid_filter_initialized != 0U)
  {
    measured_current_centi_ma = (uint16_t)((pid_filtered_current_ua + 5L) / 10L);
  }
  else
  {
    measured_current_centi_ma = Get_Measured_Current_Centi_mA();
  }
  snprintf(line1,
           sizeof(line1),
           "I:%3u.%02u D:%2u%%",
           measured_current_centi_ma / 100U,
           measured_current_centi_ma % 100U,
           (pwm_duty_permille + 5U) / 10U);

  LCD_WriteLine(0U, line0);
  LCD_WriteLine(1U, line1);
  displayed_current_setpoint_ma = current_setpoint_ma;
}

static void LCD_Show_Map_Test(void)
{
  int length = 0;

  if (lcd_ready == 0U)
  {
    LCD_Print_Diagnostic("LCDMAP:ERR");
    return;
  }

  if (LCD_Init_Selected_Map() != HAL_OK)
  {
    return;
  }

  LCD_WriteLine(0U, "AAAAAAAAAAAAAAAA");
  LCD_WriteLine(1U, "8888888888888888");

  length = snprintf(uart_message,
                    sizeof(uart_message),
                    "LCDMAP:%u %s ADDR:0x%02X\r\n",
                    (unsigned int)lcd_i2c_map_index,
                    lcd_i2c_maps[lcd_i2c_map_index].name,
                    (unsigned int)(lcd_i2c_addr >> 1U));
  if (length > 0)
  {
    if ((uint32_t)length >= sizeof(uart_message))
    {
      length = sizeof(uart_message) - 1;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
  }
}

static void LCD_Print_Diagnostic(const char *tag)
{
  int length = snprintf(uart_message,
                        sizeof(uart_message),
                        "%s READY:%u MAP:%u ADDR:0x%02X\r\n",
                        tag,
                        (unsigned int)lcd_ready,
                        (unsigned int)lcd_i2c_map_index,
                        (unsigned int)(lcd_i2c_addr >> 1U));

  if (length > 0)
  {
    if ((uint32_t)length >= sizeof(uart_message))
    {
      length = sizeof(uart_message) - 1;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
  }
}

static void LCD_Print_I2C_Scan(void)
{
  uint8_t found_count = 0U;

  UART_SendLine("I2CSCAN:BEGIN");
  for (uint8_t addr = 0x03U; addr <= 0x77U; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr << 1U), 1U, 20U) == HAL_OK)
    {
      int length = snprintf(uart_message,
                            sizeof(uart_message),
                            "I2CSCAN:FOUND 0x%02X\r\n",
                            (unsigned int)addr);
      found_count++;
      if (length > 0)
      {
        if ((uint32_t)length >= sizeof(uart_message))
        {
          length = sizeof(uart_message) - 1;
        }
        HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
      }
    }
  }

  if (found_count == 0U)
  {
    UART_SendLine("I2CSCAN:NONE");
  }
  UART_SendLine("I2CSCAN:END");
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
  HAL_UART_Transmit(&huart1, (uint8_t *)"BOOT USART1 OK\r\n", 16, 100);
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Transmit(&huart1, (uint8_t *)"I2C2 INIT OK\r\n", 14, 100);
  lcd_ready = (LCD_Init() == HAL_OK) ? 1U : 0U;
  if (LCD_MAP_TEST_MODE != 0U)
  {
    LCD_Show_Map_Test();
  }
  else
  {
    LCD_Show_Status();
  }
  {
    int length = snprintf(uart_message,
                          sizeof(uart_message),
                          "LCD:%s ADDR:0x%02X\r\n",
                          (lcd_ready != 0U) ? "OK" : "ERR",
                          (unsigned int)(lcd_i2c_addr >> 1U));
    if (length > 0)
    {
      if ((uint32_t)length >= sizeof(uart_message))
      {
        length = sizeof(uart_message) - 1;
      }
      HAL_UART_Transmit(&huart1, (uint8_t *)uart_message, (uint16_t)length, 100);
    }
  }

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
  Set_PWM_Duty_Permille(0);
  if (HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_UART_Transmit(&huart1, (uint8_t *)"ADC and PWM started\r\n", 21, 100);
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_adc_control_tick = 0;
  uint32_t last_adc_print_tick = 0;
  uint32_t last_display_refresh_tick = 0;
  uint32_t last_lcd_recovery_tick = 0;
  uint8_t lcd_recovery_count = 0U;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Process_UART_Command_Bytes();
    Process_Encoder_Delta();
    Process_Encoder_Switch_Events();
    Process_Current_Set_Keys();
    Update_Buzzer();
    if ((LCD_MAP_TEST_MODE != 0U) &&
        ((HAL_GetTick() - last_display_refresh_tick) >= LCD_MAP_TEST_HOLD_MS))
    {
      last_display_refresh_tick = HAL_GetTick();
      if (lcd_ready == 0U)
      {
        LCD_Print_I2C_Scan();
        lcd_ready = (LCD_Init() == HAL_OK) ? 1U : 0U;
      }
      else
      {
        lcd_i2c_map_index++;
        if (lcd_i2c_map_index >= lcd_i2c_map_count)
        {
          lcd_i2c_map_index = 0U;
        }
      }
      LCD_Show_Map_Test();
    }
    else if ((HAL_GetTick() - last_display_refresh_tick) >= DISPLAY_REFRESH_PERIOD_MS)
    {
      last_display_refresh_tick = HAL_GetTick();
      if (lcd_ready == 0U)
      {
        lcd_ready = (LCD_Init() == HAL_OK) ? 1U : 0U;
      }
      else if ((lcd_recovery_count < 5U) &&
               ((HAL_GetTick() - last_lcd_recovery_tick) >= LCD_RECOVERY_PERIOD_MS))
      {
        last_lcd_recovery_tick = HAL_GetTick();
        lcd_ready = (LCD_Init_Selected_Map() == HAL_OK) ? 1U : 0U;
        lcd_recovery_count++;
      }
      LCD_Show_Status();
    }

    if ((HAL_GetTick() - last_adc_control_tick) >= PID_CONTROL_PERIOD_MS)
    {
      last_adc_control_tick = HAL_GetTick();
      if (Read_Dual_ADC_Once() == HAL_OK)
      {
        Update_PID_Control();
      }
      else
      {
        HAL_UART_Transmit(&huart1, (uint8_t *)"ADC read failed\r\n", 17, 100);
      }
    }

    if ((HAL_GetTick() - last_adc_print_tick) >= ADC_PRINT_PERIOD_MS)
    {
      last_adc_print_tick = HAL_GetTick();
      Print_Dual_ADC_Values();
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
  hi2c2.Init.ClockSpeed = 50000;
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
  sBreakDeadTimeConfig.DeadTime = 72;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  __HAL_TIM_SET_PRESCALER(&htim1, TIM1_FINE_PWM_PRESCALER - 1U);
  __HAL_TIM_SET_AUTORELOAD(&htim1, TIM1_FINE_PWM_PERIOD_COUNTS - 1U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);

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
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);

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

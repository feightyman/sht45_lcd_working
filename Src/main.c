/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ht1621.h"
#include "lcd_qyt12429.h"
#include "sht45.h"

/* Private define ------------------------------------------------------------*/
#define I2C_SPEEDCLOCK             100000U
#define I2C_DUTYCYCLE              I2C_DUTYCYCLE_16_9

#define APP_REFRESH_PERIOD_SECONDS 30U
#define APP_LPTIM_PRESCALER_VALUE  128U
#define APP_LPTIM_RELOAD_VALUE     ((LSI_VALUE / APP_LPTIM_PRESCALER_VALUE) * APP_REFRESH_PERIOD_SECONDS)
#define APP_LPTIM_START_DELAY_US   160U
#define APP_KEY_DEBOUNCE_MS        30U

#define APP_KEY_GPIO_PORT          GPIOA
#define APP_KEY_GPIO_PIN           GPIO_PIN_5

#define APP_GPIOA_ACTIVE_PINS      (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
                                    GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_13 | GPIO_PIN_14)
#define APP_GPIOB_ACTIVE_PINS      (GPIO_PIN_2)

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef UartHandle;
I2C_HandleTypeDef I2cHandle;
LPTIM_HandleTypeDef LptimHandle;

static SHT45_Data_t APP_SensorData;
static uint8_t APP_SensorValid = 0U;
static volatile uint8_t key_irq_pending = 0U;
static volatile uint8_t auto_refresh_pending = 0U;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void App_UnusedGPIO_LowPowerConfig(void);
static void APP_UART_Init(void);
#if !APP_ENABLE_UART_LOG
static void APP_UARTPinsLowPowerConfig(void);
#endif
static void APP_I2C1_Init(void);
static void APP_LPTIM_Init(void);
static void APP_LPTIM_Start30s(void);
static void APP_EnterStopMode(void);
static void APP_KeyInit(void);
static uint8_t APP_KeyIrqRefreshRequested(void);
static uint8_t APP_KeyIsPressed(void);
static uint8_t APP_RefreshMeasurement(const char *reason);
static void APP_UpdateLcd(void);
static void APP_PrintSensorData(const SHT45_Data_t *data);
static void APP_DelayUs(uint32_t delay_us);
#if APP_ENABLE_UART_LOG
static void UART_Print(const char *text);
static void UART_PrintUnsigned(uint32_t value);
static void UART_PrintSignedFixed1(int32_t value_x100);
#define APP_LOG(...)                UART_Print(__VA_ARGS__)
#else
#define APP_LOG(...)                do { } while (0)
#endif

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  const char *refresh_reason;

  HAL_Init();

  App_UnusedGPIO_LowPowerConfig();
  APP_UART_Init();
  APP_LOG("\r\nSystem start\r\n");

  APP_I2C1_Init();
  SHT45_Init(&I2cHandle);
  APP_KeyInit();
  APP_LPTIM_Init();

  HT1621_Init();
  LCD_QYT12429_Init();
  LCD_QYT12429_Clear();

  (void)APP_RefreshMeasurement("start");
  APP_LPTIM_Start30s();

  while (1)
  {
    refresh_reason = NULL;

    if (APP_KeyIrqRefreshRequested() != 0U)
    {
      auto_refresh_pending = 0U;
      APP_LOG("KEY pressed, refresh now\r\n");
      refresh_reason = "key";
    }
    else if (auto_refresh_pending != 0U)
    {
      auto_refresh_pending = 0U;
      refresh_reason = "auto";
    }

    if (refresh_reason != NULL)
    {
      (void)APP_RefreshMeasurement(refresh_reason);
      APP_LPTIM_Start30s();
    }

    if ((key_irq_pending == 0U) && (auto_refresh_pending == 0U))
    {
      APP_EnterStopMode();
    }
  }
}

/**
  * @brief  Put currently unused GPIO pins into analog/no-pull state.
  * @retval None
  */
static void App_UnusedGPIO_LowPowerConfig(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
#if defined(GPIOF)
  __HAL_RCC_GPIOF_CLK_ENABLE();
#endif

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  GPIO_InitStruct.Pin = (uint16_t)(GPIO_PIN_All & (uint16_t)(~APP_GPIOA_ACTIVE_PINS));
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = (uint16_t)(GPIO_PIN_All & (uint16_t)(~APP_GPIOB_ACTIVE_PINS));
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

#if defined(GPIOF)
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
#endif
}

/**
  * @brief  Restore system clock to HSISYS after Stop wakeup.
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
#if defined(RCC_HSIDIV_SUPPORT)
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
#endif
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_8MHz;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSISYS;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  Initialize USART1 debug output.
  * @retval None
  */
static void APP_UART_Init(void)
{
#if APP_ENABLE_UART_LOG
  UartHandle.Instance          = USART1;
  UartHandle.Init.BaudRate     = 115200;
  UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
  UartHandle.Init.StopBits     = UART_STOPBITS_1;
  UartHandle.Init.Parity       = UART_PARITY_NONE;
  UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  UartHandle.Init.Mode         = UART_MODE_TX_RX;
  UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
  UartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  if (HAL_UART_Init(&UartHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }
#else
  APP_UARTPinsLowPowerConfig();
#endif
}

/**
  * @brief  Put USART1 pins in low-power state when UART logging is disabled.
  * @retval None
  */
#if !APP_ENABLE_UART_LOG
static void APP_UARTPinsLowPowerConfig(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_DISABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
#endif

/**
  * @brief  Initialize I2C1 for SHT45.
  * @retval None
  */
static void APP_I2C1_Init(void)
{
  I2cHandle.Instance             = I2C;
  I2cHandle.Init.ClockSpeed      = I2C_SPEEDCLOCK;
  I2cHandle.Init.DutyCycle       = I2C_DUTYCYCLE;
  I2cHandle.Init.OwnAddress1     = 0x00;
  I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&I2cHandle) != HAL_OK)
  {
    APP_LOG("I2C1 init failed\r\n");
    APP_ErrorHandler();
  }
}

/**
  * @brief  Initialize LPTIM with LSI clock for 30 s auto refresh wakeups.
  * @retval None
  */
static void APP_LPTIM_Init(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM;
  RCC_PeriphCLKInitStruct.LptimClockSelection = RCC_LPTIMCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  __HAL_RCC_LPTIM_CLK_ENABLE();

  LptimHandle.Instance = LPTIM;
  LptimHandle.Init.Prescaler = LPTIM_PRESCALER_DIV128;
  LptimHandle.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;

  if (HAL_LPTIM_Init(&LptimHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  HAL_NVIC_SetPriority(LPTIM1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
}

/**
  * @brief  Start a single 30 s LPTIM auto-refresh period.
  * @retval None
  */
static void APP_LPTIM_Start30s(void)
{
  __HAL_LPTIM_DISABLE(&LptimHandle);
  __HAL_LPTIM_CLEAR_FLAG(&LptimHandle, LPTIM_FLAG_ARRM);
  __HAL_LPTIM_ENABLE_IT(&LptimHandle, LPTIM_IT_ARRM);
  __HAL_LPTIM_ENABLE(&LptimHandle);
  __HAL_LPTIM_AUTORELOAD_SET(&LptimHandle, APP_LPTIM_RELOAD_VALUE);
  APP_DelayUs(APP_LPTIM_START_DELAY_US);
  __HAL_LPTIM_START_SINGLE(&LptimHandle);
}

/**
  * @brief  Enter Stop mode until PA5 EXTI or LPTIM wakes the MCU.
  * @retval None
  */
static void APP_EnterStopMode(void)
{
  PWR_StopModeConfigTypeDef PwrStopModeConf = {0};

  __disable_irq();

  if ((key_irq_pending != 0U) || (auto_refresh_pending != 0U))
  {
    __enable_irq();
    return;
  }

  HAL_SuspendTick();

  __HAL_RCC_PWR_CLK_ENABLE();

  PwrStopModeConf.LPVoltSelection = PWR_STOPMOD_LPR_VOLT_SCALE2;
  PwrStopModeConf.FlashDelay = PWR_WAKEUP_FLASH_DELAY_5US;
  PwrStopModeConf.WakeUpHsiEnableTime = PWR_WAKEUP_HSIEN_AFTER_MR;
  PwrStopModeConf.RegulatorSwitchDelay = PWR_WAKEUP_LPR_TO_MR_DELAY_2US;
  PwrStopModeConf.SramRetentionVolt = PWR_SRAM_RETENTION_VOLT_VOS;
  if (HAL_PWR_ConfigStopMode(&PwrStopModeConf) != HAL_OK)
  {
    HAL_ResumeTick();
    __enable_irq();
    APP_ErrorHandler();
  }

  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

  SystemClock_Config();
  HAL_ResumeTick();

  __enable_irq();
}

/**
  * @brief  Initialize PA5 key. Pressed state is low.
  * @retval None
  */
static void APP_KeyInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = APP_KEY_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(APP_KEY_GPIO_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/**
  * @brief  Confirm pending PA5 EXTI event with 30 ms debounce.
  * @retval 1 when a pressed key event is confirmed, otherwise 0.
  */
static uint8_t APP_KeyIrqRefreshRequested(void)
{
  if (key_irq_pending == 0U)
  {
    return 0U;
  }

  key_irq_pending = 0U;
  HAL_Delay(APP_KEY_DEBOUNCE_MS);

  if (APP_KeyIsPressed() == 0U)
  {
    key_irq_pending = 0U;
    return 0U;
  }

  key_irq_pending = 0U;

  return 1U;
}

static uint8_t APP_KeyIsPressed(void)
{
  return (HAL_GPIO_ReadPin(APP_KEY_GPIO_PORT, APP_KEY_GPIO_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

/**
  * @brief  Read SHT45, update LCD cache and print debug output.
  * @retval 1 if refresh succeeded, otherwise 0.
  */
static uint8_t APP_RefreshMeasurement(const char *reason)
{
  UNUSED(reason);

  if (SHT45_ReadTempHumi(&APP_SensorData) != HAL_OK)
  {
    APP_LOG("SHT45 read failed or CRC error\r\n");
    return 0U;
  }

  APP_SensorValid = 1U;
  APP_PrintSensorData(&APP_SensorData);
  APP_UpdateLcd();

  return 1U;
}

static void APP_UpdateLcd(void)
{
  if (APP_SensorValid == 0U)
  {
    return;
  }

  LCD_QYT12429_DisplayTemperature(APP_SensorData.temperature_x100);
  LCD_QYT12429_DisplayHumidity(APP_SensorData.humidity_x100);
}

static void APP_PrintSensorData(const SHT45_Data_t *data)
{
#if APP_ENABLE_UART_LOG
  APP_LOG("T=");
  UART_PrintSignedFixed1(data->temperature_x100);
  APP_LOG(" C, RH=");
  UART_PrintSignedFixed1(data->humidity_x100);
  APP_LOG(" %\r\n");
#else
  UNUSED(data);
#endif
}

static void APP_DelayUs(uint32_t delay_us)
{
  __IO uint32_t delay = 1U + (delay_us * (SystemCoreClock / 24U)) / 1000000U;

  do
  {
    __NOP();
  }
  while (delay-- != 0U);
}

/**
  * @brief  Send a null-terminated string through USART1.
  * @param  text String to send.
  * @retval None
  */
#if APP_ENABLE_UART_LOG
static void UART_Print(const char *text)
{
  uint16_t length = 0;

  while (text[length] != '\0')
  {
    length++;
  }

  if (length > 0U)
  {
    if (HAL_UART_Transmit(&UartHandle, (uint8_t *)text, length, 5000) != HAL_OK)
    {
      APP_ErrorHandler();
    }
  }
}

/**
  * @brief  Print an unsigned decimal integer.
  * @param  value Value to print.
  * @retval None
  */
static void UART_PrintUnsigned(uint32_t value)
{
  char text[10];
  uint8_t index = 0;

  if (value == 0U)
  {
    UART_Print("0");
    return;
  }

  while (value > 0U)
  {
    text[index++] = (char)('0' + (value % 10U));
    value /= 10U;
  }

  while (index > 0U)
  {
    char digit[2];

    digit[0] = text[--index];
    digit[1] = '\0';
    UART_Print(digit);
  }
}

static void UART_PrintSignedFixed1(int32_t value_x100)
{
  int32_t value_x10;
  uint32_t absolute;
  uint32_t fractional;

  if (value_x100 >= 0)
  {
    value_x10 = (value_x100 + 5) / 10;
  }
  else
  {
    value_x10 = -(((-value_x100) + 5) / 10);
  }

  if (value_x10 < 0)
  {
    UART_Print("-");
    absolute = (uint32_t)(-value_x10);
  }
  else
  {
    absolute = (uint32_t)value_x10;
  }

  fractional = absolute % 10U;

  UART_PrintUnsigned(absolute / 10U);
  UART_Print(".");
  UART_PrintUnsigned(fractional);
}
#endif

/**
  * @brief  EXTI line detection callback.
  * @param  GPIO_Pin GPIO pin that triggered EXTI.
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == APP_KEY_GPIO_PIN)
  {
    key_irq_pending = 1U;
  }
}

/**
  * @brief  LPTIM auto-reload match callback.
  * @param  hlptim LPTIM handle.
  * @retval None
  */
void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
  if (hlptim->Instance == LPTIM)
  {
    auto_refresh_pending = 1U;
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void APP_ErrorHandler(void)
{
  while (1)
  {
  }
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
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE****/

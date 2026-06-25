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

#define APP_REFRESH_INTERVAL_MS    30000U
#define APP_KEY_DEBOUNCE_MS        30U

#define APP_KEY_GPIO_PORT          GPIOA
#define APP_KEY_GPIO_PIN           GPIO_PIN_5

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef UartHandle;
I2C_HandleTypeDef I2cHandle;

static SHT45_Data_t APP_SensorData;
static uint8_t APP_SensorValid = 0U;
static uint8_t APP_KeyLastSample = 0U;
static uint8_t APP_KeyStablePressed = 0U;
static uint32_t APP_KeyLastChangeTick = 0U;

/* Private function prototypes -----------------------------------------------*/
static void APP_UART_Init(void);
static void APP_I2C1_Init(void);
static void APP_KeyInit(void);
static uint8_t APP_KeyShortPressed(void);
static uint8_t APP_KeyIsPressed(void);
static uint8_t APP_RefreshSensor(void);
static void APP_UpdateLcd(void);
static void APP_PrintSensorData(const SHT45_Data_t *data);
static void UART_Print(const char *text);
static void UART_PrintUnsigned(uint32_t value);
static void UART_PrintSignedFixed1(int32_t value_x100);

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  uint32_t last_refresh_tick;
  uint32_t now_tick;
  uint8_t refresh_requested;

  HAL_Init();

  APP_UART_Init();
  UART_Print("\r\nSystem start\r\n");

  APP_I2C1_Init();
  SHT45_Init(&I2cHandle);
  APP_KeyInit();

  HT1621_Init();
  LCD_QYT12429_Init();
  LCD_QYT12429_Clear();

  (void)APP_RefreshSensor();
  last_refresh_tick = HAL_GetTick();

  while (1)
  {
    refresh_requested = APP_KeyShortPressed();
    now_tick = HAL_GetTick();

    if ((refresh_requested != 0U) ||
        ((uint32_t)(now_tick - last_refresh_tick) >= APP_REFRESH_INTERVAL_MS))
    {
      if (refresh_requested != 0U)
      {
        UART_Print("KEY pressed, refresh now\r\n");
      }

      (void)APP_RefreshSensor();
      last_refresh_tick = HAL_GetTick();
    }

    HAL_Delay(10);
  }
}

/**
  * @brief  Initialize USART1 debug output.
  * @retval None
  */
static void APP_UART_Init(void)
{
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
}

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
    UART_Print("I2C1 init failed\r\n");
    APP_ErrorHandler();
  }
}

/**
  * @brief  Initialize PA5 key. Pressed state is low.
  * @retval None
  */
static void APP_KeyInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = APP_KEY_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(APP_KEY_GPIO_PORT, &GPIO_InitStruct);

  APP_KeyLastSample = APP_KeyIsPressed();
  APP_KeyStablePressed = APP_KeyLastSample;
  APP_KeyLastChangeTick = HAL_GetTick();
}

/**
  * @brief  Detect debounced short press event.
  * @retval 1 when a new press is detected, otherwise 0.
  */
static uint8_t APP_KeyShortPressed(void)
{
  uint8_t current_sample;
  uint32_t now_tick;

  current_sample = APP_KeyIsPressed();
  now_tick = HAL_GetTick();

  if (current_sample != APP_KeyLastSample)
  {
    APP_KeyLastSample = current_sample;
    APP_KeyLastChangeTick = now_tick;
  }

  if (((uint32_t)(now_tick - APP_KeyLastChangeTick) >= APP_KEY_DEBOUNCE_MS) &&
      (current_sample != APP_KeyStablePressed))
  {
    APP_KeyStablePressed = current_sample;

    if (APP_KeyStablePressed != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static uint8_t APP_KeyIsPressed(void)
{
  return (HAL_GPIO_ReadPin(APP_KEY_GPIO_PORT, APP_KEY_GPIO_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

/**
  * @brief  Read SHT45, update LCD cache and print debug output.
  * @retval 1 if refresh succeeded, otherwise 0.
  */
static uint8_t APP_RefreshSensor(void)
{
  if (SHT45_ReadTempHumi(&APP_SensorData) != HAL_OK)
  {
    UART_Print("SHT45 read failed or CRC error\r\n");
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
  UART_Print("T=");
  UART_PrintSignedFixed1(data->temperature_x100);
  UART_Print(" C, RH=");
  UART_PrintSignedFixed1(data->humidity_x100);
  UART_Print(" %\r\n");
}

/**
  * @brief  Send a null-terminated string through USART1.
  * @param  text String to send.
  * @retval None
  */
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

/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   Main program body
  * @date
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
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private define ------------------------------------------------------------*/
#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))
#define TXSTARTMESSAGESIZE    (COUNTOF(aTxStartMessage) - 1)
#define TXENDMESSAGESIZE      (COUNTOF(aTxEndMessage) - 1)
#define I2C_SPEEDCLOCK        100000U
#define I2C_DUTYCYCLE         I2C_DUTYCYCLE_16_9
#define SHT45_ADDR            0x44U
#define SHT45_CMD_MEASURE     0xFDU

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef UartHandle;
I2C_HandleTypeDef I2cHandle;
uint8_t aTxStartMessage[] = "\r\n UART Hyperterminal communication based on Polling\r\n Enter 12 characters using keyboard :\r\n";
uint8_t aTxEndMessage[] = "\r\n Example Finished\r\n";
uint8_t aRxBuffer[12] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* Private function prototypes -----------------------------------------------*/
/* Private user code ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void APP_I2C1_Init(void);
static void APP_I2CScan(void);
static HAL_StatusTypeDef SHT45_ReadTempHumi(int32_t *temperature_x100, int32_t *humidity_x100);
static void UART_Print(const char *text);
static void UART_PrintHexByte(uint8_t value);
static void UART_PrintUnsigned(uint32_t value);
static void UART_PrintSignedFixed2(int32_t value);

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Systick */
  HAL_Init();
  
  /* Initialize LED */
  BSP_LED_Init(LED_GREEN);
  
  /* Initialize USART */
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

  APP_I2C1_Init();
  APP_I2CScan();

  /* Start the transmission process */
  if(HAL_UART_Transmit(&UartHandle, (uint8_t*)aTxStartMessage, TXSTARTMESSAGESIZE, 5000)!= HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Turn on LED */
  BSP_LED_On(LED_GREEN);

  while (1)
  {
    int32_t temperature_x100 = 0;
    int32_t humidity_x100 = 0;

    if (SHT45_ReadTempHumi(&temperature_x100, &humidity_x100) == HAL_OK)
    {
      UART_Print("SHT45 T=");
      UART_PrintSignedFixed2(temperature_x100);
      UART_Print(" C, RH=");
      UART_PrintSignedFixed2(humidity_x100);
      UART_Print(" %\r\n");
    }
    else
    {
      UART_Print("SHT45 read failed\r\n");
    }

    HAL_Delay(1000);
  }
}

/**
  * @brief  Initialize I2C1 for SHT45 bus scan.
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
    UART_Print("\r\nI2C1 init failed\r\n");
    APP_ErrorHandler();
  }
}

/**
  * @brief  Scan 7-bit I2C addresses and print detected devices.
  * @retval None
  */
static void APP_I2CScan(void)
{
  uint8_t address;
  uint8_t found = 0;

  UART_Print("\r\nI2C1 scan start: 0x08-0x77\r\n");

  for (address = 0x08; address <= 0x77; address++)
  {
    if (HAL_I2C_IsDeviceReady(&I2cHandle, (uint16_t)(address << 1), 2, 10) == HAL_OK)
    {
      found++;
      UART_Print("I2C device found at 0x");
      UART_PrintHexByte(address);
      UART_Print("\r\n");

      if (address == SHT45_ADDR)
      {
        UART_Print("SHT45 found at 0x44\r\n");
      }
    }
  }

  if (found == 0)
  {
    UART_Print("No I2C device found\r\n");
  }

  UART_Print("I2C1 scan done\r\n");
}

/**
  * @brief  Read SHT45 temperature and humidity.
  * @param  temperature_x100 Temperature in centi-degrees Celsius.
  * @param  humidity_x100 Relative humidity in centi-percent.
  * @retval HAL status
  */
static HAL_StatusTypeDef SHT45_ReadTempHumi(int32_t *temperature_x100, int32_t *humidity_x100)
{
  uint8_t command = SHT45_CMD_MEASURE;
  uint8_t data[6] = {0};
  uint16_t temp_raw;
  uint16_t humi_raw;
  int32_t humidity;

  if (HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)(SHT45_ADDR << 1), &command, 1, 100) != HAL_OK)
  {
    return HAL_ERROR;
  }

  HAL_Delay(10);

  if (HAL_I2C_Master_Receive(&I2cHandle, (uint16_t)(SHT45_ADDR << 1), data, sizeof(data), 100) != HAL_OK)
  {
    return HAL_ERROR;
  }

  UART_Print("SHT45 raw: ");
  UART_Print("0x");
  UART_PrintHexByte(data[0]);
  UART_Print(" 0x");
  UART_PrintHexByte(data[1]);
  UART_Print(" 0x");
  UART_PrintHexByte(data[2]);
  UART_Print(" 0x");
  UART_PrintHexByte(data[3]);
  UART_Print(" 0x");
  UART_PrintHexByte(data[4]);
  UART_Print(" 0x");
  UART_PrintHexByte(data[5]);
  UART_Print("\r\n");

  temp_raw = ((uint16_t)data[0] << 8) | data[1];
  humi_raw = ((uint16_t)data[3] << 8) | data[4];

  *temperature_x100 = -4500 + (int32_t)(((uint32_t)17500 * temp_raw) / 65535U);
  humidity = -600 + (int32_t)(((uint32_t)12500 * humi_raw) / 65535U);

  if (humidity < 0)
  {
    humidity = 0;
  }
  else if (humidity > 10000)
  {
    humidity = 10000;
  }

  *humidity_x100 = humidity;

  return HAL_OK;
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

  if (length > 0)
  {
    if (HAL_UART_Transmit(&UartHandle, (uint8_t *)text, length, 5000) != HAL_OK)
    {
      APP_ErrorHandler();
    }
  }
}

/**
  * @brief  Print an 8-bit value as two uppercase hexadecimal digits.
  * @param  value Value to print.
  * @retval None
  */
static void UART_PrintHexByte(uint8_t value)
{
  static const char hex[] = "0123456789ABCDEF";
  char text[3];

  text[0] = hex[(value >> 4) & 0x0F];
  text[1] = hex[value & 0x0F];
  text[2] = '\0';

  UART_Print(text);
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

  if (value == 0)
  {
    UART_Print("0");
    return;
  }

  while (value > 0)
  {
    text[index++] = (char)('0' + (value % 10U));
    value /= 10U;
  }

  while (index > 0)
  {
    char digit[2];

    digit[0] = text[--index];
    digit[1] = '\0';
    UART_Print(digit);
  }
}

/**
  * @brief  Print a signed fixed-point value with two decimals.
  * @param  value Value scaled by 100.
  * @retval None
  */
static void UART_PrintSignedFixed2(int32_t value)
{
  uint32_t absolute;
  uint32_t fractional;

  if (value < 0)
  {
    UART_Print("-");
    absolute = (uint32_t)(-value);
  }
  else
  {
    absolute = (uint32_t)value;
  }

  fractional = absolute % 100U;

  UART_PrintUnsigned(absolute / 100U);
  UART_Print(".");

  if (fractional < 10U)
  {
    UART_Print("0");
  }

  UART_PrintUnsigned(fractional);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void APP_ErrorHandler(void)
{
  /* infinite loop */
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
  /* User can add his own implementation to report the file name and line number,
     for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE****/

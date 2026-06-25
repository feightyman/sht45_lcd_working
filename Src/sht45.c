/**
  ******************************************************************************
  * @file    sht45.c
  * @brief   SHT45 temperature and humidity sensor driver.
  ******************************************************************************
  */

#include "sht45.h"

#define SHT45_CMD_MEASURE_HIGH_PRECISION     0xFDU
#define SHT45_CRC_POLYNOMIAL                 0x31U
#define SHT45_CRC_INIT                       0xFFU

static I2C_HandleTypeDef *SHT45_I2cHandle;

static uint8_t SHT45_CalcCrc(const uint8_t *data, uint8_t length);

/**
  * @brief  Bind the SHT45 driver to the initialized I2C handle.
  * @param  hi2c HAL I2C handle.
  * @retval None
  */
void SHT45_Init(I2C_HandleTypeDef *hi2c)
{
  SHT45_I2cHandle = hi2c;
}

/**
  * @brief  Read SHT45 temperature and humidity with CRC validation.
  * @param  result Sensor result. Temperature is in centi-degrees Celsius,
  *         humidity is in centi-percent.
  * @retval HAL status.
  */
HAL_StatusTypeDef SHT45_ReadTempHumi(SHT45_Data_t *result)
{
  uint8_t command = SHT45_CMD_MEASURE_HIGH_PRECISION;
  uint8_t index;
  int32_t humidity;

  if ((SHT45_I2cHandle == NULL) || (result == NULL))
  {
    return HAL_ERROR;
  }

  if (HAL_I2C_Master_Transmit(SHT45_I2cHandle, (uint16_t)(SHT45_I2C_ADDR_7BIT << 1),
                              &command, 1, 100) != HAL_OK)
  {
    return HAL_ERROR;
  }

  HAL_Delay(10);

  if (HAL_I2C_Master_Receive(SHT45_I2cHandle, (uint16_t)(SHT45_I2C_ADDR_7BIT << 1),
                             result->raw, sizeof(result->raw), 100) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((SHT45_CheckCrc(&result->raw[0], 2, result->raw[2]) == 0U) ||
      (SHT45_CheckCrc(&result->raw[3], 2, result->raw[5]) == 0U))
  {
    for (index = 0; index < sizeof(result->raw); index++)
    {
      result->raw[index] = 0U;
    }
    return HAL_ERROR;
  }

  result->temp_raw = ((uint16_t)result->raw[0] << 8) | result->raw[1];
  result->humi_raw = ((uint16_t)result->raw[3] << 8) | result->raw[4];

  result->temperature_x100 = -4500 + (int32_t)(((uint32_t)17500 * result->temp_raw) / 65535U);
  humidity = -600 + (int32_t)(((uint32_t)12500 * result->humi_raw) / 65535U);

  if (humidity < 0)
  {
    humidity = 0;
  }
  else if (humidity > 10000)
  {
    humidity = 10000;
  }

  result->humidity_x100 = humidity;

  return HAL_OK;
}

/**
  * @brief  Check SHT45 CRC byte.
  * @param  data Data buffer.
  * @param  length Data length.
  * @param  checksum Expected CRC.
  * @retval 1 if CRC matches, otherwise 0.
  */
uint8_t SHT45_CheckCrc(const uint8_t *data, uint8_t length, uint8_t checksum)
{
  return (SHT45_CalcCrc(data, length) == checksum) ? 1U : 0U;
}

static uint8_t SHT45_CalcCrc(const uint8_t *data, uint8_t length)
{
  uint8_t crc = SHT45_CRC_INIT;
  uint8_t byte_index;
  uint8_t bit_index;

  for (byte_index = 0; byte_index < length; byte_index++)
  {
    crc ^= data[byte_index];

    for (bit_index = 0; bit_index < 8U; bit_index++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1) ^ SHT45_CRC_POLYNOMIAL);
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

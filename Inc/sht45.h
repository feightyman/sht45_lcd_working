/**
  ******************************************************************************
  * @file    sht45.h
  * @brief   SHT45 temperature and humidity sensor driver.
  ******************************************************************************
  */

#ifndef __SHT45_H
#define __SHT45_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"

#define SHT45_I2C_ADDR_7BIT       0x44U

typedef struct
{
  uint8_t raw[6];
  uint16_t temp_raw;
  uint16_t humi_raw;
  int32_t temperature_x100;
  int32_t humidity_x100;
} SHT45_Data_t;

void SHT45_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef SHT45_ReadTempHumi(SHT45_Data_t *result);
uint8_t SHT45_CheckCrc(const uint8_t *data, uint8_t length, uint8_t checksum);

#ifdef __cplusplus
}
#endif

#endif /* __SHT45_H */

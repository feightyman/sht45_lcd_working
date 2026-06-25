/**
  ******************************************************************************
  * @file    ht1621.h
  * @brief   HT1621 three-wire LCD controller driver.
  ******************************************************************************
  */

#ifndef __HT1621_H
#define __HT1621_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"

#define HT1621_LCD_SEG_COUNT      16U
#define HT1621_LCD_COM_COUNT      4U
#define HT1621_RAM_ADDRESS_COUNT  32U

void HT1621_Init(void);
void HT1621_Clear(void);
void HT1621_AllOn(void);
void HT1621_AllOnRaw(void);
void HT1621_AllOffRaw(void);
void HT1621_AddressScanTest(void);
void HT1621_Write4Bit(uint8_t address, uint8_t data);
void HT1621_WriteNibble(uint8_t address, uint8_t data);
void HT1621_SetSegment(uint8_t seg, uint8_t com, uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif /* __HT1621_H */

/**
  ******************************************************************************
  * @file    lcd_qyt12429.h
  * @brief   QYT12429 segment LCD display helpers.
  ******************************************************************************
  */

#ifndef __LCD_QYT12429_H
#define __LCD_QYT12429_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"

typedef enum
{
  LCD_QYT12429_ICON_S1 = 1,
  LCD_QYT12429_ICON_S2,
  LCD_QYT12429_ICON_S3,
  LCD_QYT12429_ICON_S4,
  LCD_QYT12429_ICON_S5,
  LCD_QYT12429_ICON_S6,
  LCD_QYT12429_ICON_S7,
  LCD_QYT12429_ICON_S8,
  LCD_QYT12429_ICON_S9
} LCD_QYT12429_Icon_t;

void LCD_QYT12429_Init(void);
void LCD_QYT12429_AllOn(void);
void LCD_QYT12429_Clear(void);
void LCD_QYT12429_DisplayTemperature(int32_t temperature_x100);
void LCD_QYT12429_DisplayHumidity(int32_t humidity_x100);
void LCD_QYT12429_SetIcon(LCD_QYT12429_Icon_t icon, uint8_t enabled);
void LCD_QYT12429_SetBatteryIcon(uint8_t level);
void LCD_QYT12429_SetSignalIcon(uint8_t level);
void LCD_Show888Test(void);
void LCD_ShowFixedTest(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_QYT12429_H */

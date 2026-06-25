/**
  ******************************************************************************
  * @file    lcd_qyt12429.c
  * @brief   QYT12429 segment LCD display helpers.
  ******************************************************************************
  */

#include "lcd_qyt12429.h"
#include "ht1621.h"

#define LCD_TOTAL_DIGIT_COUNT    7U
#define LCD_TEMP_DIGIT_COUNT     4U
#define LCD_HUMI_DIGIT_COUNT     3U
#define LCD_UNUSED_POINT         0xFFU
#define LCD_NO_DECIMAL_POINT     0xFFU

typedef struct
{
  uint8_t seg;
  uint8_t com;
} LCD_Point_t;

typedef enum
{
  LCD_SEG_A = 0,
  LCD_SEG_B,
  LCD_SEG_C,
  LCD_SEG_D,
  LCD_SEG_E,
  LCD_SEG_F,
  LCD_SEG_G,
  LCD_SEG_DP,
  LCD_SEG_COUNT_PER_DIGIT
} LCD_DigitSegment_t;

/* QYT12429 segment map.
   LCD Pin1-4 -> COM0-COM3, LCD Pin5-20 -> SEG0-SEG15.
   HT1621 RAM bit0-bit3 -> COM0-COM3.
   Positions 0-3 are upper digits 1-4, positions 4-6 are lower digits 5-7. */
static const LCD_Point_t LCD_DigitMap[LCD_TOTAL_DIGIT_COUNT][LCD_SEG_COUNT_PER_DIGIT] =
{
  {
    {1U, 3U}, {1U, 2U}, {1U, 1U}, {0U, 0U},
    {0U, 1U}, {0U, 3U}, {0U, 2U}, {1U, 0U}
  },
  {
    {3U, 3U}, {3U, 2U}, {3U, 1U}, {2U, 0U},
    {2U, 1U}, {2U, 3U}, {2U, 2U}, {3U, 0U}
  },
  {
    {5U, 3U}, {5U, 2U}, {5U, 1U}, {4U, 0U},
    {4U, 1U}, {4U, 3U}, {4U, 2U}, {5U, 0U}
  },
  {
    {7U, 3U}, {7U, 2U}, {7U, 1U}, {6U, 0U},
    {6U, 1U}, {6U, 3U}, {6U, 2U}, {LCD_UNUSED_POINT, LCD_UNUSED_POINT}
  },
  {
    {9U, 3U}, {9U, 2U}, {9U, 1U}, {8U, 0U},
    {8U, 1U}, {8U, 3U}, {8U, 2U}, {9U, 0U}
  },
  {
    {11U, 3U}, {11U, 2U}, {11U, 1U}, {10U, 0U},
    {10U, 1U}, {10U, 3U}, {10U, 2U}, {11U, 0U}
  },
  {
    {13U, 3U}, {13U, 2U}, {13U, 1U}, {12U, 0U},
    {12U, 1U}, {12U, 3U}, {12U, 2U}, {LCD_UNUSED_POINT, LCD_UNUSED_POINT}
  }
};

static const LCD_Point_t LCD_CelsiusIcon = {7U, 0U};
static const LCD_Point_t LCD_PercentIcon = {13U, 0U};

static const LCD_Point_t LCD_IconMap[9] =
{
  {9U, 0U},  /* S1 */
  {15U, 3U}, /* S2 */
  {15U, 2U}, /* S3 */
  {15U, 1U}, /* S4 */
  {15U, 0U}, /* S5 */
  {14U, 0U}, /* S6 */
  {14U, 3U}, /* S7 */
  {14U, 2U}, /* S8 */
  {14U, 1U}  /* S9 */
};

static const uint8_t LCD_NumberPattern[10] =
{
  0x3FU, /* 0: A B C D E F */
  0x06U, /* 1: B C */
  0x5BU, /* 2: A B D E G */
  0x4FU, /* 3: A B C D G */
  0x66U, /* 4: B C F G */
  0x6DU, /* 5: A C D F G */
  0x7DU, /* 6: A C D E F G */
  0x07U, /* 7: A B C */
  0x7FU, /* 8: A B C D E F G */
  0x6FU  /* 9: A B C D F G */
};

static void LCD_SetPoint(const LCD_Point_t *point, uint8_t enabled);
static void LCD_SetDigitPattern(uint8_t position, uint8_t pattern, uint8_t decimal_point);
static void LCD_SetChar(uint8_t position, char value, uint8_t decimal_point);
static void LCD_DisplayChars(uint8_t start_position, uint8_t digit_count,
                             const char *chars, uint8_t decimal_position);
static void LCD_FormatSignedFixed1(int32_t value_x100, char chars[LCD_TEMP_DIGIT_COUNT],
                                   uint8_t *decimal_position);
static void LCD_FormatHumidity(int32_t humidity_x100, char chars[LCD_HUMI_DIGIT_COUNT],
                               uint8_t *decimal_position);

/**
  * @brief  Initialize the QYT12429 LCD helper.
  * @retval None
  */
void LCD_QYT12429_Init(void)
{
  LCD_QYT12429_Clear();
}

/**
  * @brief  Turn on all QYT12429 SEG0-SEG15/COM0-COM3 points.
  * @retval None
  */
void LCD_QYT12429_AllOn(void)
{
  HT1621_AllOn();
}

/**
  * @brief  Clear the LCD.
  * @retval None
  */
void LCD_QYT12429_Clear(void)
{
  HT1621_Clear();
}

/**
  * @brief  Display temperature as signed fixed-point value.
  * @param  temperature_x100 Temperature scaled by 100.
  * @retval None
  */
void LCD_QYT12429_DisplayTemperature(int32_t temperature_x100)
{
  char chars[LCD_TEMP_DIGIT_COUNT];
  uint8_t decimal_position;

  LCD_FormatSignedFixed1(temperature_x100, chars, &decimal_position);
  LCD_DisplayChars(0U, LCD_TEMP_DIGIT_COUNT, chars, decimal_position);
  LCD_SetPoint(&LCD_CelsiusIcon, 1U);
}

/**
  * @brief  Display humidity.
  * @param  humidity_x100 Humidity scaled by 100.
  * @retval None
  */
void LCD_QYT12429_DisplayHumidity(int32_t humidity_x100)
{
  char chars[LCD_HUMI_DIGIT_COUNT];
  uint8_t decimal_position;

  LCD_FormatHumidity(humidity_x100, chars, &decimal_position);
  LCD_DisplayChars(LCD_TEMP_DIGIT_COUNT, LCD_HUMI_DIGIT_COUNT, chars, decimal_position);
  LCD_SetPoint(&LCD_PercentIcon, 1U);
}

/**
  * @brief  Set a QYT12429 icon point S1-S9.
  * @param  icon Icon enum.
  * @param  enabled 1 to turn on, 0 to turn off.
  * @retval None
  */
void LCD_QYT12429_SetIcon(LCD_QYT12429_Icon_t icon, uint8_t enabled)
{
  if ((icon < LCD_QYT12429_ICON_S1) || (icon > LCD_QYT12429_ICON_S9))
  {
    return;
  }

  LCD_SetPoint(&LCD_IconMap[(uint8_t)icon - 1U], enabled);
}

/**
  * @brief  Set S6-S9 as four battery icon points.
  * @param  level 0-4 lit points.
  * @retval None
  */
void LCD_QYT12429_SetBatteryIcon(uint8_t level)
{
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S6, (level >= 1U) ? 1U : 0U);
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S7, (level >= 2U) ? 1U : 0U);
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S8, (level >= 3U) ? 1U : 0U);
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S9, (level >= 4U) ? 1U : 0U);
}

/**
  * @brief  Set S2-S5 as four signal icon points.
  * @param  level 0-4 lit points.
  * @retval None
  */
void LCD_QYT12429_SetSignalIcon(uint8_t level)
{
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S2, (level >= 1U) ? 1U : 0U);
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S3, (level >= 2U) ? 1U : 0U);
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S4, (level >= 3U) ? 1U : 0U);
  LCD_QYT12429_SetIcon(LCD_QYT12429_ICON_S5, (level >= 4U) ? 1U : 0U);
}

/**
  * @brief  Show upper 8888 degrees C and lower 888%.
  * @retval None
  */
void LCD_Show888Test(void)
{
  static const char temp_chars[LCD_TEMP_DIGIT_COUNT] = {'8', '8', '8', '8'};
  static const char humi_chars[LCD_HUMI_DIGIT_COUNT] = {'8', '8', '8'};

  LCD_QYT12429_Clear();
  LCD_DisplayChars(0U, LCD_TEMP_DIGIT_COUNT, temp_chars, LCD_NO_DECIMAL_POINT);
  LCD_DisplayChars(LCD_TEMP_DIGIT_COUNT, LCD_HUMI_DIGIT_COUNT, humi_chars, LCD_NO_DECIMAL_POINT);
  LCD_SetPoint(&LCD_CelsiusIcon, 1U);
  LCD_SetPoint(&LCD_PercentIcon, 1U);
}

/**
  * @brief  Show upper 27.2 degrees C and lower 73.1%.
  * @retval None
  */
void LCD_ShowFixedTest(void)
{
  LCD_QYT12429_Clear();
  LCD_QYT12429_DisplayTemperature(2720);
  LCD_QYT12429_DisplayHumidity(7310);
}

static void LCD_SetPoint(const LCD_Point_t *point, uint8_t enabled)
{
  if ((point->seg == LCD_UNUSED_POINT) || (point->com == LCD_UNUSED_POINT))
  {
    return;
  }

  HT1621_SetSegment(point->seg, point->com, enabled);
}

static void LCD_SetDigitPattern(uint8_t position, uint8_t pattern, uint8_t decimal_point)
{
  uint8_t segment;

  if (position >= LCD_TOTAL_DIGIT_COUNT)
  {
    return;
  }

  for (segment = LCD_SEG_A; segment <= LCD_SEG_G; segment++)
  {
    LCD_SetPoint(&LCD_DigitMap[position][segment], ((pattern & (uint8_t)(1U << segment)) != 0U) ? 1U : 0U);
  }

  LCD_SetPoint(&LCD_DigitMap[position][LCD_SEG_DP], decimal_point);
}

static void LCD_SetChar(uint8_t position, char value, uint8_t decimal_point)
{
  if ((value >= '0') && (value <= '9'))
  {
    LCD_SetDigitPattern(position, LCD_NumberPattern[(uint8_t)(value - '0')], decimal_point);
  }
  else if (value == '-')
  {
    LCD_SetDigitPattern(position, (uint8_t)(1U << LCD_SEG_G), decimal_point);
  }
  else
  {
    LCD_SetDigitPattern(position, 0x00U, decimal_point);
  }
}

static void LCD_DisplayChars(uint8_t start_position, uint8_t digit_count,
                             const char *chars, uint8_t decimal_position)
{
  uint8_t index;

  for (index = 0U; index < digit_count; index++)
  {
    LCD_SetChar((uint8_t)(start_position + index), chars[index],
                (index == decimal_position) ? 1U : 0U);
  }
}

static void LCD_FormatSignedFixed1(int32_t value_x100, char chars[LCD_TEMP_DIGIT_COUNT],
                                   uint8_t *decimal_position)
{
  int32_t value_x10;
  int32_t abs_x10;
  int32_t integer;
  int32_t fraction;
  uint8_t index;

  for (index = 0U; index < LCD_TEMP_DIGIT_COUNT; index++)
  {
    chars[index] = ' ';
  }
  *decimal_position = LCD_NO_DECIMAL_POINT;

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
    abs_x10 = -value_x10;
  }
  else
  {
    abs_x10 = value_x10;
  }

  if (value_x10 < 0)
  {
    if (abs_x10 < 100)
    {
      chars[0] = '-';
      chars[1] = (char)('0' + (abs_x10 / 10));
      chars[2] = (char)('0' + (abs_x10 % 10));
      *decimal_position = 1U;
    }
    else
    {
      integer = (abs_x10 + 5) / 10;
      if (integer > 99)
      {
        integer = 99;
      }
      chars[0] = '-';
      chars[1] = (char)('0' + ((integer / 10) % 10));
      chars[2] = (char)('0' + (integer % 10));
    }
  }
  else
  {
    integer = abs_x10 / 10;
    fraction = abs_x10 % 10;

    if (integer >= 100)
    {
      if (integer > 999)
      {
        integer = 999;
      }
      chars[0] = (char)('0' + ((integer / 100) % 10));
      chars[1] = (char)('0' + ((integer / 10) % 10));
      chars[2] = (char)('0' + (integer % 10));
    }
    else if (integer >= 10)
    {
      chars[0] = (char)('0' + (integer / 10));
      chars[1] = (char)('0' + (integer % 10));
      chars[2] = (char)('0' + fraction);
      *decimal_position = 1U;
    }
    else
    {
      chars[1] = (char)('0' + integer);
      chars[2] = (char)('0' + fraction);
      *decimal_position = 1U;
    }
  }
}

static void LCD_FormatHumidity(int32_t humidity_x100, char chars[LCD_HUMI_DIGIT_COUNT],
                               uint8_t *decimal_position)
{
  int32_t humidity_x10;
  int32_t integer;
  int32_t fraction;

  if (humidity_x100 < 0)
  {
    humidity_x100 = 0;
  }
  else if (humidity_x100 > 10000)
  {
    humidity_x100 = 10000;
  }

  if (humidity_x100 >= 9950)
  {
    chars[0] = '1';
    chars[1] = '0';
    chars[2] = '0';
    *decimal_position = LCD_NO_DECIMAL_POINT;
  }
  else
  {
    humidity_x10 = (humidity_x100 + 5) / 10;
    integer = humidity_x10 / 10;
    fraction = humidity_x10 % 10;

    chars[0] = (char)('0' + (integer / 10));
    chars[1] = (char)('0' + (integer % 10));
    chars[2] = (char)('0' + fraction);
    *decimal_position = 1U;
  }
}

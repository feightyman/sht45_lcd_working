/**
  ******************************************************************************
  * @file    ht1621.c
  * @brief   HT1621 three-wire LCD controller driver.
  ******************************************************************************
  */

#include "ht1621.h"

#define HT1621_CS_GPIO_PORT       GPIOA
#define HT1621_CS_GPIO_PIN        GPIO_PIN_0
#define HT1621_WR_GPIO_PORT       GPIOA
#define HT1621_WR_GPIO_PIN        GPIO_PIN_1
#define HT1621_DATA_GPIO_PORT     GPIOA
#define HT1621_DATA_GPIO_PIN      GPIO_PIN_4

#define HT1621_ID_CMD             0x04U
#define HT1621_ID_WRITE           0x05U

#define HT1621_CMD_SYS_DIS        0x00U
#define HT1621_CMD_SYS_EN         0x02U
#define HT1621_CMD_LCD_OFF        0x04U
#define HT1621_CMD_LCD_ON         0x06U
#define HT1621_CMD_RC_256K        0x30U
#define HT1621_CMD_BIAS_1_3_4COM  0x52U

extern UART_HandleTypeDef UartHandle;

static uint8_t HT1621_Ram[HT1621_RAM_ADDRESS_COUNT];

static void HT1621_GPIOInit(void);
static void HT1621_SendCommand(uint8_t command);
static void HT1621_WriteBitsMsb(uint16_t data, uint8_t bits);
static void HT1621_WriteBitsLsb(uint16_t data, uint8_t bits);
static void HT1621_Print(const char *text);
static void HT1621_PrintHexByte(uint8_t value);
static void HT1621_Delay(void);

/**
  * @brief  Initialize HT1621 GPIO and LCD controller.
  * @retval None
  */
void HT1621_Init(void)
{
  HT1621_GPIOInit();

  HT1621_SendCommand(HT1621_CMD_SYS_EN);
  HT1621_SendCommand(HT1621_CMD_RC_256K);
  HT1621_SendCommand(HT1621_CMD_BIAS_1_3_4COM);
  HT1621_SendCommand(HT1621_CMD_LCD_ON);

  HT1621_Clear();
}

/**
  * @brief  Clear all RAM bits used by QYT12429 SEG0-SEG15.
  * @retval None
  */
void HT1621_Clear(void)
{
  HT1621_AllOffRaw();
}

/**
  * @brief  Turn on all LCD bits used by QYT12429 SEG0-SEG15.
  * @retval None
  */
void HT1621_AllOn(void)
{
  HT1621_AllOnRaw();
}

/**
  * @brief  Raw all-on test for HT1621 RAM addresses 0-31.
  * @retval None
  */
void HT1621_AllOnRaw(void)
{
  uint8_t address;

  for (address = 0U; address < HT1621_RAM_ADDRESS_COUNT; address++)
  {
    HT1621_Write4Bit(address, 0x0FU);
  }
}

/**
  * @brief  Raw all-off test for HT1621 RAM addresses 0-31.
  * @retval None
  */
void HT1621_AllOffRaw(void)
{
  uint8_t address;

  for (address = 0U; address < HT1621_RAM_ADDRESS_COUNT; address++)
  {
    HT1621_Write4Bit(address, 0x00U);
  }
}

/**
  * @brief  Scan HT1621 RAM addresses one by one for raw LCD mapping test.
  * @retval None
  */
void HT1621_AddressScanTest(void)
{
  uint8_t address;

  for (address = 0U; address < HT1621_RAM_ADDRESS_COUNT; address++)
  {
    HT1621_AllOffRaw();
    HT1621_Write4Bit(address, 0x0FU);

    HT1621_Print("HT1621 addr=0x");
    HT1621_PrintHexByte(address);
    HT1621_Print("\r\n");

    HAL_Delay(1000);
  }
}

/**
  * @brief  Write one 4-bit HT1621 RAM address.
  * @param  address HT1621 RAM address.
  * @param  data COM0-COM3 bitmap in lower 4 bits.
  * @retval None
  */
void HT1621_Write4Bit(uint8_t address, uint8_t data)
{
  if (address >= HT1621_RAM_ADDRESS_COUNT)
  {
    return;
  }

  HT1621_Ram[address] = data & 0x0FU;

  HAL_GPIO_WritePin(HT1621_CS_GPIO_PORT, HT1621_CS_GPIO_PIN, GPIO_PIN_RESET);
  HT1621_WriteBitsMsb(HT1621_ID_WRITE, 3U);
  HT1621_WriteBitsMsb((uint16_t)(address & 0x3FU), 6U);
  HT1621_WriteBitsLsb((uint16_t)HT1621_Ram[address], 4U);
  HAL_GPIO_WritePin(HT1621_CS_GPIO_PORT, HT1621_CS_GPIO_PIN, GPIO_PIN_SET);
}

void HT1621_WriteNibble(uint8_t address, uint8_t data)
{
  HT1621_Write4Bit(address, data);
}

/**
  * @brief  Set or clear one SEG/COM LCD point.
  * @param  seg SEG index, 0-15.
  * @param  com COM index, 0-3.
  * @param  enabled 1 to turn on, 0 to turn off.
  * @retval None
  */
void HT1621_SetSegment(uint8_t seg, uint8_t com, uint8_t enabled)
{
  uint8_t mask;

  if ((seg >= HT1621_LCD_SEG_COUNT) || (com >= HT1621_LCD_COM_COUNT))
  {
    return;
  }

  mask = (uint8_t)(1U << com);

  if (enabled != 0U)
  {
    HT1621_Ram[seg] |= mask;
  }
  else
  {
    HT1621_Ram[seg] &= (uint8_t)(~mask);
  }

  HT1621_WriteNibble(seg, HT1621_Ram[seg]);
}

static void HT1621_GPIOInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(HT1621_CS_GPIO_PORT, HT1621_CS_GPIO_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(HT1621_WR_GPIO_PORT, HT1621_WR_GPIO_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(HT1621_DATA_GPIO_PORT, HT1621_DATA_GPIO_PIN, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = HT1621_CS_GPIO_PIN | HT1621_WR_GPIO_PIN | HT1621_DATA_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void HT1621_SendCommand(uint8_t command)
{
  HAL_GPIO_WritePin(HT1621_CS_GPIO_PORT, HT1621_CS_GPIO_PIN, GPIO_PIN_RESET);
  /* Command mode: ID 100, then 9 command bits MSB first. */
  HT1621_WriteBitsMsb(HT1621_ID_CMD, 3U);
  HT1621_WriteBitsMsb((uint16_t)command, 9U);
  HAL_GPIO_WritePin(HT1621_CS_GPIO_PORT, HT1621_CS_GPIO_PIN, GPIO_PIN_SET);
}

static void HT1621_WriteBitsMsb(uint16_t data, uint8_t bits)
{
  uint16_t mask;

  while (bits > 0U)
  {
    mask = (uint16_t)(1U << (bits - 1U));

    HAL_GPIO_WritePin(HT1621_WR_GPIO_PORT, HT1621_WR_GPIO_PIN, GPIO_PIN_RESET);
    HT1621_Delay();

    HAL_GPIO_WritePin(HT1621_DATA_GPIO_PORT, HT1621_DATA_GPIO_PIN,
                      ((data & mask) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HT1621_Delay();

    HAL_GPIO_WritePin(HT1621_WR_GPIO_PORT, HT1621_WR_GPIO_PIN, GPIO_PIN_SET);
    HT1621_Delay();

    bits--;
  }
}

static void HT1621_WriteBitsLsb(uint16_t data, uint8_t bits)
{
  uint16_t mask = 1U;

  while (bits > 0U)
  {
    HAL_GPIO_WritePin(HT1621_WR_GPIO_PORT, HT1621_WR_GPIO_PIN, GPIO_PIN_RESET);
    HT1621_Delay();

    HAL_GPIO_WritePin(HT1621_DATA_GPIO_PORT, HT1621_DATA_GPIO_PIN,
                      ((data & mask) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HT1621_Delay();

    HAL_GPIO_WritePin(HT1621_WR_GPIO_PORT, HT1621_WR_GPIO_PIN, GPIO_PIN_SET);
    HT1621_Delay();

    mask <<= 1;
    bits--;
  }
}

static void HT1621_Print(const char *text)
{
  uint16_t length = 0U;

  while (text[length] != '\0')
  {
    length++;
  }

  if (length > 0U)
  {
    (void)HAL_UART_Transmit(&UartHandle, (uint8_t *)text, length, 1000);
  }
}

static void HT1621_PrintHexByte(uint8_t value)
{
  static const char hex[] = "0123456789ABCDEF";
  char text[3];

  text[0] = hex[(value >> 4) & 0x0FU];
  text[1] = hex[value & 0x0FU];
  text[2] = '\0';

  HT1621_Print(text);
}

static void HT1621_Delay(void)
{
  volatile uint8_t delay;

  for (delay = 0U; delay < 6U; delay++)
  {
    __NOP();
  }
}

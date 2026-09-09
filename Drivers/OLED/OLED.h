#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

/*
 * OLED SSD1306 - SPI2 (shared with W25Q128)
 *   PB13 = SPI2_CLK   (SCK)
 *   PB15 = SPI2_MOSI  (MOSI data)
 *   PG4  = OLED_CS    (CS)
 *   PB5  = OLED_DC    (DC data/cmd select)
 *   PG11 = OLED_RES   (RES reset)
 *
 * SPI Mode 3 (CPOL=1, CPHA=1) -- must reconfig if switching from W25Q
 */

#define OLED_DC_LOW()    GPIO_ResetBits(GPIOB, GPIO_Pin_5)
#define OLED_DC_HIGH()   GPIO_SetBits(GPIOB, GPIO_Pin_5)

#define OLED_CS_LOW()    GPIO_ResetBits(GPIOG, GPIO_Pin_4)
#define OLED_CS_HIGH()   GPIO_SetBits(GPIOG, GPIO_Pin_4)

#define OLED_RST_LOW()   GPIO_ResetBits(GPIOG, GPIO_Pin_11)
#define OLED_RST_HIGH()  GPIO_SetBits(GPIOG, GPIO_Pin_11)

extern uint8_t oled_buf[1024];

void OLED_Init(void);
void OLED_Clear(void);
void OLED_DrawPoint(uint16_t x, uint16_t y, uint8_t t);
void OLED_Refresh(void);
void OLED_ShowChar(uint16_t x, uint16_t y, uint8_t ch);
void OLED_ShowStr(uint16_t x, uint16_t y, const char *str);
void OLED_ShowCenter(uint16_t y, const char *msg);

/* Switch SPI2 to OLED mode (Mode 3) */
void OLED_SPI2_Mode(void);
void OLED_DisplayOn(void);
void OLED_DisplayOff(void);
#endif

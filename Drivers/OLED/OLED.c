#include "OLED.h"
#include <string.h>
#include "OLED_font.h"

uint8_t oled_buf[1024];

/* ============================================================
 * SPI2 Mode switching (OLED shares SPI2 with W25Q128)
 *   OLED: Mode 3 (CPOL=1, CPHA=2Edge)
 *   W25Q: Mode 0 (CPOL=0, CPHA=1Edge)
 * ============================================================ */

static uint8_t spi2_mode_oled = 0;  /* 1 = OLED mode, 0 = W25Q mode */

void OLED_SPI2_Mode(void)
{
    SPI_InitTypeDef SPI_InitStruct;
    if(spi2_mode_oled) return;  /* already in OLED mode */

    SPI_Cmd(SPI2, DISABLE);
    SPI_InitStruct.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStruct.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStruct.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStruct.SPI_CPOL              = SPI_CPOL_High;
    SPI_InitStruct.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStruct.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStruct.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStruct.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &SPI_InitStruct);
    SPI_Cmd(SPI2, ENABLE);
    spi2_mode_oled = 1;
}

static uint8_t SPI2_ReadWrite(uint8_t data)
{
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, data);
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI2);
}

static void OLED_Write_Cmd(uint8_t cmd)
{
    OLED_SPI2_Mode();
    OLED_DC_LOW();
    OLED_CS_LOW();
    SPI2_ReadWrite(cmd);
    OLED_CS_HIGH();
}

void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    SPI_InitTypeDef  SPI_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOG, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* PB5 DC -- push-pull output */
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PG4 CS, PG11 RST -- push-pull output */
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* SPI2: PB13 SCK, PB15 MOSI -- AF push-pull */
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    OLED_CS_HIGH();

    /* Init SPI2 in OLED mode */
    SPI_Cmd(SPI2, DISABLE);
    SPI_InitStruct.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStruct.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStruct.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStruct.SPI_CPOL              = SPI_CPOL_High;
    SPI_InitStruct.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStruct.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStruct.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStruct.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &SPI_InitStruct);
    SPI_Cmd(SPI2, ENABLE);
    spi2_mode_oled = 1;

    /* Reset */
    OLED_RST_LOW();
    { volatile uint32_t i; for(i = 0; i < 100000; i++); }
    OLED_RST_HIGH();
    { volatile uint32_t i; for(i = 0; i < 100000; i++); }

    /* SSD1306 init commands */
    OLED_Write_Cmd(0xAE);
    OLED_Write_Cmd(0xD5); OLED_Write_Cmd(0x80);
    OLED_Write_Cmd(0xA8); OLED_Write_Cmd(0x3F);
    OLED_Write_Cmd(0xD3); OLED_Write_Cmd(0x00);
    OLED_Write_Cmd(0x40);
    OLED_Write_Cmd(0x8D); OLED_Write_Cmd(0x14);
    OLED_Write_Cmd(0x20); OLED_Write_Cmd(0x02);
    OLED_Write_Cmd(0xA1);
    OLED_Write_Cmd(0xC8);
    OLED_Write_Cmd(0xDA); OLED_Write_Cmd(0x12);
    OLED_Write_Cmd(0x81); OLED_Write_Cmd(0xCF);
    OLED_Write_Cmd(0xD9); OLED_Write_Cmd(0xF1);
    OLED_Write_Cmd(0xDB); OLED_Write_Cmd(0x30);
    OLED_Write_Cmd(0xA4);
    OLED_Write_Cmd(0xA6);
    OLED_Write_Cmd(0xAF);

    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void)
{
    memset(oled_buf, 0x00, sizeof(oled_buf));
}

void OLED_DrawPoint(uint16_t x, uint16_t y, uint8_t t)
{
    uint16_t pos;
    uint8_t  bit;
    if(x > 127 || y > 63) return;
    pos = (y / 8) * 128 + x;
    bit = y % 8;
    if(t)
        oled_buf[pos] |=  (1u << bit);
    else
        oled_buf[pos] &= ~(1u << bit);
}

void OLED_Refresh(void)
{
    uint8_t page;
    OLED_SPI2_Mode();
    for(page = 0; page < 8; page++)
    {
        OLED_Write_Cmd(0xB0 + page);
        OLED_Write_Cmd(0x00);
        OLED_Write_Cmd(0x10);

        OLED_DC_HIGH();
        OLED_CS_LOW();
        for(uint16_t i = 0; i < 128; i++)
        {
            SPI2_ReadWrite(oled_buf[page * 128 + i]);
        }
        OLED_CS_HIGH();
    }
}

//Ϣ�����ر���ʾ�������ı��Դ����ݣ��������滹��
void OLED_DisplayOff(void){
	OLED_Write_Cmd(0xAE);/* SSD1306: display off */
}

//���� �� �ָ���ʾ
void OLED_DisplayOn(void){
    OLED_Write_Cmd(0xAF);  /* SSD1306: display on */
}

/* Show one 8x16 ASCII character at (x, y) */
void OLED_ShowChar(uint16_t x, uint16_t y, uint8_t ch)
{
    const uint8_t *p = OLED_F8x16[ch - 0x20];
    uint8_t col, r, b;
    for(col = 0; col < 8; col++)
    {
        b = p[col];
        for(r = 0; r < 8; r++)
            OLED_DrawPoint(x+col, y+r, (b >> r) & 1);
    }
    for(col = 0; col < 8; col++)
    {
        b = p[8 + col];
        for(r = 0; r < 8; r++)
            OLED_DrawPoint(x+col, y+8+r, (b >> r) & 1);
    }
}

/* Show a null-terminated string at (x, y) */
void OLED_ShowStr(uint16_t x, uint16_t y, const char *str)
{
    while(*str) { OLED_ShowChar(x, y, (uint8_t)*str++); x += 8; }
}

/* Show a string centered horizontally at row y */
void OLED_ShowCenter(uint16_t y, const char *msg)
{
    uint8_t len = 0;
    const char *p = msg;
    while(*p++) len++;
    uint16_t x = (128 - (uint16_t)len * 8) / 2;
    OLED_ShowStr(x, y, msg);
}

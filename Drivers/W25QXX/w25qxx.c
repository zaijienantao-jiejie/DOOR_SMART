#include "w25qxx.h"
#include "delay.h"

/*
 * W25Q128 driver -- SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI), CS=PB12
 *
 * SPI2 is SHARED with OLED!
 *   W25Q128: Mode 0 (CPOL=0, CPHA=0)
 *   OLED:    Mode 3 (CPOL=1, CPHA=1)
 *
 * We switch SPI to Mode 0 before each W25Q operation.
 * OLED switches back to Mode 3 before its operations.
 */

extern void OLED_SPI2_Mode(void);  /* defined in OLED.c */

static void SPI2_Init_Mode0(void)
{
    SPI_InitTypeDef SPI_InitStructure;

    SPI_Cmd(SPI2, DISABLE);
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
}

void W25Q_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* PB12 (CS) -- push-pull output */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_12);  /* CS high = idle */

    /* PB13 (SCK) PB15 (MOSI) -- AF push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB14 (MISO) -- floating input */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Don't init SPI2 here - OLED does it first.
     * We reconfigure SPI mode before each W25Q operation. */
}

static u8 SPI2_ReadWriteByte(u8 tx)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, tx);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI2);
}

u32 W25Q_ReadID(void)
{
    u32 id;
    SPI2_Init_Mode0();
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(W25X_JedecDeviceID);
    id  = (u32)SPI2_ReadWriteByte(0xFF) << 16;
    id |= (u32)SPI2_ReadWriteByte(0xFF) << 8;
    id |= (u32)SPI2_ReadWriteByte(0xFF);
    W25Q_CS_HIGH();
    OLED_SPI2_Mode();  /* restore OLED SPI mode */
    return id;
}

static void W25Q_WriteEnable(void)
{
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(W25X_WriteEnable);
    W25Q_CS_HIGH();
}

static void W25Q_WaitBusy(void)
{
    while (1) {
        W25Q_CS_LOW();
        SPI2_ReadWriteByte(W25X_ReadStatusReg);
        if ((SPI2_ReadWriteByte(0xFF) & 0x01) == 0)
            break;
        W25Q_CS_HIGH();
    }
    W25Q_CS_HIGH();
}

void W25Q_Read(u8 *buf, u32 addr, u16 len)
{
    u16 i;
    SPI2_Init_Mode0();
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(W25X_ReadData);
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    for (i = 0; i < len; i++)
        buf[i] = SPI2_ReadWriteByte(0xFF);
    W25Q_CS_HIGH();
    OLED_SPI2_Mode();  /* restore OLED SPI mode */
}

static void W25Q_WritePage(u8 *buf, u32 addr, u16 len)
{
    u16 i;
    W25Q_WriteEnable();
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(W25X_PageProgram);
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    for (i = 0; i < len; i++)
        SPI2_ReadWriteByte(buf[i]);
    W25Q_CS_HIGH();
    W25Q_WaitBusy();
}

void W25Q_Write(u8 *buf, u32 addr, u16 len)
{
    u16 page_remain;
    SPI2_Init_Mode0();
    page_remain = 256 - (addr % 256);
    if (len <= page_remain)
        page_remain = len;

    while (1) {
        W25Q_WritePage(buf, addr, page_remain);
        if (len == page_remain)
            break;
        buf  += page_remain;
        addr += page_remain;
        len  -= page_remain;
        page_remain = (len > 256) ? 256 : len;
    }
    OLED_SPI2_Mode();  /* restore OLED SPI mode */
}

void W25Q_EraseSector(u32 addr)
{
    SPI2_Init_Mode0();
    W25Q_WriteEnable();
    W25Q_WaitBusy();
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(W25X_SectorErase);
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    W25Q_CS_HIGH();
    W25Q_WaitBusy();
    OLED_SPI2_Mode();  /* restore OLED SPI mode */
}

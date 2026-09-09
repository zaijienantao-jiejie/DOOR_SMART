#ifndef __W25QXX_H
#define __W25QXX_H

#include "stm32f10x.h"

/* W25Q128: 16MB, 4KB sector, 64KB block, 256B page */
#define W25Q128_ID          0xEF4018
#define W25X_WriteEnable    0x06
#define W25X_WriteDisable   0x04
#define W25X_ReadStatusReg  0x05
#define W25X_WriteStatusReg 0x01
#define W25X_ReadData       0x03
#define W25X_FastReadData   0x0B
#define W25X_PageProgram    0x02
#define W25X_SectorErase    0x20
#define W25X_BlockErase32   0x52
#define W25X_BlockErase64   0xD8
#define W25X_ChipErase      0xC7
#define W25X_PowerDown      0xB9
#define W25X_ReleasePowerDown 0xAB
#define W25X_DeviceID       0xAB
#define W25X_ManufactDeviceID 0x90
#define W25X_JedecDeviceID  0x9F

/* CS pin: PB12 */
#define W25Q_CS_LOW()   GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define W25Q_CS_HIGH()  GPIO_SetBits(GPIOB, GPIO_Pin_12)

void W25Q_Init(void);
u32  W25Q_ReadID(void);
void W25Q_Read(u8 *buf, u32 addr, u16 len);
void W25Q_Write(u8 *buf, u32 addr, u16 len);
void W25Q_EraseSector(u32 addr);

#endif

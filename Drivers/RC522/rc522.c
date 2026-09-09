#include "rc522.h"
#include "delay.h"


/*
 * RC522 驱动 — 硬件 SPI1 版本
 * 引脚: PA4(CS) PA5(SCK) PA6(MISO) PA7(MOSI) PC4(RST)
 */

static void SPI1_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    SPI_InitTypeDef   SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC |
                           RCC_APB2Periph_AFIO  | RCC_APB2Periph_SPI1, ENABLE);

    /* PA5(SCK) PA7(MOSI) — 复用推挽 由外设控制引脚电平 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA6(MISO) — 浮空输入 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA4(CS) — 推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PC4(RST) — 推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = MF522_RST_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MF522_RST_PORT, &GPIO_InitStructure);

    /* SPI1 配置: 全双工主机, 8bit, MSB, CPOL=0, CPHA=0, 分频16(4.5MHz) */
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);

    NSS_H;
    RST_H;
}

static uint8_t SPI1_ReadWriteByte(uint8_t tx)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, tx);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

/* —— RC522 端口初始化 —— */
void PcdInit(void)
{
    SPI1_Init();
}

/* —— 读 RC522 寄存器 —— */
unsigned char ReadRawRC(unsigned char Address)
{
    unsigned char ucAddr;
    unsigned char ucResult;

    NSS_L;//拉低片选信号线
    ucAddr = ((Address << 1) & 0x7E) | 0x80;//发送读指令，最高为为1代表读指令
    SPI1_ReadWriteByte(ucAddr);
    ucResult = SPI1_ReadWriteByte(0x00);//发送0x00哑字节，接收寄存器返回值
    NSS_H;//拉高片选信号线
    return ucResult;
}

/* —— 写 RC522 寄存器 —— */
void WriteRawRC(unsigned char Address, unsigned char value)
{
    unsigned char ucAddr;

    NSS_L;
    ucAddr = (Address << 1) & 0x7E;//将寄存器地址的最高位置为0，代表写
    SPI1_ReadWriteByte(ucAddr);//发送写指令，最高为置0代表写
    SPI1_ReadWriteByte(value);
    NSS_H;
}

/* —— 置位 —— */
//作用寄存器置1（把位掩码的几位置1，再写回去，其他位不变）
void SetBitMask(unsigned char reg, unsigned char mask)
{
    unsigned char tmp;
    tmp = ReadRawRC(reg);//读取原寄存器的值
    WriteRawRC(reg, tmp | mask);//将原寄存器的几个位置为1并且写回去
}

/* —— 清位 —— */
void ClearBitMask(unsigned char reg, unsigned char mask)
{
    unsigned char tmp;
    tmp = ReadRawRC(reg);
    WriteRawRC(reg, tmp & ~mask);
}

/* —— CRC 计算 —— */
/* @param pIndata 待计算CRC的数据缓冲区指针
 * @param len 待计算CRC的数据字节长度
 * @param pOutData [out]输出2字节CRC，pOutData[0]低字节，pOutData[1]高字节*/
//调用RC522硬件crc16计算iso1443a crc校验码
void CalulateCRC(unsigned char *pIndata, unsigned char len, unsigned char *pOutData)
{
    unsigned char i, n;

    ClearBitMask(DivIrqReg, 0x04);//将第三位清零
    WriteRawRC(CommandReg, PCD_IDLE);
    SetBitMask(FIFOLevelReg, 0x80);
    for (i = 0; i < len; i++)
        WriteRawRC(FIFODataReg, *(pIndata + i));
    WriteRawRC(CommandReg, PCD_CALCCRC);
    i = 0xFF;
    do {
        n = ReadRawRC(DivIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x04));
    pOutData[0] = ReadRawRC(CRCResultRegL);
    pOutData[1] = ReadRawRC(CRCResultRegM);
}

/* —— 复位 RC522 —— */
char PcdReset(void)
{
    RST_H;
    delay_ms(10);
    RST_L;
    delay_ms(10);
    RST_H;
    delay_ms(100);

    WriteRawRC(CommandReg, PCD_RESETPHASE);
    WriteRawRC(ModeReg, 0x3D);
    WriteRawRC(TReloadRegL, 30);
    WriteRawRC(TReloadRegH, 0);
    WriteRawRC(TModeReg, 0x8D);
    WriteRawRC(TPrescalerReg, 0x3E);
    WriteRawRC(TxAutoReg, 0x40);
    return MI_OK;
}

/* —— 配置 ISO14443-A —— */
char M500PcdConfigISOType(unsigned char type)
{
    if (type != 'A')
        return (char)-1;

    ClearBitMask(Status2Reg, 0x08);
    WriteRawRC(ModeReg, 0x3D);
    WriteRawRC(RxSelReg, 0x86);
    WriteRawRC(RFCfgReg, 0x7F);
    WriteRawRC(TReloadRegL, 30);
    WriteRawRC(TReloadRegH, 0);
    WriteRawRC(TModeReg, 0x8D);
    WriteRawRC(TPrescalerReg, 0x3E);
    delay_ms(10);
    PcdAntennaOn();
    return MI_OK;
}

/* —— 通过 RC522 与卡通信 —— */
//command -- pcd_authent(验证)或者pcd_transceive(收发)
//pIndata -- 要发送给卡的数据
//InLenByte	发送数据的字节数
//pOutData	接收卡回复的缓冲区
//pOutLenBit	收到的回复数据位数（注意是 bit 不是 byte）
char PcdComMF522(unsigned char Command, unsigned char *pInData,
                  unsigned char InLenByte, unsigned char *pOutData,
                  unsigned int  *pOutLenBit)
{
    char status = MI_ERR;
    unsigned char irqEn   = 0x00;
    unsigned char waitFor = 0x0;
    unsigned char lastBits;
    unsigned char n;
    unsigned int i;

    switch (Command) {
    case PCD_AUTHENT:		//认证命令
        irqEn   = 0x12;		//开启空闲中断+定时器中断
        waitFor = 0x10;		//等待空闲标志
        break;
    case PCD_TRANSCEIVE:	//收发命令
        irqEn   = 0x77;		//开启所有通信中断
        waitFor = 0x30;		//等待接收完成+空闲标志
        break;
    default:
        break;
    }

    WriteRawRC(ComIEnReg, irqEn | 0x80);// 开启对应中断，且 bit7=1 允许中断输出
    ClearBitMask(ComIrqReg, 0x80);		// 清除中断标志位
    WriteRawRC(CommandReg, PCD_IDLE);   // 先设为 IDLE，停止当前操作
    SetBitMask(FIFOLevelReg, 0x80);		// bit7=1 → 清空 FIFO 缓冲区

    for (i = 0; i < InLenByte; i++)
        WriteRawRC(FIFODataReg, pInData[i]); // 把数据逐字节写入 FIFO
    WriteRawRC(CommandReg, Command);

    if (Command == PCD_TRANSCEIVE)
        SetBitMask(BitFramingReg, 0x80);	// bit7=1 → STARTSEND，启动发送

    i = 2000;
    do {
        n = ReadRawRC(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitFor));
    ClearBitMask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(ReadRawRC(ErrorReg) & 0x1B)) {
            status = MI_OK;
            if (n & irqEn & 0x01)
                status = MI_NOTAGERR;
            if (Command == PCD_TRANSCEIVE) {
                n = ReadRawRC(FIFOLevelReg);
                lastBits = ReadRawRC(ControlReg) & 0x07;
                if (lastBits)
                    *pOutLenBit = (n - 1) * 8 + lastBits;
                else
                    *pOutLenBit = n * 8;
                if (n == 0) n = 1;
                if (n > MAXRLEN) n = MAXRLEN;
                for (i = 0; i < n; i++)
                    pOutData[i] = ReadRawRC(FIFODataReg);
            }
        } else {
            status = MI_ERR;
        }
    }

    SetBitMask(ControlReg, 0x80);
    WriteRawRC(CommandReg, PCD_IDLE);
    return status;
}

/* —— 开启天线 —— */
void PcdAntennaOn(void)
{
    unsigned char i;
    i = ReadRawRC(TxControlReg);
    if (!(i & 0x03))
        SetBitMask(TxControlReg, 0x03);
}

/* —— 关闭天线 —— */
void PcdAntennaOff(void)
{
    ClearBitMask(TxControlReg, 0x03);
}

/* —— 寻卡 —— */
char PcdRequest(unsigned char req_code, unsigned char *pTagType)
{
    char status;
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];
	
	//ISO14443A 规定寻卡命令 0x26 是一个 7 bit 的短帧（不含校验位）。
	//如果发 8 bit，模块会认为最后一个 bit 是校验位，导致通信失败。
    ClearBitMask(Status2Reg, 0x08);// 清除 MFAuthent 标志（之前可能认证过卡）
    WriteRawRC(BitFramingReg, 0x07);//发送7位，BitFramingReg的低三位表示最后一个字节发送多少位
    SetBitMask(TxControlReg, 0x03);// 确保天线开启（TxControlReg 的 bit0+bit1）

    ucComMF522Buf[0] = req_code;
    status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf, &unLen);

    if ((status == MI_OK) && (unLen == 0x10)) {
        *pTagType     = ucComMF522Buf[0];
        *(pTagType+1) = ucComMF522Buf[1];
    } else {
        status = MI_ERR;
    }
    return status;
}

/* —— 防冲撞, 获取 4 字节卡号 —— */
char PcdAnticoll(unsigned char *pSnr)
{
    char status;
    unsigned char i, snr_check = 0;
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];

    ClearBitMask(Status2Reg, 0x08);
    WriteRawRC(BitFramingReg, 0x00);
    ClearBitMask(CollReg, 0x80);

    ucComMF522Buf[0] = PICC_ANTICOLL1;
    ucComMF522Buf[1] = 0x20;
    status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, &unLen);

    if (status == MI_OK) {
        for (i = 0; i < 4; i++) {
            *(pSnr + i) = ucComMF522Buf[i];
            snr_check ^= ucComMF522Buf[i];
        }
        if (snr_check != ucComMF522Buf[i])
            status = MI_ERR;
    }
    SetBitMask(CollReg, 0x80);
    return status;
}

/* —— 选定卡片 —— */
char PcdSelect(unsigned char *pSnr)
{
    char status;
    unsigned char i;
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_ANTICOLL1;
    ucComMF522Buf[1] = 0x70;
    ucComMF522Buf[6] = 0;
    for (i = 0; i < 4; i++) {
        ucComMF522Buf[i+2] = *(pSnr + i);
        ucComMF522Buf[6]  ^= *(pSnr + i);
    }
    CalulateCRC(ucComMF522Buf, 7, &ucComMF522Buf[7]);
    ClearBitMask(Status2Reg, 0x08);
    status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, &unLen);

    if ((status == MI_OK) && (unLen == 0x18))
        status = MI_OK;
    else
        status = MI_ERR;
    return status;
}

/* —— 验证密钥 —— */
char PcdAuthState(unsigned char auth_mode, unsigned char addr,
                   unsigned char *pKey, unsigned char *pSnr)
{
    char status;
    unsigned int  unLen;
    unsigned char i, ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = auth_mode;
    ucComMF522Buf[1] = addr;
    for (i = 0; i < 6; i++)
        ucComMF522Buf[i+2] = *(pKey + i);
    for (i = 0; i < 4; i++)
        ucComMF522Buf[i+8] = *(pSnr + i);

    status = PcdComMF522(PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, &unLen);
    if ((status != MI_OK) || (!(ReadRawRC(Status2Reg) & 0x08)))
        status = MI_ERR;
    return status;
}

/* —— 读块 (16 字节) —— */
char PcdRead(unsigned char addr, unsigned char *pData)
{
    char status;
    unsigned int  unLen;
    unsigned char i, ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_READ;
    ucComMF522Buf[1] = addr;
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);
    status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);

    if ((status == MI_OK) && (unLen == 0x90)) {
        for (i = 0; i < 16; i++)
            *(pData + i) = ucComMF522Buf[i];
    } else {
        status = MI_ERR;
    }
    return status;
}

/* —— 写块 (16 字节) —— */
char PcdWrite(unsigned char addr, unsigned char *pData)
{
    char status;
    unsigned int  unLen;
    unsigned char i, ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_WRITE;
    ucComMF522Buf[1] = addr;
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);
    status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);

    if ((status != MI_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
        status = MI_ERR;

    if (status == MI_OK) {
        for (i = 0; i < 16; i++)
            ucComMF522Buf[i] = *(pData + i);
        CalulateCRC(ucComMF522Buf, 16, &ucComMF522Buf[16]);
        status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, &unLen);
        if ((status != MI_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
            status = MI_ERR;
    }
    return status;
}

/* —— 休眠卡片 —— */
char PcdHalt(void)
{
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_HALT;
    ucComMF522Buf[1] = 0;
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);
    PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);
    return MI_OK;
}

/* —— 等待卡离开 —— */
void WaitCardOff(void)
{
    char status;
    unsigned char TagType[2];

    while (1) {
        status = PcdRequest(PICC_REQALL, TagType);
        if (status) {
            status = PcdRequest(PICC_REQALL, TagType);
            if (status) {
                status = PcdRequest(PICC_REQALL, TagType);
                if (status)
                    return;
            }
        }
        delay_ms(100);
    }
}

/* —— RFID 模块统一初始化 —— */
void RFID_Init(void)
{
    PcdInit();
    PcdReset();
    PcdAntennaOff();
    PcdAntennaOn();
    M500PcdConfigISOType('A');
}

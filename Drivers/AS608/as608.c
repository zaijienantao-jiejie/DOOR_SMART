#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "delay.h"
#include "as608.h"
#include "as608_uart.h"

u32 AS608Addr = 0XFFFFFFFF;

/* PB7 input for WAK touch detection
 * AS608 touch output: HIGH when finger touched -> pull-down input */
void PS_StaGPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void MYUSART_SendData(u8 data)
{
    AS608_SendByte(data);
}

static void SendHead(void)  { MYUSART_SendData(0xEF); MYUSART_SendData(0x01); }
static void SendAddr(void)  { MYUSART_SendData(AS608Addr>>24); MYUSART_SendData(AS608Addr>>16); MYUSART_SendData(AS608Addr>>8); MYUSART_SendData(AS608Addr); }
static void SendFlag(u8 f)  { MYUSART_SendData(f); }
static void SendLength(int l) { MYUSART_SendData(l>>8); MYUSART_SendData(l); }
static void Sendcmd(u8 c)   { MYUSART_SendData(c); }
static void SendCheck(u16 c){ MYUSART_SendData(c>>8); MYUSART_SendData(c); }

static u8 *JudgeStr(u16 waittime)
{
    char *data;
    u8 str[8];
    str[0]=0xef; str[1]=0x01;
    str[2]=AS608Addr>>24; str[3]=AS608Addr>>16;
    str[4]=AS608Addr>>8;  str[5]=AS608Addr;
    str[6]=0x07; str[7]='\0';
    AS608_RX_STA = 0;
    while(--waittime) {
        delay_ms(1);
        if(AS608_RX_STA & 0x8000) {
            AS608_RX_STA = 0;
            data = strstr((const char*)AS608_RX_BUF, (const char*)str);
            if(data) return (u8*)data;
        }
    }
    return 0;
}

u8 PS_GetImage(void)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x01);
    temp = 0x01+0x03+0x01; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_GenChar(u8 BufferID)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x04); Sendcmd(0x02);
    MYUSART_SendData(BufferID);
    temp = 0x01+0x04+0x02+BufferID; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_Match(void)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x03);
    temp = 0x01+0x03+0x03; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_Search(u8 BufferID, u16 StartPage, u16 PageNum, SearchResult *p)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x08); Sendcmd(0x04);
    MYUSART_SendData(BufferID);
    MYUSART_SendData(StartPage>>8); MYUSART_SendData(StartPage);
    MYUSART_SendData(PageNum>>8); MYUSART_SendData(PageNum);
    temp = 0x01+0x08+0x04+BufferID+(StartPage>>8)+(u8)StartPage+(PageNum>>8)+(u8)PageNum;
    SendCheck(temp);
    data = JudgeStr(2000);
    if(data) { ensure=data[9]; p->pageID=(data[10]<<8)+data[11]; p->mathscore=(data[12]<<8)+data[13]; }
    else ensure = 0xff;
    return ensure;
}

u8 PS_RegModel(void)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x05);
    temp = 0x01+0x03+0x05; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_StoreChar(u8 BufferID, u16 PageID)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x06); Sendcmd(0x06);
    MYUSART_SendData(BufferID);
    MYUSART_SendData(PageID>>8); MYUSART_SendData(PageID);
    temp = 0x01+0x06+0x06+BufferID+(PageID>>8)+(u8)PageID;
    SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_DeletChar(u16 PageID, u16 N)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x07); Sendcmd(0x0C);
    MYUSART_SendData(PageID>>8); MYUSART_SendData(PageID);
    MYUSART_SendData(N>>8); MYUSART_SendData(N);
    temp = 0x01+0x07+0x0C+(PageID>>8)+(u8)PageID+(N>>8)+(u8)N;
    SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_Empty(void)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x0D);
    temp = 0x01+0x03+0x0D; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_WriteReg(u8 RegNum, u8 DATA)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x05); Sendcmd(0x0E);
    MYUSART_SendData(RegNum); MYUSART_SendData(DATA);
    temp = RegNum+DATA+0x01+0x05+0x0E; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_ReadSysPara(SysPara *p)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x0F);
    temp = 0x01+0x03+0x0F; SendCheck(temp);
    data = JudgeStr(1000);
    if(data) {
        ensure = data[9];
        p->PS_max=(data[14]<<8)+data[15];
        p->PS_level=data[17];
        p->PS_addr=(data[18]<<24)+(data[19]<<16)+(data[20]<<8)+data[21];
        p->PS_size=data[23];
        p->PS_N=data[25];
    } else ensure = 0xff;
    return ensure;
}

u8 PS_SetAddr(u32 PS_addr)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x07); Sendcmd(0x15);
    MYUSART_SendData(PS_addr>>24); MYUSART_SendData(PS_addr>>16);
    MYUSART_SendData(PS_addr>>8); MYUSART_SendData(PS_addr);
    temp = 0x01+0x07+0x15+(u8)(PS_addr>>24)+(u8)(PS_addr>>16)+(u8)(PS_addr>>8)+(u8)PS_addr;
    SendCheck(temp);
    AS608Addr = PS_addr;
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    AS608Addr = PS_addr;
    return ensure;
}

u8 PS_WriteNotepad(u8 NotePageNum, u8 *Byte32)
{
    u16 temp=0; u8 ensure, i; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(36); Sendcmd(0x18);
    MYUSART_SendData(NotePageNum);
    for(i=0;i<32;i++) { MYUSART_SendData(Byte32[i]); temp+=Byte32[i]; }
    temp = 0x01+36+0x18+NotePageNum+temp; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) ensure = data[9]; else ensure = 0xff;
    return ensure;
}

u8 PS_ReadNotepad(u8 NotePageNum, u8 *Byte32)
{
    u16 temp; u8 ensure, i; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x04); Sendcmd(0x19);
    MYUSART_SendData(NotePageNum);
    temp = 0x01+0x04+0x19+NotePageNum; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) { ensure=data[9]; for(i=0;i<32;i++) Byte32[i]=data[10+i]; }
    else ensure = 0xff;
    return ensure;
}

u8 PS_HighSpeedSearch(u8 BufferID, u16 StartPage, u16 PageNum, SearchResult *p)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x08); Sendcmd(0x1b);
    MYUSART_SendData(BufferID);
    MYUSART_SendData(StartPage>>8); MYUSART_SendData(StartPage);
    MYUSART_SendData(PageNum>>8); MYUSART_SendData(PageNum);
    temp = 0x01+0x08+0x1b+BufferID+(StartPage>>8)+(u8)StartPage+(PageNum>>8)+(u8)PageNum;
    SendCheck(temp);
    data = JudgeStr(2000);
    if(data) { ensure=data[9]; p->pageID=(data[10]<<8)+data[11]; p->mathscore=(data[12]<<8)+data[13]; }
    else ensure = 0xff;
    return ensure;
}

u8 PS_ValidTempleteNum(u16 *ValidN)
{
    u16 temp; u8 ensure; u8 *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x1d);
    temp = 0x01+0x03+0x1d; SendCheck(temp);
    data = JudgeStr(2000);
    if(data) { ensure=data[9]; *ValidN=(data[10]<<8)+data[11]; }
    else ensure = 0xff;
    return ensure;
}

u8 PS_HandShake(u32 *PS_Addr)
{
    u8 i;
    AS608_RX_STA = 0;  /* clear before sending, avoid stale flag */
    SendHead(); SendAddr();
    MYUSART_SendData(0x01); MYUSART_SendData(0x00); MYUSART_SendData(0x00);
    delay_ms(200);
    if(AS608_RX_STA & 0x8000) {
        if(AS608_RX_BUF[0]==0xEF && AS608_RX_BUF[1]==0x01 && AS608_RX_BUF[6]==0x07) {
            *PS_Addr = (AS608_RX_BUF[2]<<24)+(AS608_RX_BUF[3]<<16)+(AS608_RX_BUF[4]<<8)+AS608_RX_BUF[5];
            AS608_RX_STA = 0;
            return 0;
        }
        /* got data but not a valid reply -> dump for debug */
        printf("AS608 bad reply, len=%d: ", (int)(AS608_RX_STA & 0x3FFF));
        for (i = 0; i < 12 && i < (AS608_RX_STA & 0x3FFF); i++)
            printf("%02X ", AS608_RX_BUF[i]);
        printf("\r\n");
        AS608_RX_STA = 0;
    } else {
        printf("AS608 no reply (RX empty)\r\n");
    }
    return 1;
}

const char *EnsureMessage(u8 ensure)
{
    const char *p;
    switch(ensure) {
    case 0x00: p="       OK       "; break;
    case 0x01: p=" data receive err"; break;
    case 0x02: p=" no finger      "; break;
    case 0x03: p=" image fail     "; break;
    case 0x04: p=" too dry        "; break;
    case 0x05: p=" too wet        "; break;
    case 0x06: p=" too messy      "; break;
    case 0x07: p=" too few points "; break;
    case 0x08: p=" not match      "; break;
    case 0x09: p=" not found      "; break;
    case 0x0a: p=" merge fail     "; break;
    case 0x0b: p=" addr out range "; break;
    case 0x10: p=" del fail       "; break;
    case 0x11: p=" empty fail     "; break;
    case 0x15: p=" no valid img   "; break;
    case 0x18: p=" flash r/w err  "; break;
    case 0x19: p=" undefined err  "; break;
    case 0x1a: p=" invalid reg    "; break;
    case 0x1b: p=" reg data err   "; break;
    case 0x1c: p=" notepad err    "; break;
    case 0x1f: p=" lib full       "; break;
    case 0x20: p=" addr err       "; break;
    default:   p=" unknown err    "; break;
    }
    return p;
}

/*=========================================================================
 * User functions -- adapted for door access system
 *=========================================================================*/

static bool usedIDs[MAX_ID] = {false};

static void ShowErrMessage(uint8_t ensure)
{
    printf("%s\r\n", EnsureMessage(ensure));
}

uint8_t GetAvailableID(void)
{
    uint8_t id;
    for (id = 0; id < MAX_ID; id++) {
        if (!usedIDs[id]) {
            usedIDs[id] = true;
            return id;
        }
    }
    return 0xFF;
}

void ReleaseID(uint8_t id)
{
    if (id < MAX_ID)
        usedIDs[id] = false;
}

/* Register a new fingerprint (blocking, call from main loop) */
void Add_FR(void)
{
    uint8_t ensure, processnum = 0;
    uint8_t ID_NUM;

    ID_NUM = GetAvailableID();
    if (ID_NUM == 0xFF) {
        printf("mei you ke yong ID!\r\n");
        return;
    }

    while (1) {
        printf("qing an shou zhi\r\n");
        delay_ms(1000);
        switch (processnum) {
        case 0:
            ensure = PS_GetImage();
            if (ensure == 0x00) {
                ensure = PS_GenChar(CharBuffer1);
                if (ensure == 0x00) {
                    printf("zhi wen zheng chang\r\n");
                    processnum = 1;
                } else ShowErrMessage(ensure);
            } else ShowErrMessage(ensure);
            break;

        case 1:
            printf("qing zai an yi ci\r\n");
            ensure = PS_GetImage();
            if (ensure == 0x00) {
                ensure = PS_GenChar(CharBuffer2);
                if (ensure == 0x00) {
                    printf("zhi wen zheng chang\r\n");
                    processnum = 2;
                } else ShowErrMessage(ensure);
            } else ShowErrMessage(ensure);
            break;

        case 2:
            printf("dui bi liang ci zhi wen\r\n");
            ensure = PS_Match();
            if (ensure == 0x00) {
                printf("dui bi cheng gong\r\n");
                processnum = 3;
            } else {
                printf("dui bi shi bai\r\n");
                ShowErrMessage(ensure);
                ReleaseID(ID_NUM);
                processnum = 0;
            }
            delay_ms(500);
            break;

        case 3:
            printf("sheng cheng zhi wen mo ban\r\n");
            delay_ms(500);
            ensure = PS_RegModel();
            if (ensure == 0x00) {
                printf("sheng cheng cheng gong\r\n");
                processnum = 4;
            } else {
                printf("sheng cheng shi bai\r\n");
                ShowErrMessage(ensure);
                ReleaseID(ID_NUM);
                processnum = 0;
            }
            delay_ms(1000);
            break;

        case 4:
            printf("xuan ze ID: %d\r\n", ID_NUM);
            ensure = PS_StoreChar(CharBuffer1, ID_NUM);
            if (ensure == 0x00) {
                printf("lu ru cheng gong\r\n");
                delay_ms(1500);
                return;
            } else {
                printf("lu ru shi bai\r\n");
                ShowErrMessage(ensure);
                ReleaseID(ID_NUM);
            }
            break;
        }
        delay_ms(400);
    }
}

/* Verify fingerprint (single attempt, returns 1=success 0=failure) */
int press_FR(void)
{
    SearchResult seach;
    uint8_t ensure;

    ensure = PS_GetImage();
    if (ensure != 0x00) return 0;

    ensure = PS_GenChar(CharBuffer1);
    if (ensure != 0x00) return 0;

    ensure = PS_HighSpeedSearch(CharBuffer1, 0, 99, &seach);
    if (ensure == 0x00) {
        printf("zhi wen yan zheng cheng gong! ID:%d\r\n", seach.pageID);
        return 1;
    }
    printf("zhi wen yan zheng shi bai!\r\n");
    return 0;
}

/* Delete single fingerprint (default ID=1) */
void Del_FR(void)
{
    uint8_t  ensure;
    uint16_t ID_NUM = 1;
    printf("shan chu ID: %d\r\n", ID_NUM);
    ensure = PS_DeletChar(ID_NUM, 1);
    if (ensure == 0)
        printf("shan chu cheng gong\r\n");
    else
        ShowErrMessage(ensure);
    delay_ms(1500);
}

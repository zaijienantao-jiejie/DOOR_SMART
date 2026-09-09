#ifndef __AS608_H
#define __AS608_H
#include <stdio.h>
#include "stm32f10x.h"

/* PB7 for touch detection (WAK pin) */
#define PS_Sta   GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)
#define CharBuffer1 0x01
#define CharBuffer2 0x02
#define MAX_ID 100

extern u32 AS608Addr;

typedef struct {
    u16 pageID;
    u16 mathscore;
} SearchResult;

typedef struct {
    u16 PS_max;
    u8  PS_level;
    u32 PS_addr;
    u8  PS_size;
    u8  PS_N;
} SysPara;

void PS_StaGPIO_Init(void);

u8 PS_GetImage(void);
u8 PS_GenChar(u8 BufferID);
u8 PS_Match(void);
u8 PS_Search(u8 BufferID, u16 StartPage, u16 PageNum, SearchResult *p);
u8 PS_RegModel(void);
u8 PS_StoreChar(u8 BufferID, u16 PageID);
u8 PS_DeletChar(u16 PageID, u16 N);
u8 PS_Empty(void);
u8 PS_WriteReg(u8 RegNum, u8 DATA);
u8 PS_ReadSysPara(SysPara *p);
u8 PS_SetAddr(u32 addr);
u8 PS_WriteNotepad(u8 NotePageNum, u8 *content);
u8 PS_ReadNotepad(u8 NotePageNum, u8 *note);
u8 PS_HighSpeedSearch(u8 BufferID, u16 StartPage, u16 PageNum, SearchResult *p);
u8 PS_ValidTempleteNum(u16 *ValidN);
u8 PS_HandShake(u32 *PS_Addr);

uint8_t GetAvailableID(void);
void ReleaseID(uint8_t id);

const char *EnsureMessage(u8 ensure);

void Add_FR(void);
int  press_FR(void);
void Del_FR(void);

#endif

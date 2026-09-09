#ifndef __BEEP_H__
#define __BEEP_H__

#include "stm32f10x.h"

void beep_init(void);
void BEEP_OFF(void);
void BEEP_ON(void);

/* 语义反馈音 (供应用层调用) */
void Beep_Click(void);   /* 按键确认音 40ms */
void Beep_OK(void);      /* 成功提示音 300ms */
void Beep_Err(void);     /* 错误提示音 3 短声 */

#endif

#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <stdio.h>

#define USART_REC_LEN  200

extern u8  USART_RX_BUF[USART_REC_LEN];
extern u16 USART_RX_STA;

void usart_init(uint32_t bound);

#endif

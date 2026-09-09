#ifndef __AS608_UART_H
#define __AS608_UART_H
#include "stm32f10x.h"

#define AS608_MAX_RECV_LEN  400
#define AS608_MAX_SEND_LEN  400

extern u8  AS608_RX_BUF[AS608_MAX_RECV_LEN];
extern vu16 AS608_RX_STA;

void AS608_USART2_Init(u32 bound);
void AS608_TIM2_Init(void);
void AS608_SendByte(u8 data);

#endif

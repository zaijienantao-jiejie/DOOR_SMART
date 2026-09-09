#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x.h"

/*
 * TIM4 定时中断 — 每 100ms 触发一次刷卡检测
 * 用中断替代 while(1) 轮询, 主循环可以休眠或做其他事
 */

void Timer4_Init(u16 ms);

#endif

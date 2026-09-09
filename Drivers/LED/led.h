#ifndef __LED_H__
#define __LED_H__

#include "stm32f10x.h"

/* D1 = PG6，低电平点亮（LED 一端接 3.3V，另一端接 PG6） */
void led_init(void);
void LED1_ON(void);      /* 点亮 D1 */
void LED1_OFF(void);     /* 熄灭 D1 */
void LED1_Toggle(void);  /* 翻转 D1 */

#endif

#include "led.h"

/* D1 = PG6，低电平点亮 */
void led_init(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin   = GPIO_Pin_6;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOG, &gpio);
    LED1_OFF();   /* 默认熄灭 */
}

void LED1_ON(void)
{
    GPIO_ResetBits(GPIOG, GPIO_Pin_6);   /* 低电平点亮 */
}

void LED1_OFF(void)
{
    GPIO_SetBits(GPIOG, GPIO_Pin_6);     /* 高电平熄灭 */
}

void LED1_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(GPIOG, GPIO_Pin_6))
        GPIO_ResetBits(GPIOG, GPIO_Pin_6);
    else
        GPIO_SetBits(GPIOG, GPIO_Pin_6);
}

#include "beep.h"
#include "delay.h"

/* 蜂鸣器初始化 -- PG7 推挽输出, 默认低电平 */
void beep_init(void)
{
    GPIO_InitTypeDef gpio_instrcut;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
    gpio_instrcut.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_instrcut.GPIO_Pin   = GPIO_Pin_7;
    gpio_instrcut.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOG, &gpio_instrcut);
    GPIO_WriteBit(GPIOG, GPIO_Pin_7, Bit_RESET);
}

/* 打开蜂鸣器 */
void BEEP_ON(void)
{
    GPIO_WriteBit(GPIOG, GPIO_Pin_7, Bit_SET);
}

/* 关闭蜂鸣器 */
void BEEP_OFF(void)
{
    GPIO_WriteBit(GPIOG, GPIO_Pin_7, Bit_RESET);
}

/* 按键确认音 */
void Beep_Click(void)
{
    BEEP_ON(); delay_ms(40);  BEEP_OFF();
}

/* 成功提示音 */
void Beep_OK(void)
{
    BEEP_ON(); delay_ms(300); BEEP_OFF();
}

/* 错误提示音: 3 声短促蜂鸣 */
void Beep_Err(void)
{
    uint8_t i;
    for (i = 0; i < 3; i++) {
        BEEP_ON(); delay_ms(80); BEEP_OFF(); delay_ms(80);
    }
}

#include "timer.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* 信号量句柄在 freertos_test.c 里定义 */
extern SemaphoreHandle_t xRFID_Sem;

void Timer4_Init(uint16_t ms)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef        NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    TIM_TimeBaseStructure.TIM_Period        = ms * 10 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler     = 7199;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    /* 抢占优先级必须改成 3！
     * NVIC_PriorityGroup_2 下：
     *   抢占2 = 0x80 = 128 < 191 ✗ 不能调 FromISR
     *   抢占3 = 0xC0 = 192 > 191 ✓ 可以调 FromISR
     */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM4, ENABLE);
}

void TIM4_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

        /* 释放信号量，唤醒 RFID 任务 */
        if (xRFID_Sem != NULL) {
            xSemaphoreGiveFromISR(xRFID_Sem, &xHigherPriorityTaskWoken);
        }

        /* 有更高优先级任务被唤醒就切换 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

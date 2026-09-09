#include "as608_uart.h"

/*
 * AS608 UART layer -- USART2 dedicated (PA2 TX / PA3 RX)
 *
 * USART2 is independent from USART1 (printf), no shared-mode needed.
 *   PA2 (TX) -> AS608 RX
 *   PA3 (RX) -> AS608 TX
 *
 * TIM2: 10ms gap detection for AS608 packet end
 *   When USART2 receives a byte, TIM2 resets.
 *   If 10ms pass with no new byte, TIM2 IRQ sets AS608_RX_STA bit15.
 */

u8  AS608_RX_BUF[AS608_MAX_RECV_LEN];
vu16 AS608_RX_STA = 0;

/* USART2 init: PA2(TX), PA3(RX), 8N1, RXNE interrupt */
void AS608_USART2_Init(u32 bound)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA2 (TX) -- AF push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA3 (RX) -- floating input */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* USART2 NVIC: preemption 2, sub 1 */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_InitStructure.USART_BaudRate            = bound;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

/* TIM2: 10ms interrupt for AS608 RX gap detection */
/* APB1=36MHz, timer clock=72MHz, PSC=7199 -> 10kHz, ARR=99 -> 10ms */
void AS608_TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period        = 99;
    TIM_TimeBaseStructure.TIM_Prescaler     = 7199;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* TimeBaseInit sets UIF via UG event; clear it or TIM2 fires immediately */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, DISABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    AS608_RX_STA = 0;   /* clear RX state, avoid stale flag */
}

/* USART2 RX interrupt: store byte, reset TIM2 for gap detection */
void USART2_IRQHandler(void)
{
    u8 res;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        res = USART_ReceiveData(USART2);

        if ((AS608_RX_STA & 0x8000) == 0) {
            if (AS608_RX_STA < AS608_MAX_RECV_LEN) {
                TIM_SetCounter(TIM2, 0);
                if (AS608_RX_STA == 0)
                    TIM_Cmd(TIM2, ENABLE);
                AS608_RX_BUF[AS608_RX_STA++] = res;
            } else {
                AS608_RX_STA |= 1 << 15;
            }
        }
    }
}

/* TIM2 IRQ: 10ms with no new byte = AS608 packet complete */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if (AS608_RX_STA < 0x7FFF) {
            AS608_RX_STA |= 1 << 15;
        }
        TIM_Cmd(TIM2, DISABLE);
    }
}

/* Send one byte to AS608 via USART2 */
void AS608_SendByte(u8 data)
{
    while ((USART2->SR & 0x40) == 0);
    USART2->DR = data;
}

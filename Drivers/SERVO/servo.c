#include "servo.h"
#include "delay.h"

/* SG90 舵机驱动 (TIM5_CH2 / PA1)
 * 使用 TIM5 避免与 AS608 的 TIM2 间隔检测冲突
 * 系统时钟 72MHz, APB1=36MHz, 定时器时钟=72MHz (APB1*2)
 * PSC=71 -> 计数频率 1MHz (1us/tick)
 * ARR=19999 -> 周期 20000us = 20ms = 50Hz
 * SG90 规格: 脉宽 0.5ms-2.5ms 对应 0-180°
 *   0°   = 500  (0.5ms)
 *   90°  = 1500 (1.5ms)
 *   180° = 2500 (2.5ms)
 */

uint8_t g_door_state = 0;   /* 全局门锁状态: 0=关锁 1=开锁 */

void Servo_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_BaseStruct;
    TIM_OCInitTypeDef TIM_OCStruct;

    /* 1. 开时钟: GPIOA, TIM5 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    /* 2. PA1 复用推挽输出 (TIM5_CH2) */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 3. 定时器基础配置 (50Hz, 20ms 周期) */
    TIM_BaseStruct.TIM_Period = 19999;          /* ARR */
    TIM_BaseStruct.TIM_Prescaler = 71;          /* PSC, 1MHz */
    TIM_BaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_BaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM5, &TIM_BaseStruct);

    /* 4. PWM 模式 1 配置 CH2 */
    TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStruct.TIM_Pulse = 500;               /* 初始 0° (0.5ms) */
    TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM5, &TIM_OCStruct);

    /* 预装载使能, 让 CCR 改写平滑生效 */
    TIM_OC2PreloadConfig(TIM5, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM5, ENABLE);

    /* 5. 启动定时器 */
    TIM_Cmd(TIM5, ENABLE);

    /* 初始状态: 关锁 */
    g_door_state = 0;
    delay_ms(500);   /* 等舵机就位 */
}

/* 设置舵机角度 (0-180) */
void Servo_SetAngle(uint8_t angle)
{
    if(angle > 180) angle = 180;
    uint16_t ccr = 500 + (uint16_t)angle * 2000 / 180;
    TIM_SetCompare2(TIM5, ccr);
}

/* 关锁 (0°) */
void Servo_Lock(void)
{
    if(g_door_state == 0) return;   /* 已是关锁状态 */
    Servo_SetAngle(0);
    delay_ms(800);                 /* 等舵机动作到位 */
    g_door_state = 0;
}

/* 开锁 (90°) */
void Servo_Unlock(void)
{
    if(g_door_state == 1) return;   /* 已是开锁状态 */
    Servo_SetAngle(90);
    delay_ms(800);                 /* 等舵机动作到位 */
    g_door_state = 1;
}

/* 通知门锁状态变化 (其他模块直接操作 PWM 后调用此函数) */
void Servo_SetState(uint8_t state)
{
    g_door_state = state;
}

/* 获取当前门锁状态: 0=关, 1=开 */
uint8_t Servo_GetState(void)
{
    return g_door_state;
}

#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

/* 舵机参数: TIM5_CH2 (PA1), 50Hz PWM
 * 使用 TIM5 而非 TIM2, 避免与 AS608 的 TIM2 间隔检测冲突
 * 1ms = 0°, 1.5ms = 90°, 2ms = 180°
 */

/* 全局门锁状态 (0=关锁 1=开锁)
 * 其他模块(指纹/NFC)直接操作 PWM 控制舵机后, 必须调用 Servo_SetState 通知此变量
 */
extern uint8_t g_door_state;

void Servo_Init(void);              /* 初始化 TIM5_CH2 PWM */
void Servo_SetAngle(uint8_t angle); /* 设置角度 0-180 */
void Servo_Lock(void);              /* 关门动作 (0°) */
void Servo_Unlock(void);            /* 开门动作 (90°) */
void Servo_SetState(uint8_t state); /* 通知门锁状态变化(不控制舵机, 仅更新变量) */
uint8_t Servo_GetState(void);       /* 获取当前状态 0=关 1=开 */

#endif

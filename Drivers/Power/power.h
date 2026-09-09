#ifndef __POWER_H
#define __POWER_H

#include "stm32f10x.h"

/* ============================================================
 * 低功耗管理 (STM32 STOP 模式)
 *
 * 调用 Enter_LowPower() 进入 STOP 模式, 等待任意按键 EXTI 唤醒.
 * 唤醒后自动恢复系统时钟 (PLL 72MHz) 和外设状态.
 *
 * STOP 模式期间:
 *   - CPU 停止, 大部分外设时钟停止 (~20uA)
 *   - WiFi/RFID/指纹全部暂停, 唤醒后自动恢复
 *   - 通过 PF4~PF7 (列线) 的下降沿 EXTI 唤醒
 * ============================================================ */

/* 长按阈值: 300 * ~10ms = 3 秒长按进入低功耗 */
#define LONG_PRESS_THRESHOLD  300

/* 进入低功耗模式 (阻塞调用, 唤醒后返回) */
void Enter_LowPower(void);

#endif

#include "delay.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
/*
 * SysTick 滴答定时器延时
 *   delay_init(72)  — 初始化 (72MHz 主频)
 *   delay_us(n)     — 微秒延时 (不占用中断, 阻塞查询)
 *   delay_ms(n)     — 毫秒延时
 *
 * 原理: SysTick 是 Cortex-M3 内核自带的 24 位递减计数器
 *   时钟源选内核时钟 (HCLK = 72MHz)
 *   计一个数 = 1/72MHz ≈ 13.9ns
 *   delay_us: 重装载值 = us * 72
 *   delay_ms: 循环调用 delay_us, 每次最多 1864ms (24位最大值限制)
 */

void delay_init(void)
{
}

/*
 * 毫秒延时
 *
 * 参数：nms — 延时多少毫秒
 *
 * 两种工作模式：
 *   模式 1：调度器未启动（初始化阶段）
 *           → 软件循环死等（调用 delay_us）
 *           → 阻塞 CPU，没办法只能这样
 *
 *   模式 2：调度器已启动（任务运行中）
 *           → 调用 vTaskDelay(nms)
 *           → 任务进入阻塞态，让出 CPU 给其他任务
 *           → 时间到了自动唤醒
 *
 * 为什么要分两种模式？
 *   初始化的时候调度器还没启动，不能调用 vTaskDelay
 *   （会直接 crash），所以只能用软件循环
 */
void delay_ms(uint32_t nms)
{
    /* xTaskGetSchedulerState() 返回当前调度器状态 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        /* 调度器已经在跑了 → 用 FreeRTOS 的任务延时 */
        vTaskDelay(nms);
    } else {
        /* 调度器还没启动 → 软件循环死等 */
        while (nms--) {
            delay_us(1000);  /* 1ms = 1000us */
        }
    }
}

void delay_us(uint32_t nus)
{
    volatile uint32_t i;  /* volatile 防止编译器优化掉这个循环 */
    for (i = 0; i < nus * 7; i++) {
        __NOP();          /* 空操作指令，占 1 个 CPU 周期 */
    }
}


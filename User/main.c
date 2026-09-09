/*********************************************************************
 *  智能门锁 — main.c (FreeRTOS 版)
 *
 *  本文件只负责两件事：
 *    1. 初始化所有硬件外设
 *    2. 调用 freertos_demo() 启动 FreeRTOS
 *
 *  所有业务逻辑（按键、OLED、刷卡、指纹、WiFi）都在 freertos_test.c
 *  的各个任务中实现，main.c 不关心具体业务。
 *
 *  硬件初始化顺序很重要（有依赖关系）：
 *    1. NVIC 优先级分组 → 必须最先
 *    2. delay / usart → 串口打印要尽早
 *    3. OLED → 显示启动进度（OLED 先初始化 SPI2）
 *    4. W25Q128 → 用 SPI2，必须在 OLED 之后
 *    5. Servo → 舵机
 *    6. RC522 + 白名单 → SPI1，独立
 *    7. AS608 指纹 → USART2 + TIM2
 *    8. 键盘 → GPIO
 *    9. ESP8266 WiFi → USART3，初始化最慢，放最后
 *
 *  Author: Smart Door Lock Project
 *********************************************************************/

#include "stm32f10x.h"
#include "door_access.h"
#include "rc522.h"
#include "usart.h"
#include "delay.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>
#include "servo.h"
#include "as608.h"
#include "as608_uart.h"
#include "w25qxx.h"
#include "keypad.h"
#include "OLED.h"
#include "beep.h"
#include "led.h"
#include "esp8266.h"
#include "freertos_test.h"  /* FreeRTOS 入口 */

int main(void)
{
    u32 flash_id;            /* W25Q128 Flash ID */ 
	
    /* ============================================================
     *  第 1 步：NVIC 优先级分组
     *  必须最先调用，因为后面所有中断初始化都依赖这个设置
     *
     *  NVIC_PriorityGroup_2 = 2 位抢占优先级 + 2 位子优先级
     *  抢占优先级范围：0~3（数值越小优先级越高）
     *  子优先级范围：0~3
     *
     *  FreeRTOS 要求：所有调用 FromISR API 的中断，
     *  抢占优先级必须 >= 某个阈值（由 configMAX_SYSCALL_INTERRUPT_PRIORITY 决定）
     * ============================================================ */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* ============================================================
     *  第 2 步：系统基础组件
     * ============================================================ */
    delay_init();                                    /* 延时函数初始化（FreeRTOS版里是空的） */
    usart_init(115200);                              /* USART1 初始化，波特率 115200 */
    printf("=== Smart Door Lock (FreeRTOS + WiFi) ===\r\n");

    beep_init();   									/* 蜂鸣器初始化 */
	led_init();
    /* ============================================================
     *  第 3 步：OLED 显示
     *  OLED 用 SPI2，先初始化，后面 W25Q128 也要用 SPI2
     *  顺便显示 "Booting..." 给用户看
     * ============================================================ */
    OLED_Init();
    OLED_ShowCenter(16, "Booting...");
    OLED_Refresh();
    printf("OLED init done\r\n");

    /* ============================================================
     *  第 4 步：W25Q128 SPI Flash
     *  读 ID 验证芯片是否正常
     *  W25Q128 ID = 0xEF4018
     * ============================================================ */
    W25Q_Init();
    flash_id = W25Q_ReadID();
    printf("W25Q128 ID: 0x%06X\r\n", (unsigned int)flash_id);
    if (flash_id == 0xEF4018) {
        OLED_ShowCenter(32, "Flash OK");
        OLED_Refresh();
    } else {
        OLED_ShowCenter(32, "Flash FAIL");
        OLED_Refresh();
    }
    delay_ms(500);  /* 停一下让人看到结果 */

    /* ============================================================
     *  第 5 步：SG90 舵机（TIM5_CH2，PA1 引脚 PWM 输出）
     * ============================================================ */
    Servo_Init();

    /* ============================================================
     *  第 6 步：RC522 RFID 模块 + 白名单加载
     *  白名单存在 W25Q128 里，Door_Access_Init 会自动加载
     * ============================================================ */
    Door_Access_Init();
    printf("RC522 init done\r\n");

    /* ============================================================
     *  第 7 步：AS608 指纹模块
     *  USART2 (PA2=TX, PA3=RX) 波特率 57600
     *  TIM2 用于 UART 数据包间隔检测
     *  PB7 = WAK 引脚（检测手指是否放上）
     * ============================================================ */
    AS608_USART2_Init(57600);
    AS608_TIM2_Init();
    PS_StaGPIO_Init();
    delay_ms(200);  /* 模块上电稳定时间 */

    if (PS_HandShake(&AS608Addr) == 0) {
        printf("AS608 OK, addr: 0x%08X\r\n", AS608Addr);
        set_as608_ok(1);
    } else {
        printf("AS608 FAIL\r\n");
		set_as608_ok(0); 
    }

    /* ============================================================
     *  第 8 步：4x4 矩阵键盘（PF0~PF3 行，PF4~PF7 列）
     * ============================================================ */
    KeyPad_Init();
    printf("Keypad init done\r\n");

   /* ============================================================
     *  第 9 步：ESP8266 硬件初始化
     *  只初始化串口！真正的联网放到 WiFi_Task 后台做，
     *  不阻塞主流程 —— 没有 WiFi 也能立刻用按键/刷卡/指纹
     * ============================================================ */
    ESP8266_USART3_Init();

    printf("All modules ready, starting FreeRTOS...\r\n");

    /* ============================================================
     *  第 10 步：启动 FreeRTOS
     *
     *  freertos_demo() 内部会：
     *    1. 创建 start_task 启动任务
     *    2. 调用 vTaskStartScheduler() 启动调度器
     *    3. 从此不再返回
     *
     *  后面的 while(1) 永远不会执行，
     *  只是为了语法完整性和防止意外返回
     * ============================================================ */
    freertos_demo();

    while (1) {
        /* 永远不会到这里 */
    }
}

#include "power.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "delay.h"
#include "OLED.h"
#include "beep.h"
#include "keypad.h"
#include "app_ui.h"
#include "app_state.h"
#include <stdio.h>

/* 睡眠时拉低的行线 / 用作 EXTI 触发的列线 */
#define SLEEP_ROW_PINS  (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3)
#define SLEEP_COL_PINS  (GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7)

/* ============================================================
 * EXTI 配置 (STOP 模式唤醒用)
 * PF4~PF7 (列线) 配为下降沿中断, 某一行拉低时按键触发
 * ============================================================ */
static void Sleep_EXTI_Setup(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    for(i = 0; i < 4; i++) {
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOF, GPIO_PinSource4 + i);
        exti.EXTI_Line    = (EXTI_Line4 << i);
        exti.EXTI_Mode    = EXTI_Mode_Interrupt;
        exti.EXTI_Trigger = EXTI_Trigger_Falling;
        exti.EXTI_LineCmd = ENABLE;
        EXTI_Init(&exti);
    }

    nvic.NVIC_IRQChannel                   = EXTI4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel                   = EXTI9_5_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority        = 1;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

/* 清除 EXTI 配置 (唤醒后调用) */
static void Sleep_EXTI_Cleanup(void)
{
    EXTI_InitTypeDef exti;
    uint8_t i;

    for(i = 0; i < 4; i++) {
        exti.EXTI_Line    = (EXTI_Line4 << i);
        exti.EXTI_LineCmd = DISABLE;
        EXTI_Init(&exti);
        EXTI_ClearITPendingBit(EXTI_Line4 << i);
    }
    NVIC_DisableIRQ(EXTI4_IRQn);
    NVIC_DisableIRQ(EXTI9_5_IRQn);
}

/* 唤醒后恢复系统时钟到 PLL 72MHz
 * STOP 模式唤醒后系统时钟 = HSI 8MHz, 需重新切到 PLL */
static void SYSCLK_ResumeFromSTOP(void)
{
    RCC_HSEConfig(RCC_HSE_ON);
    if(RCC_WaitForHSEStartUp() == SUCCESS) {
        RCC_PLLCmd(ENABLE);
        while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        while(RCC_GetSYSCLKSource() != 0x08);
    }
}

/* ============================================================
 * 进入低功耗模式
 *   1. 显示提示  2. 等待按键释放  3. 关 OLED  4. 配 EXTI
 *   5. 拉低行线   6. 进入 STOP   7. 唤醒后恢复
 * ============================================================ */
void Enter_LowPower(void)
{
    UI_ShowMessage("Sleep mode", "Press key to wake");
    Beep_OK();
    vTaskDelay(pdMS_TO_TICKS(800));

    while(KeyPad_IsAnyKeyPressed()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    OLED_DisplayOff();
    Sleep_EXTI_Setup();

    /* 拉低所有行线, 任一列线变低即触发 EXTI */
    GPIO_ResetBits(GPIOF, SLEEP_ROW_PINS);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

    /* 暂停 FreeRTOS 调度器 (防止 tick 中断干扰 STOP) */
    vTaskSuspendAll();

    printf("[Sleep] Entering STOP mode...\r\n");
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

    /* ===== 唤醒点 =====
     * 某按键 -> EXTI 中断 -> CPU 唤醒, 此时系统时钟 = HSI 8MHz */

    /* 恢复行线为高 (恢复键盘扫描) */
    GPIO_SetBits(GPIOF, SLEEP_ROW_PINS);

    Sleep_EXTI_Cleanup();
    SYSCLK_ResumeFromSTOP();
    xTaskResumeAll();

    OLED_DisplayOn();
    App_Reset();   /* 唤醒后复位状态机，避免长按触发键残留状态 */
    UI_ShowMessage("Woke up", "");
    Beep_Click();
    vTaskDelay(pdMS_TO_TICKS(800));
    UI_IdleScreen();

    printf("[Sleep] Woke up from STOP mode\r\n");
}

/* ============================================================
 * EXTI 中断服务程序 (STOP 模式唤醒用)
 * 只清标志位, 不做其他操作, WFI 检测到中断即退出
 * ============================================================ */
void EXTI4_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line4) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line4);
    }
}

void EXTI9_5_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line5) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
    if(EXTI_GetITStatus(EXTI_Line6) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line6);
    }
    if(EXTI_GetITStatus(EXTI_Line7) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line7);
    }
}

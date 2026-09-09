#include "freertos_test.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timer.h"

#include <stdio.h>
#include "door_access.h"   /* Door_Access_Scan, Door_Get_Event, Password_Load */
#include "rc522.h"          /* (RFID_Init 由 Door_Access_Init 调用) */
#include "usart.h"          /* printf */
#include "delay.h"
#include "servo.h"          /* Servo_GetState */
#include "as608.h"          /* PS_Sta, press_FR */
#include "keypad.h"         /* KeyPad_Scan, KeyPad_IsAnyKeyPressed */
#include "OLED.h"
#include "beep.h"
#include "esp8266.h"        /* ESP8266_Init_Connect, Handle_WiFi_Downlink, ... */
#include "app_ui.h"         /* UI_IdleScreen, Screen_WakeUp, Screen_TickIdle */
#include "app_state.h"      /* Handle_Key, Handle_Door_Event, App_GetState */
#include "power.h"          /* Enter_LowPower, LONG_PRESS_THRESHOLD */

/* ============================================================
 *  AS608 模块就绪标志 (由 main.c 在握手成功后通过 set_as608_ok 设置)
 * ============================================================ */
static uint8_t as608_ok = 0;
void set_as608_ok(uint8_t ok){ as608_ok = ok; }

/* ============================================================
 *  任务声明 / 栈深 / 优先级
 * ============================================================ */
void start_task(void *pvParameters);
#define START_TASK_STACK_SIZE   128
#define START_TASK_PRIO         1
static TaskHandle_t start_task_handle = NULL;

void RFID_Task(void *pvParameters);
#define RFID_TASK_STACK_SIZE    256
#define RFID_TASK_PRIO          4
static TaskHandle_t rfid_task_handle = NULL;

void Main_Task(void *pvParameters);
#define MAIN_TASK_STACK_SIZE    512
#define MAIN_TASK_PRIO          3
static TaskHandle_t main_task_handle = NULL;

void WiFi_Task(void *pvParameters);
#define WIFI_TASK_STACK_SIZE    1024
#define WIFI_TASK_PRIO          2
static TaskHandle_t wifi_task_handle = NULL;

/* ============================================================
 *  RFID 二值信号量 (TIM4 100ms 中断释放, RFID_Task 消费)
 * ============================================================ */
SemaphoreHandle_t xRFID_Sem = NULL;

/* ============================================================
 *  RFID 扫描任务
 *  等到信号量后调用 Door_Access_Scan() 扫一次卡
 * ============================================================ */
void RFID_Task(void *pvParameters)
{
    while (1) {
        if (xSemaphoreTake(xRFID_Sem, portMAX_DELAY) == pdTRUE) {
            Door_Access_Scan();
        }
    }
}

/* ============================================================
 *  主任务: 10ms 周期采集 (按键 / 门禁事件 / 指纹) 并分发
 *  同时负责屏幕休眠计数和长按进入低功耗
 * ============================================================ */
void Main_Task(void *pvParameters)
{
    Password_Load();
    UI_IdleScreen();

    while (1) {
        uint8_t ev = DOOR_EVENT_NONE;
        uint8_t key = KEY_NONE;
        uint8_t finger = 0;
        static uint16_t hold_cnt = 0;

        /* 1. 采集事件 */
        ev = Door_Get_Event();
        key = KeyPad_Scan();
        if (as608_ok && App_GetState() == ST_IDLE && PS_Sta == 1)
            finger = 1;

        /* 2. 任意活动 -> 唤醒屏幕并重置休眠计数 */
        if (ev != DOOR_EVENT_NONE || key != KEY_NONE || finger) {
            Screen_WakeUp();
        }

        /* 3. 分发门禁事件 */
        if (ev != DOOR_EVENT_NONE)
            Handle_Door_Event(ev);

        /* 4. 分发按键 */
        if (key != KEY_NONE) {
            printf("Key: %c\r\n", key);
            Handle_Key(key);
        }

        /* 5. 处理指纹 */
        if (finger) {
            UI_ShowMessage("Verifying...", "");
            printf("Finger detected\r\n");
            if (press_FR() == 1) {
                Door_Unlock();
            } else {
                UI_ShowMessage("DENIED", "");
                Beep_Err();
                vTaskDelay(pdMS_TO_TICKS(1200));
                UI_IdleScreen();
            }
            /* 等待手指移开，避免重复触发（用 PS_GetImage 判断，不依赖触摸脚） */
            uint16_t release_wait = 0;
            while (release_wait < 60) {   /* 最多等 3 秒 */
                vTaskDelay(pdMS_TO_TICKS(50));
                release_wait++;
                if (PS_GetImage() != 0x00) break;  /* 手指已移开 */
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        /* 6. 屏幕休眠计数 (仅在空闲态累计) */
        if (ev == DOOR_EVENT_NONE && key == KEY_NONE && !finger) {
            Screen_TickIdle(App_GetState() == ST_IDLE);
        }

        /* 7. 长按检测: 持续按住任意键 3 秒 -> 进入低功耗 */
        if (KeyPad_IsAnyKeyPressed()) {
            hold_cnt++;
            if (hold_cnt >= LONG_PRESS_THRESHOLD) {
                hold_cnt = 0;
                Enter_LowPower();
                continue;
            }
        } else {
            hold_cnt = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ============================================================
 *  WiFi 任务: 后台联网 + 状态上报 + 处理下行指令
 * ============================================================ */
void WiFi_Task(void *pvParameters)
{
    uint8_t  wifi_retries = 0;
    uint16_t heartbeat_cnt = 0;
    uint16_t state_check_cnt = 0;

    vTaskDelay(pdMS_TO_TICKS(1200));   /* 等 ESP8266 上电稳定 */

    wifi_retries = 0;
    while (ESP8266_Init_Connect() == 0 && wifi_retries < 3) {
        printf("[System] WiFi init failed, retry %d/3 in 5s...\r\n", wifi_retries + 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        wifi_retries++;
    }

    if (wifi_retries < 3) {
        printf("[System] WiFi connected\r\n");
        ESP_Upload_DoorState(Servo_GetState());
    } else {
        printf("[System] WiFi failed, running local-only mode\r\n");
    }
    WiFi_SetLastState(Servo_GetState());

    while (1) {
        Handle_WiFi_Downlink();

        state_check_cnt++;
        if (state_check_cnt >= 100) {
            state_check_cnt = 0;
            uint8_t cur = Servo_GetState();
            if (cur != WiFi_GetLastState()) {
                printf("[StateChange]\r\n");
                ESP_Upload_DoorState(cur);
                WiFi_SetLastState(cur);
            }
        }

        heartbeat_cnt++;
        if (heartbeat_cnt >= 4500) {
            ESP_SendHeartbeat();
            heartbeat_cnt = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ============================================================
 *  启动任务: 创建信号量 / 其他任务 / TIM4, 然后删除自己
 * ============================================================ */
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();

    xRFID_Sem = xSemaphoreCreateBinary();
    if (xRFID_Sem != NULL) {
        printf("[FreeRTOS] RFID semaphore created\r\n");
    }

    xTaskCreate((TaskFunction_t)RFID_Task,
                (char *)"RFID",
                (configSTACK_DEPTH_TYPE)RFID_TASK_STACK_SIZE,
                (void *)NULL,
                (UBaseType_t)RFID_TASK_PRIO,
                (TaskHandle_t *)&rfid_task_handle);

    xTaskCreate((TaskFunction_t)Main_Task,
                (char *)"Main",
                (configSTACK_DEPTH_TYPE)MAIN_TASK_STACK_SIZE,
                (void *)NULL,
                (UBaseType_t)MAIN_TASK_PRIO,
                (TaskHandle_t *)&main_task_handle);

    xTaskCreate((TaskFunction_t)WiFi_Task,
                (char *)"WiFi",
                (configSTACK_DEPTH_TYPE)WIFI_TASK_STACK_SIZE,
                (void *)NULL,
                (UBaseType_t)WIFI_TASK_PRIO,
                (TaskHandle_t *)&wifi_task_handle);

    /* TIM4 必须在 xRFID_Sem 创建之后初始化, 因为其中断会释放信号量 */
    Timer4_Init(100);
    printf("[FreeRTOS] TIM4 started, 100ms tick\r\n");

    taskEXIT_CRITICAL();

    vTaskDelete(NULL);
}

/* ============================================================
 *  FreeRTOS 入口 (由 main.c 调用)
 * ============================================================ */
void freertos_demo(void)
{
    printf("Starting FreeRTOS...\r\n");

    xTaskCreate((TaskFunction_t)start_task,
                (char *)"start_task",
                (configSTACK_DEPTH_TYPE)START_TASK_STACK_SIZE,
                (void *)NULL,
                (UBaseType_t)START_TASK_PRIO,
                (TaskHandle_t *)&start_task_handle);

    vTaskStartScheduler();

    /* 走到这里说明调度器启动失败 (一般是 heap 不足) */
    printf("[FATAL] FreeRTOS scheduler start failed!\r\n");
    printf("[FATAL] Check configTOTAL_HEAP_SIZE\r\n");
    while (1) {
        BEEP_ON(); delay_ms(200); BEEP_OFF(); delay_ms(200);
    }
}

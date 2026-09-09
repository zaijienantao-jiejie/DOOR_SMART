#include "esp8266.h"
#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>
#include "delay.h"
#include "led.h"
#include "FreeRTOS.h"
#include "task.h"
#include "servo.h"
#include "beep.h"
#include "app_ui.h"

#define ESP_USART USART3

char esp_rx_buf[256];
uint16_t esp_rx_idx = 0;

/* LED 闪烁 n 次（WiFi 状态提示，D1 低电平点亮） */
void Beep_N(uint8_t n)
{
    uint8_t i;
    for(i = 0; i < n; i++)
    {
        LED1_ON();
        delay_ms(100);
        LED1_OFF();
        delay_ms(150);
    }
}

/* LED 长亮 1s（WiFi 失败提示） */
void Beep_Fail(void)
{
    LED1_ON();
    delay_ms(1000);
    LED1_OFF();
    delay_ms(500);
}

/* 调试输出: 统一使用 printf (USART1), 由 main.c 中 usart_init() 初始化 */
/* 打印 esp_rx_buf 内容 */
static void Debug_PrintBuf(void)
{
    printf("[RX] %s\r\n", esp_rx_buf);
}

void ESP_SendByte(uint8_t dat)
{
    while(USART_GetFlagStatus(ESP_USART, USART_FLAG_TXE) == RESET);
    USART_SendData(ESP_USART, dat);
}

void ESP_SendString(char *str)
{
    while(*str != '\0')
    {
        ESP_SendByte(*str++);
    }
}

void ESP_ClearBuf(void)
{
    memset(esp_rx_buf, 0, sizeof(esp_rx_buf));
    esp_rx_idx = 0;
}

void ESP_Receive_IRQ(void)
{
    /* 读 DR 清 RXNE 标志, 避免中断风暴 */
    uint8_t dat = USART_ReceiveData(ESP_USART);
    if(esp_rx_idx < 254)
    {
        esp_rx_buf[esp_rx_idx++] = dat;
        esp_rx_buf[esp_rx_idx] = '\0';   /* 保证字符串安全终止 */
    }
}

uint8_t ESP_StrFind(char *src, char *dst)
{
    if(strstr(src, dst) != NULL) return 1;
    return 0;
}

/* 解析巴法云下行控制指令
 * 巴法云下行格式: "cmd=2&uid=xxx&topic=xxx&msg=开锁" 或 "msg=关锁"
 * 用 UTF-8 转义匹配中文, 避免文件编码问题
 * 返回: 0=无指令, 1=开锁, 2=关锁
 */
uint8_t ESP_ParseDownlink(void)
{
    char *p;
    if((p = strstr(esp_rx_buf, "&msg=")) != NULL)
    {
        if(strstr(p, MSG_UNLOCK))  return 1;   /* 开锁 */
        if(strstr(p, MSG_LOCK))    return 2;   /* 关锁 */
    }
    return 0;
}

void ESP8266_USART3_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    /* USART3 默认引脚: PB10(TX) / PB11(RX) - 对齐 ESP8266 接口 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    /* PB10 TX 复用推挽 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB11 RX 浮空输入 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    USART_InitStruct.USART_BaudRate = 115200;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStruct);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStruct.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    USART_Cmd(USART3, ENABLE);
}

/* 非透传模式发送 TCP 数据
 * AT+CIPSEND=N, 等待 > 后发数据, 检查 SEND OK */
uint8_t ESP_SendTCP(char *data)
{
    uint8_t retry;
    uint16_t len = strlen(data);
    char cmd[32];

    sprintf(cmd, "AT+CIPSEND=%u\r\n", len);

    for(retry = 0; retry < 3; retry++)
    {
        ESP_ClearBuf();
        ESP_SendString(cmd);
        delay_ms(500);
        if(ESP_StrFind(esp_rx_buf, ">"))
        {
            ESP_ClearBuf();
            ESP_SendString(data);
            delay_ms(1000);
            if(ESP_StrFind(esp_rx_buf, "SEND OK"))
            {
                return 1;
            }
            if(ESP_StrFind(esp_rx_buf, "SEND FAIL") ||
               ESP_StrFind(esp_rx_buf, "ERROR"))
            {
                return 0;
            }
        }
        delay_ms(300);
    }
    return 0;
}

/* 初始化 ESP8266: 连接 WiFi + TCP + 订阅主题 */
uint8_t ESP8266_Init_Connect(void)
{
    uint8_t retry;
    uint8_t wifi_ok = 0;
    uint8_t tcp_ok = 0;

    printf("\r\n==== ESP8266 Init Start ====\r\n");

    /* AT 测试 */
    for(retry = 0; retry < 5; retry++)
    {
        ESP_ClearBuf();
        ESP_SendString("AT\r\n");
        delay_ms(1000);
        if(ESP_StrFind(esp_rx_buf, "OK"))
        {
            printf("[OK] AT test\r\n");
            break;
        }
    }
    if(retry >= 5)
    {
        printf("[FAIL] AT no response, check baud/power/wiring\r\n");
        Beep_Fail();
        return 0;
    }

    /* CWMODE=3 (Station+AP 混合模式) */
    ESP_ClearBuf();
    ESP_SendString("AT+CWMODE=3\r\n");
    delay_ms(1000);
    printf("[CWMODE=3] ");
    Debug_PrintBuf();

       /* ===== WiFi 连接（支持两个 WiFi 自动切换） ===== */
    {
        const char *ssid_list[2] = { WIFI1_SSID, WIFI2_SSID };
        const char *pwd_list[2]  = { WIFI1_PASSWORD, WIFI2_PASSWORD };
        uint8_t w;
        uint8_t t;

        /* 先断开残留连接，避免 "ALREADY CONNECTED" 干扰切换 */
        ESP_ClearBuf();
        ESP_SendString("AT+CWQAP\r\n");
        delay_ms(2000);

        /* 遍历每个 WiFi，先连 WIFI1，连不上连 WIFI2 */
        for(w = 0; w < 2 && !wifi_ok; w++)
        {
            printf("[Try WiFi] %s\r\n", ssid_list[w]);

            /* 每个 WiFi 尝试 2 次 */
            for(t = 0; t < 2; t++)
            {
                char wifi_cmd[128];
                uint16_t wait;
                ESP_ClearBuf();
                sprintf(wifi_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n",
                        ssid_list[w], pwd_list[w]);
                ESP_SendString(wifi_cmd);

                /* 轮询等待结果：最多 8 秒，检测到结果就提前退出
                 * 没有该 WiFi 时模块几秒内就会返回 FAIL/ERROR，
                 * 不用死等 8 秒 */
                for(wait = 0; wait < 80; wait++)
                {
                    delay_ms(100);
                    if(ESP_StrFind(esp_rx_buf, "WIFI GOT IP") ||
                       ESP_StrFind(esp_rx_buf, "FAIL") ||
                       ESP_StrFind(esp_rx_buf, "ERROR"))
                        break;
                }

                printf("[WiFi try ");
                Debug_PrintBuf();
                if(ESP_StrFind(esp_rx_buf, "WIFI GOT IP"))
                {
                    wifi_ok = 1;
                    printf("[OK] WiFi connected\r\n");
                    Beep_N(2);
                    break;
                }
                printf("[FAIL] WiFi retry\r\n");
                delay_ms(1000);
            }
        }
    }

    if(!wifi_ok)
    {
        printf("[FAIL] WiFi connect failed\r\n");
        Beep_Fail(); Beep_Fail();
        return 0;
    }

    /* CIPMUX=0 (单连接, 非透传) */
    ESP_ClearBuf();
    ESP_SendString("AT+CIPMUX=0\r\n");
    delay_ms(800);

    /* TCP 连接巴法云 */
    for(retry = 0; retry < 5; retry++)
    {
        ESP_ClearBuf();
        ESP_SendString("AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n");
        delay_ms(3000);
        printf("[CIPSTART ");
        Debug_PrintBuf();
        if(ESP_StrFind(esp_rx_buf, "CONNECT") &&
           !ESP_StrFind(esp_rx_buf, "CONNECT FAIL") &&
           !ESP_StrFind(esp_rx_buf, "ERROR") &&
           !ESP_StrFind(esp_rx_buf, "DNS"))
        {
            tcp_ok = 1;
            printf("[OK] TCP connected\r\n");
            Beep_N(3);
            break;
        }
        if(ESP_StrFind(esp_rx_buf, "ALREADY"))
        {
            tcp_ok = 1;
            printf("[OK] TCP already connected\r\n");
            Beep_N(3);
            break;
        }
        printf("[FAIL] TCP retry\r\n");
        delay_ms(2000);
    }
    if(!tcp_ok)
    {
        printf("[FAIL] TCP connect failed\r\n");
        Beep_Fail(); Beep_Fail(); Beep_Fail();
        return 0;
    }

    /* 订阅主题: cmd=1&uid=xxx&topic=xxx, 服务器回复 cmd=1&res=1 */
    {
        char sub_buf[200];
        sprintf(sub_buf, "cmd=1&uid=%s&topic=%s\r\n", BEMFA_UID, BEMFA_TOPIC);
        printf("[Subscribe] %s", sub_buf);
        if(ESP_SendTCP(sub_buf))
        {
            delay_ms(1000);
            printf("[Server ");
            Debug_PrintBuf();
            if(ESP_StrFind(esp_rx_buf, "cmd=1&res=1"))
            {
                printf("[OK] Subscribe success\r\n");
                Beep_N(4);
            }
            else
            {
                printf("[WARN] Subscribe not confirmed, but will continue...\r\n");
                Beep_N(4);
            }
        }
        else
        {
            printf("[FAIL] Send subscribe failed\r\n");
            Beep_Fail(); Beep_Fail(); Beep_Fail(); Beep_Fail();
            return 0;
        }
    }

    printf("==== ESP8266 Init Success ====\r\n");
    return 1;
}

/* 重连: 重新建立 WiFi + TCP */
uint8_t ESP_Reconnect(void)
{
    printf("\r\n[Reconnect] TCP lost, reconnecting...\r\n");
    Beep_N(5);

    ESP_ClearBuf();
    ESP_SendString("AT+CIPCLOSE\r\n");
    delay_ms(800);

    return ESP8266_Init_Connect();
}

/* 心跳保活 - 每 45s 发一次, 防止 60s 超时断开 */
uint8_t ESP_SendHeartbeat(void)
{
    char hb_buf[32];
    sprintf(hb_buf, "cmd=0&msg=ping\r\n");
    if(ESP_SendTCP(hb_buf))
    {
        printf("[Heartbeat] ping OK\r\n");
        ESP_ClearBuf();
        return 1;
    }
    printf("[Heartbeat] FAIL, reconnecting...\r\n");
    ESP_Reconnect();
    return 0;
}

void ESP_Upload_DoorState(uint8_t state)
{
    /* 检测 TCP 是否断开 */
    if(ESP_StrFind(esp_rx_buf, "CLOSED") ||
       ESP_StrFind(esp_rx_buf, "link is not"))
    {
        ESP_Reconnect();
        return;
    }

    /* 上传格式: cmd=2&uid=xxx&topic=xxx&msg=开锁/关锁 */
    const char *state_str = (state == 1) ? MSG_UNLOCK : MSG_LOCK;
    char send_buf[200];
    char dbg_buf[64];
    sprintf(send_buf, "cmd=2&uid=%s&topic=%s&msg=%s\r\n",
            BEMFA_UID, BEMFA_TOPIC, state_str);
    if(ESP_SendTCP(send_buf))
    {
        sprintf(dbg_buf, "[Upload] %s OK\r\n", state_str);
        printf("%s", dbg_buf);
        ESP_ClearBuf();
    }
    else
    {
        printf("[Upload FAIL] reconnecting...\r\n");
        ESP_Reconnect();
    }
}

/* ============================================================
 * WiFi 下行指令处理
 *   解析巴法云下发的 "开锁"/"关锁" 指令并执行
 *   返回值: 0=无指令, 1=开锁, 2=关锁
 * ============================================================ */
static uint8_t wifi_last_state = 0;

uint8_t WiFi_GetLastState(void) { return wifi_last_state; }
void     WiFi_SetLastState(uint8_t s) { wifi_last_state = s; }

void Handle_WiFi_Downlink(void)
{
    if (esp_rx_idx > 0) {
        uint8_t cmd = ESP_ParseDownlink();
        ESP_ClearBuf();

        if (cmd == 1) {
            Screen_WakeUp();
            printf("[Downlink] %s\r\n", MSG_UNLOCK);
            Beep_Click();
            Servo_Unlock();
            UI_ShowMessage("WiFi Unlock", "");
            vTaskDelay(pdMS_TO_TICKS(1200));
            UI_IdleScreen();
            {
                uint8_t cur = Servo_GetState();
                if (cur != wifi_last_state) {
                    ESP_Upload_DoorState(cur);
                    wifi_last_state = cur;
                }
            }
        } else if (cmd == 2) {
            Screen_WakeUp();
            printf("[Downlink] %s\r\n", MSG_LOCK);
            Beep_Click();
            Servo_Lock();
            UI_ShowMessage("WiFi Lock", "");
            vTaskDelay(pdMS_TO_TICKS(1200));
            UI_IdleScreen();
            {
                uint8_t cur = Servo_GetState();
                if (cur != wifi_last_state) {
                    ESP_Upload_DoorState(cur);
                    wifi_last_state = cur;
                }
            }
        }
    }
}


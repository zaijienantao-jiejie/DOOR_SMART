#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

/*===== 巴法云配置, 请修改后烧录 =====*/
#define BEMFA_UID      "97bf7d1b194d9facb793dbdd857ece16"
#define BEMFA_TOPIC    "lock001"

/*===== WiFi 配置, 请修改后烧录 =====*/
#define WIFI1_SSID      	"TP-LINK_HXZG"
#define WIFI1_PASSWORD 		"Hxzg0309!!"
#define WIFI2_SSID			"ZAIJIENANTAO"
#define WIFI2_PASSWORD		"00000000"

/* 巴法云消息内容 (UTF-8 转义, 避免文件编码问题) */
#define MSG_UNLOCK  "\xE5\xBC\x80\xE9\x94\x81"   /* "开锁" */
#define MSG_LOCK    "\xE5\x85\xB3\xE9\x94\x81"   /* "关锁" */

extern char esp_rx_buf[256];
extern uint16_t esp_rx_idx;
void ESP8266_USART3_Init(void);
void ESP_SendByte(uint8_t dat);
void ESP_SendString(char *str);
void ESP_ClearBuf(void);
void ESP_Receive_IRQ(void);
uint8_t ESP_StrFind(char *src,char *dst);
uint8_t ESP_ParseDownlink(void);
uint8_t ESP_SendTCP(char *data);
uint8_t ESP8266_Init_Connect(void);
uint8_t ESP_Reconnect(void);
uint8_t ESP_SendHeartbeat(void);
void ESP_Upload_DoorState(uint8_t state);
void Beep_N(uint8_t n);
void Beep_Fail(void);

/* ============================================================
 * WiFi 下行指令处理 (由主任务周期调用)
 * 解析巴法云下发的开锁/关锁指令并执行
 * ============================================================ */
void Handle_WiFi_Downlink(void);

/* 获取/设置门锁状态缓存 (供 WiFi_Task 状态变化检测) */
uint8_t WiFi_GetLastState(void);
void     WiFi_SetLastState(uint8_t s);

#endif

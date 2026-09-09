#ifndef __DOOR_ACCESS_H
#define __DOOR_ACCESS_H

#include "stm32f10x.h"

#define MAX_CARDS 4

/* 门禁状态码 */
#define DOOR_GRANTED    1
#define DOOR_DENIED     0
#define DOOR_NO_CARD   (-1)
#define DOOR_READ_FAIL (-2)

/* 门禁工作模式 */
#define DOOR_MODE_NORMAL   0   /* 正常门禁模式 */
#define DOOR_MODE_ADD_CARD 1   /* 添加卡模式: 下一张卡加入白名单 */
#define DOOR_MODE_DEL_CARD 2   /* 删除卡模式: 下一张卡从白名单删除 */

/* 初始化门禁系统 (调用一次) */
void Door_Access_Init(void);

/* 门禁轮询函数 (在 while(1) 中调用), 返回 DOOR_* 状态码 */
int  Door_Access_Scan(void);

/* 添加白名单卡号 (最多 MAX_CARDS 张), 成功返回1, 失败返回0 */
u8   Door_Add_Card(uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3);

/* 删除白名单卡号, 成功返回1, 失败返回0 */
u8   Door_Delete_Card(uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3);

/* 将白名单保存到 W25Q128 Flash */
void Door_Save_Whitelist(void);

/* 从 W25Q128 Flash 加载白名单, 返回加载的数量 */
u8   Door_Load_Whitelist(void);

/* 手动开锁 (指纹/密码等外部验证成功后调用), 3秒后自动关锁 */
void Door_Unlock(void);

/* 设置门禁模式: NORMAL / ADD_CARD / DEL_CARD */
void Door_Set_Mode(uint8_t mode);

/* 获取当前门禁模式 */
uint8_t Door_Get_Mode(void);

/* 获取最近一次读到的卡号, 无则返回NULL */
uint8_t *Door_Get_Last_Card(void);

/* ============================================================
 * 门禁事件 (供主循环轮询, 用于 OLED 等界面刷新)
 * ============================================================ */
#define DOOR_EVENT_NONE      0   /* 无事件 */
#define DOOR_EVENT_GRANTED   1   /* 开锁成功 (刷卡或指纹) */
#define DOOR_EVENT_DENIED    2   /* 验证拒绝 (无效卡) */
#define DOOR_EVENT_LOCKED    3   /* 已关锁 */
#define DOOR_EVENT_ADD_OK    4   /* 添加卡成功 */
#define DOOR_EVENT_ADD_FAIL  5   /* 添加卡失败 (满/重复) */
#define DOOR_EVENT_DEL_OK    6   /* 删除卡成功 */
#define DOOR_EVENT_DEL_FAIL  7   /* 删除卡失败 (不存在) */

/* 读取并清除最近一次门禁事件 */
uint8_t Door_Get_Event(void);

/* ============================================================
 * 密码管理 (W25Q128 sector 1, 6 位数字密码)
 * ============================================================ */
#define PASSWORD_ADDR  0x1000   /* 密码 Flash 存储地址 */
#define PASSWORD_LEN   6        /* 密码长度 6 位 */
#define MAX_TRY        5        /* 最大重试次数 */

extern uint8_t g_password[PASSWORD_LEN];  /* 当前正确密码 */

/* 从 Flash 加载密码, 无效则恢复默认 123456 */
void Password_Load(void);
/* 保存当前密码到 Flash */
void Password_Save(void);

#endif

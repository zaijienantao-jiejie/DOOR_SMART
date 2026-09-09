#ifndef __APP_STATE_H
#define __APP_STATE_H

#include "stm32f10x.h"

/* ============================================================
 * 应用状态机 + 门禁事件分发
 *
 * 该模块负责:
 *   1. 接收按键 (KeyPad_Scan 返回) 并按当前状态处理
 *   2. 接收门禁事件 (Door_Get_Event 返回) 并刷新 UI/蜂鸣
 *
 * 状态定义:
 *   ST_IDLE        - 空闲等待
 *   ST_PWD_INPUT   - 输入开锁密码
 *   ST_PWD_OLD     - 修改密码: 旧密码
 *   ST_PWD_NEW     - 修改密码: 新密码
 *   ST_UNLOCK      - 已开锁, 等待自动关锁
 *   ST_ADMIN_AUTH  - 管理员认证 (添加/删除指纹/卡)
 * ============================================================ */

typedef enum {
    ST_IDLE = 0,
    ST_PWD_INPUT,
    ST_PWD_OLD,
    ST_PWD_NEW,
    ST_UNLOCK,
    ST_ADMIN_AUTH,
} app_state_t;

/* 获取当前应用状态 */
app_state_t App_GetState(void);

/* 设置应用状态 */
void App_SetState(app_state_t s);

/* 复位应用状态机（状态/密码缓冲/管理命令缓存），休眠唤醒后调用 */
void App_Reset(void);

/* 按键处理总入口 (由主任务调用) */
void Handle_Key(uint8_t key);

/* 门禁事件处理 (由主任务调用, ev 来自 Door_Get_Event) */
void Handle_Door_Event(uint8_t ev);

#endif

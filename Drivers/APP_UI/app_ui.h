#ifndef __APP_UI_H
#define __APP_UI_H

#include "stm32f10x.h"

/* ============================================================
 * OLED UI 辅助函数 + 屏幕休眠管理
 *
 * 该模块封装了 OLED 显示的常用界面 (idle/message/password),
 * 以及屏幕无操作自动休眠的计数逻辑.
 *
 * 调用约定:
 *   UI_*      - 主动绘制某个界面 (会立即刷新屏幕)
 *   Screen_*  - 屏幕休眠状态机 (由主任务周期调用)
 * ============================================================ */

/* 屏幕休眠阈值: 3000 * 10ms = 30 秒 */
#define IDLE_TIMEOUT_TICK  3000

/* 将一个字节转为 2 位十六进制字符串 (如 0xA3 -> "A3") */
void UI_HexByte(char *buf, uint8_t val);

/* 在指定 y 行居中显示 4 字节卡号 */
void UI_ShowCardID(uint16_t y, uint8_t *id);

/* 空闲主界面 */
void UI_IdleScreen(void);

/* 通用消息界面 (两行, 第二行可为 NULL) */
void UI_ShowMessage(const char *line1, const char *line2);

/* 密码输入界面 (给定标题) */
void UI_PasswordScreen(const char *title);

/* 刷新密码输入点 (传入已输入的密码长度) */
void UI_RedrawPwd(uint8_t pwd_len);

/* 屏幕唤醒 + 重置休眠计数 (有任意活动时调用) */
void Screen_WakeUp(void);

/* 屏幕休眠计数 (主任务 10ms 调用一次, is_idle 表示当前是否空闲态) */
void Screen_TickIdle(uint8_t is_idle);

#endif

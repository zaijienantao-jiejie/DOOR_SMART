#include "app_state.h"
#include "app_ui.h"
#include "beep.h"
#include "door_access.h"
#include "keypad.h"
#include "as608.h"
#include "OLED.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 应用状态变量
 * ============================================================ */
static app_state_t app_state = ST_IDLE;
static uint8_t last_key = KEY_NONE;
static uint8_t pending_admin = KEY_NONE;
static uint8_t pwd_buf[PASSWORD_LEN];
static uint8_t pwd_len = 0;
static uint8_t try_cnt = 0;

app_state_t App_GetState(void) { return app_state; }
void App_SetState(app_state_t s) { app_state = s; }

/* 复位状态机：清空状态、密码缓冲、管理命令缓存 */
void App_Reset(void)
{
    app_state     = ST_IDLE;
    last_key      = KEY_NONE;
    pending_admin = KEY_NONE;
    pwd_len       = 0;
    try_cnt       = 0;
}

/* ============================================================
 * 密码缓冲区操作
 * ============================================================ */
static void Pwd_Append(uint8_t key)
{
    if (pwd_len < PASSWORD_LEN) {
        pwd_buf[pwd_len] = key;
        pwd_len++;
        UI_RedrawPwd(pwd_len);
    }
}

static void Pwd_Delete(void)
{
    if (pwd_len > 0) {
        pwd_len--;
        UI_RedrawPwd(pwd_len);
    }
}

/* ============================================================
 * 各状态下的按键处理
 * ============================================================ */

/* ST_PWD_INPUT: 输入 6 位开锁密码, # 确认, * 删除/取消 */
static void Handle_PwdInput_Key(uint8_t key)
{
    if (key >= '0' && key <= '9') {
        Beep_Click();
        Pwd_Append(key);
        return;
    }
    if (key == '*') {
        Beep_Click();
        if (pwd_len > 0) Pwd_Delete();
        else { app_state = ST_IDLE; UI_IdleScreen(); }
        return;
    }
    if (key == '#') {
        Beep_Click();
        if (pwd_len == PASSWORD_LEN) {
            if (memcmp(pwd_buf, g_password, PASSWORD_LEN) == 0) {
                try_cnt = 0;
                Door_Unlock();
            } else {
                try_cnt++;
                if (try_cnt >= MAX_TRY) {
                    UI_ShowMessage("LOCKED", "");
                    Beep_Err();
                    vTaskDelay(pdMS_TO_TICKS(2500));
                    try_cnt = 0;
                } else {
                    UI_ShowMessage("WRONG PWD", "");
                    Beep_Err();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                app_state = ST_IDLE;
                UI_IdleScreen();
            }
        }
        return;
    }
}

/* ST_PWD_OLD: 修改密码时输入旧密码验证 */
static void Handle_PwdOld_Key(uint8_t key)
{
    if (key >= '0' && key <= '9') {
        Beep_Click();
        Pwd_Append(key);
        return;
    }
    if (key == '*') {
        Beep_Click();
        if (pwd_len > 0) Pwd_Delete();
        else { app_state = ST_IDLE; UI_IdleScreen(); }
        return;
    }
    if (key == '#') {
        Beep_Click();
        if (pwd_len == PASSWORD_LEN) {
            if (memcmp(pwd_buf, g_password, PASSWORD_LEN) == 0) {
                app_state = ST_PWD_NEW;
                pwd_len = 0;
                UI_PasswordScreen("NEW PWD");
                UI_RedrawPwd(0);
            } else {
                UI_ShowMessage("WRONG PWD", "");
                Beep_Err();
                vTaskDelay(pdMS_TO_TICKS(1000));
                app_state = ST_IDLE;
                UI_IdleScreen();
            }
        }
        return;
    }
}

/* ST_PWD_NEW: 输入新密码并保存 */
static void Handle_PwdNew_Key(uint8_t key)
{
    if (key >= '0' && key <= '9') {
        Beep_Click();
        Pwd_Append(key);
        return;
    }
    if (key == '*') {
        Beep_Click();
        if (pwd_len > 0) Pwd_Delete();
        else { app_state = ST_IDLE; UI_IdleScreen(); }
        return;
    }
    if (key == '#') {
        Beep_Click();
        if (pwd_len == PASSWORD_LEN) {
            uint8_t i;
            for (i = 0; i < PASSWORD_LEN; i++) g_password[i] = pwd_buf[i];
            Password_Save();
            UI_ShowMessage("PWD Changed", "");
            Beep_OK();
            vTaskDelay(pdMS_TO_TICKS(1200));
            app_state = ST_IDLE;
            UI_IdleScreen();
        }
        return;
    }
}

/* ST_ADMIN_AUTH: 验证管理员密码后执行 A/B/C/D 管理命令 */
static void Exec_Admin_Command(uint8_t cmd)
{
    if (cmd == 'A') {
        UI_ShowMessage("Add Fingerprint", "Press finger...");
        Beep_Click();
        Add_FR();
        UI_IdleScreen();
    } else if (cmd == 'B') {
        UI_ShowMessage("Delete FP ID=1", "");
        Beep_Click();
        Del_FR();
        vTaskDelay(pdMS_TO_TICKS(1000));
        UI_IdleScreen();
    } else if (cmd == 'C') {
        Door_Set_Mode(DOOR_MODE_ADD_CARD);
        UI_ShowMessage("Add Card Mode", "Swipe card...");
        Beep_Click();
    } else if (cmd == 'D') {
        Door_Set_Mode(DOOR_MODE_DEL_CARD);
        UI_ShowMessage("Del Card Mode", "Swipe card...");
        Beep_Click();
    }
}

static void Handle_AdminAuth_Key(uint8_t key)
{
    if (key >= '0' && key <= '9') {
        Beep_Click();
        Pwd_Append(key);
        return;
    }
    if (key == '*') {
        Beep_Click();
        if (pwd_len > 0) Pwd_Delete();
        else {
            pending_admin = KEY_NONE;
            app_state = ST_IDLE;
            UI_IdleScreen();
        }
        return;
    }
    if (key == '#') {
        Beep_Click();
        if (pwd_len == PASSWORD_LEN) {
            if (memcmp(pwd_buf, g_password, PASSWORD_LEN) == 0) {
                uint8_t cmd = pending_admin;
                pending_admin = KEY_NONE;
                app_state = ST_IDLE;
                Exec_Admin_Command(cmd);
            } else {
                UI_ShowMessage("WRONG PWD", "");
                Beep_Err();
                vTaskDelay(pdMS_TO_TICKS(1000));
                pending_admin = KEY_NONE;
                app_state = ST_IDLE;
                UI_IdleScreen();
            }
        }
        return;
    }
}

/* ST_IDLE: 空闲状态下的按键映射
 *   0-9  -> 开始输入密码
 *   #    -> 开始输入密码 (空开始)
 *   *    -> 修改密码
 *   A/B/C/D + D -> 管理命令 (需管理员密码确认)
 */
static void Handle_Idle_Key(uint8_t key)
{
    if (key == 'D' && last_key != KEY_NONE) {
        uint8_t first = last_key;
        last_key = KEY_NONE;

        pending_admin = first;
        app_state = ST_ADMIN_AUTH;
        pwd_len = 0;
        UI_PasswordScreen("ADMIN PWD");
        UI_RedrawPwd(0);
        Beep_Click();
        return;
    }

    if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
        last_key = key;
        char hint[20];
        hint[0] = key; hint[1] = '+'; hint[2] = 'D';
        hint[3] = ' '; hint[4] = '='; hint[5] = ' '; hint[6] = 0;
        if (key == 'A')      strcat(hint, "Add FP");
        else if (key == 'B') strcat(hint, "Del FP");
        else if (key == 'C') strcat(hint, "Add CD");
        else                 strcat(hint, "Del CD");
        UI_ShowMessage(hint, "Press D to confirm");
        Beep_Click();
        return;
    }

    last_key = KEY_NONE;

    if (key >= '0' && key <= '9') {
        app_state = ST_PWD_INPUT;
        pwd_len = 0;
        UI_PasswordScreen("INPUT PWD");
        Beep_Click();
        Pwd_Append(key);
        return;
    }
    if (key == '#') {
        app_state = ST_PWD_INPUT;
        pwd_len = 0;
        UI_PasswordScreen("INPUT PWD");
        UI_RedrawPwd(0);
        Beep_Click();
        return;
    }
    if (key == '*') {
        app_state = ST_PWD_OLD;
        pwd_len = 0;
        UI_PasswordScreen("OLD PWD");
        UI_RedrawPwd(0);
        Beep_Click();
        return;
    }
}

/* 按键处理总入口 */
void Handle_Key(uint8_t key)
{
    switch (app_state) {
        case ST_IDLE:       Handle_Idle_Key(key);      break;
        case ST_PWD_INPUT:  Handle_PwdInput_Key(key);  break;
        case ST_PWD_OLD:    Handle_PwdOld_Key(key);     break;
        case ST_PWD_NEW:    Handle_PwdNew_Key(key);     break;
        case ST_ADMIN_AUTH: Handle_AdminAuth_Key(key); break;
        default: break;
    }
}

/* ============================================================
 * 门禁事件处理 (Door_Get_Event 返回的事件)
 * ============================================================ */
void Handle_Door_Event(uint8_t ev)
{
    switch (ev) {
    case DOOR_EVENT_GRANTED:
        UI_ShowMessage("UNLOCKED", "");
        Beep_OK();
        app_state = ST_UNLOCK;
        break;

    case DOOR_EVENT_DENIED:
        UI_ShowMessage("DENIED", "");
        Beep_Err();
        vTaskDelay(pdMS_TO_TICKS(1200));
        UI_IdleScreen();
        break;

    case DOOR_EVENT_LOCKED:
        UI_IdleScreen();
        app_state = ST_IDLE;   /* 关锁后回到空闲态，否则按键会被 default 分支忽略 */
        break;

    case DOOR_EVENT_ADD_OK: {
        uint8_t *card = Door_Get_Last_Card();
        OLED_Clear();
        OLED_ShowCenter(8, "Card Added");
        if (card) UI_ShowCardID(24, card);
        OLED_Refresh();
        Beep_OK();
        vTaskDelay(pdMS_TO_TICKS(1500));
        UI_IdleScreen();
        break;
    }

    case DOOR_EVENT_ADD_FAIL:
        UI_ShowMessage("Add Card FAIL", "");
        Beep_Err();
        vTaskDelay(pdMS_TO_TICKS(1500));
        UI_IdleScreen();
        break;

    case DOOR_EVENT_DEL_OK: {
        uint8_t *card = Door_Get_Last_Card();
        OLED_Clear();
        OLED_ShowCenter(8, "Card Deleted");
        if (card) UI_ShowCardID(24, card);
        OLED_Refresh();
        Beep_OK();
        vTaskDelay(pdMS_TO_TICKS(1500));
        UI_IdleScreen();
        break;
    }

    case DOOR_EVENT_DEL_FAIL:
        UI_ShowMessage("Delete FAIL", "Not found");
        Beep_Err();
        vTaskDelay(pdMS_TO_TICKS(1500));
        UI_IdleScreen();
        break;

    default:
        break;
    }
}

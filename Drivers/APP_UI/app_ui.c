#include "app_ui.h"
#include "OLED.h"
#include "door_access.h"   /* PASSWORD_LEN */

/* ============================================================
 * OLED UI 辅助函数
 * ============================================================ */

void UI_HexByte(char *buf, uint8_t val)
{
    uint8_t hi = val >> 4, lo = val & 0x0F;
    buf[0] = (hi < 10) ? ('0' + hi) : ('A' + hi - 10);
    buf[1] = (lo < 10) ? ('0' + lo) : ('A' + lo - 10);
}

void UI_ShowCardID(uint16_t y, uint8_t *id)
{
    char str[12];
    uint8_t i, p = 0;
    for (i = 0; i < 4; i++) {
        UI_HexByte(&str[p], id[i]);
        p += 2;
        if (i < 3) str[p++] = ' ';
    }
    str[p] = 0;
    OLED_ShowCenter(y, str);
}

void UI_IdleScreen(void)
{
    OLED_Clear();
    OLED_ShowCenter(0,  "DOOR LOCK");
    OLED_ShowStr(16, 16, "#=PWD *=SET");
    OLED_ShowStr(16, 32, "AD/BD/CD/DD");
    OLED_Refresh();
}

void UI_ShowMessage(const char *line1, const char *line2)
{
    OLED_Clear();
    OLED_ShowCenter(16, line1);
    if (line2) OLED_ShowCenter(32, line2);
    OLED_Refresh();
}

void UI_PasswordScreen(const char *title)
{
    OLED_Clear();
    OLED_ShowCenter(0, title);
    OLED_ShowCenter(24, "______");
    OLED_ShowStr(0, 48, "*Clear #OK");
    OLED_Refresh();
}

void UI_RedrawPwd(uint8_t pwd_len)
{
    uint8_t i;
    for (i = 0; i < PASSWORD_LEN; i++) {
        if (i < pwd_len)
            OLED_ShowChar(40 + i * 8, 24, '*');
        else
            OLED_ShowChar(40 + i * 8, 24, '_');
    }
    OLED_Refresh();
}

/* ============================================================
 * 屏幕休眠管理
 * ============================================================ */

static uint8_t  screen_off = 0;    /* 1 = 屏幕已关闭 */
static uint32_t idle_cnt  = 0;     /* 空闲计数器 (主任务每 10ms +1) */

void Screen_WakeUp(void)
{
    if (screen_off) {
        OLED_DisplayOn();
        screen_off = 0;
    }
    idle_cnt = 0;
}

void Screen_TickIdle(uint8_t is_idle)
{
    if (is_idle && !screen_off) {
        idle_cnt++;
        if (idle_cnt >= IDLE_TIMEOUT_TICK) {
            OLED_DisplayOff();
            screen_off = 1;
            idle_cnt = 0;
        }
    }
}

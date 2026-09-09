#include "door_access.h"
#include "rc522.h"
#include <stdio.h>
#include <string.h>
#include "servo.h"
#include "w25qxx.h"

/* W25Q128 storage layout at address 0:
 *   [0-1]   magic 0xA5A5  (valid marker)
 *   [2]     card_count     (number of cards)
 *   [3-6]   card[0]        (4 bytes)
 *   [7-10]  card[1]
 *   [11-14] card[2]
 *   [15-18] card[3]
 */
#define FLASH_WHITELIST_ADDR   0
#define FLASH_MAGIC            0xA5A5

static uint8_t card_whitelist[MAX_CARDS][4];
static uint8_t card_count = 0;

/* ============================================================
 * 密码管理 (W25Q128 sector 1, 6 位数字密码)
 * ============================================================ */
uint8_t g_password[PASSWORD_LEN];

void Password_Load(void)
{
    uint8_t i, invalid = 0;
    W25Q_Read(g_password, PASSWORD_ADDR, PASSWORD_LEN);
    for (i = 0; i < PASSWORD_LEN; i++) {
        if (g_password[i] < '0' || g_password[i] > '9') { invalid = 1; break; }
    }
    if (invalid) {
        const uint8_t def[6] = {'1','2','3','4','5','6'};
        for (i = 0; i < PASSWORD_LEN; i++) g_password[i] = def[i];
        W25Q_EraseSector(PASSWORD_ADDR);
        W25Q_Write(g_password, PASSWORD_ADDR, PASSWORD_LEN);
        printf("Password: reset to default 123456\r\n");
    }
    printf("Password: loaded from flash\r\n");
}

void Password_Save(void)
{
    W25Q_EraseSector(PASSWORD_ADDR);
    W25Q_Write(g_password, PASSWORD_ADDR, PASSWORD_LEN);
    printf("Password: saved to flash\r\n");
}

/* Working mode: NORMAL, ADD_CARD, DEL_CARD */
static volatile uint8_t door_mode = DOOR_MODE_NORMAL;

/* Event flag: set by scan logic, read+cleared by main loop */
static volatile uint8_t door_event = DOOR_EVENT_NONE;

/* Last scanned card ID (for OLED display) */
static uint8_t last_card[4];
static uint8_t last_card_valid = 0;

typedef enum {
    STATE_IDLE,
    STATE_PROCESSED,
    STATE_UNLOCKED,
    STATE_CLOSING
} door_state_t;

static door_state_t state = STATE_IDLE;
static uint8_t no_card_cnt = 0;
#define NO_CARD_THRESHOLD 3
static uint16_t close_cnt = 0;
#define CLOSE_DELAY_TICK 30

void Door_Unlock(void)
{
    printf("Match OK, unlock\r\n");
    Servo_SetAngle(90);
    Servo_SetState(1);   /* 同步全局门锁状态, 供 WiFi 模块轮询上报 */
    state = STATE_CLOSING;
    close_cnt = CLOSE_DELAY_TICK;
    door_event = DOOR_EVENT_GRANTED;
}

static void Door_Lock(void)
{
    printf("Card removed, lock\r\n");
    Servo_SetAngle(0);
    Servo_SetState(0);   /* 同步全局门锁状态, 供 WiFi 模块轮询上报 */
    door_event = DOOR_EVENT_LOCKED;
}

static int ID_Compare(uint8_t *id, uint8_t *saved)
{
    uint8_t i;
    for (i = 0; i < 4; i++) {
        if (id[i] != saved[i])
            return 1;
    }
    return 0;
}

static uint8_t Find_Card(uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3)
{
    uint8_t i;
    for (i = 0; i < card_count; i++) {
        if (card_whitelist[i][0] == id0 &&
            card_whitelist[i][1] == id1 &&
            card_whitelist[i][2] == id2 &&
            card_whitelist[i][3] == id3)
            return i;
    }
    return 0xFF;
}

u8 Door_Add_Card(uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3)
{
    if (card_count >= MAX_CARDS)
        return 0;
    if (Find_Card(id0, id1, id2, id3) != 0xFF)
        return 0;
    card_whitelist[card_count][0] = id0;
    card_whitelist[card_count][1] = id1;
    card_whitelist[card_count][2] = id2;
    card_whitelist[card_count][3] = id3;
    card_count++;
    Door_Save_Whitelist();
    return 1;
}

u8 Door_Delete_Card(uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3)
{
    uint8_t idx, i;
    idx = Find_Card(id0, id1, id2, id3);
    if (idx == 0xFF)
        return 0;
    for (i = idx; i < card_count - 1; i++) {
        card_whitelist[i][0] = card_whitelist[i + 1][0];
        card_whitelist[i][1] = card_whitelist[i + 1][1];
        card_whitelist[i][2] = card_whitelist[i + 1][2];
        card_whitelist[i][3] = card_whitelist[i + 1][3];
    }
    card_count--;
    Door_Save_Whitelist();
    return 1;
}

void Door_Save_Whitelist(void)
{
    uint8_t buf[3 + MAX_CARDS * 4];
    uint16_t len, i;

    buf[0] = (uint8_t)(FLASH_MAGIC >> 8);
    buf[1] = (uint8_t)FLASH_MAGIC;
    buf[2] = card_count;

    for (i = 0; i < card_count; i++) {
        buf[3 + i * 4 + 0] = card_whitelist[i][0];
        buf[3 + i * 4 + 1] = card_whitelist[i][1];
        buf[3 + i * 4 + 2] = card_whitelist[i][2];
        buf[3 + i * 4 + 3] = card_whitelist[i][3];
    }

    len = 3 + card_count * 4;

    W25Q_EraseSector(FLASH_WHITELIST_ADDR);
    W25Q_Write(buf, FLASH_WHITELIST_ADDR, len);
}

u8 Door_Load_Whitelist(void)
{
    uint8_t buf[3 + MAX_CARDS * 4];
    uint16_t magic;
    uint8_t i, count;

    W25Q_Read(buf, FLASH_WHITELIST_ADDR, 3 + MAX_CARDS * 4);

    magic = ((uint16_t)buf[0] << 8) | buf[1];
    if (magic != FLASH_MAGIC) {
        printf("Flash: no valid whitelist, empty\r\n");
        card_count = 0;
        return 0;
    }

    count = buf[2];
    if (count > MAX_CARDS)
        count = MAX_CARDS;

    for (i = 0; i < count; i++) {
        card_whitelist[i][0] = buf[3 + i * 4 + 0];
        card_whitelist[i][1] = buf[3 + i * 4 + 1];
        card_whitelist[i][2] = buf[3 + i * 4 + 2];
        card_whitelist[i][3] = buf[3 + i * 4 + 3];
    }
    card_count = count;
    printf("Flash: loaded %d card(s) from whitelist\r\n", count);
    return count;
}

void Door_Set_Mode(uint8_t mode)
{
    door_mode = mode;
    state = STATE_IDLE;
    no_card_cnt = 0;
}

uint8_t Door_Get_Mode(void)
{
    return door_mode;
}

uint8_t *Door_Get_Last_Card(void)
{
    if (!last_card_valid) return 0;
    return last_card;
}

uint8_t Door_Get_Event(void)
{
    uint8_t e = door_event;
    door_event = DOOR_EVENT_NONE;
    return e;
}

void Door_Access_Init(void)
{
    RFID_Init();
    Door_Load_Whitelist();
}

int Door_Access_Scan(void)
{
    uint8_t card_id[4];
    uint8_t status;
    uint8_t i;

    /* ---- UNLOCKED: wait for card removal ---- */
    if (state == STATE_UNLOCKED) {
        status = PcdRequest(PICC_REQIDL, card_id);
        if (status == MI_OK) {
            PcdHalt();
            no_card_cnt = 0;
            return DOOR_NO_CARD;
        }
        no_card_cnt++;
        if (no_card_cnt < NO_CARD_THRESHOLD)
            return DOOR_NO_CARD;
        no_card_cnt = 0;
        state = STATE_CLOSING;
        close_cnt = CLOSE_DELAY_TICK;
        printf("Card removed, lock in 3s\r\n");
        return DOOR_NO_CARD;
    }

    /* CLOSING: countdown */
    if (state == STATE_CLOSING) {
        if (close_cnt > 0) {
            close_cnt--;
            return DOOR_NO_CARD;
        }
        Door_Lock();
        state = STATE_IDLE;
        return DOOR_NO_CARD;
    }

    /* PROCESSED: wait for card removal */
    if (state == STATE_PROCESSED) {
        status = PcdRequest(PICC_REQIDL, card_id);
        if (status == MI_OK) {
            PcdHalt();
            no_card_cnt = 0;
            return DOOR_NO_CARD;
        }
        no_card_cnt++;
        if (no_card_cnt < NO_CARD_THRESHOLD) {
            return DOOR_NO_CARD;
        }
        no_card_cnt = 0;
        state = STATE_IDLE;
        return DOOR_NO_CARD;
    }

    /* ---- IDLE: look for card ---- */
    status = PcdRequest(PICC_REQIDL, card_id);
    if (status != MI_OK)
        return DOOR_NO_CARD;

    if (PcdAnticoll(card_id) != MI_OK)
        return DOOR_NO_CARD;

    /* Save for OLED display */
    for (i = 0; i < 4; i++) last_card[i] = card_id[i];
    last_card_valid = 1;

    printf("Card ID: %02x %02x %02x %02x\r\n",
           card_id[0], card_id[1], card_id[2], card_id[3]);

    /* ---- ADD_CARD mode: add this card to whitelist ---- */
    if (door_mode == DOOR_MODE_ADD_CARD) {
        if (Door_Add_Card(card_id[0], card_id[1], card_id[2], card_id[3])) {
            printf("Card added to whitelist\r\n");
            door_event = DOOR_EVENT_ADD_OK;
            PcdHalt();
            state = STATE_PROCESSED;
            door_mode = DOOR_MODE_NORMAL;
            return DOOR_GRANTED;
        } else {
            printf("Add card FAIL (full or duplicate)\r\n");
            door_event = DOOR_EVENT_ADD_FAIL;
            PcdHalt();
            state = STATE_PROCESSED;
            door_mode = DOOR_MODE_NORMAL;
            return DOOR_DENIED;
        }
    }

    /* ---- DEL_CARD mode: delete this card from whitelist ---- */
    if (door_mode == DOOR_MODE_DEL_CARD) {
        if (Door_Delete_Card(card_id[0], card_id[1], card_id[2], card_id[3])) {
            printf("Card deleted from whitelist\r\n");
            door_event = DOOR_EVENT_DEL_OK;
            PcdHalt();
            state = STATE_PROCESSED;
            door_mode = DOOR_MODE_NORMAL;
            return DOOR_GRANTED;
        } else {
            printf("Delete card FAIL (not found)\r\n");
            door_event = DOOR_EVENT_DEL_FAIL;
            PcdHalt();
            state = STATE_PROCESSED;
            door_mode = DOOR_MODE_NORMAL;
            return DOOR_DENIED;
        }
    }

    /* ---- NORMAL mode: whitelist check ---- */
    for (i = 0; i < card_count; i++) {
        if (ID_Compare(card_id, card_whitelist[i]) == 0) {
            Door_Unlock();
            PcdHalt();
            state = STATE_UNLOCKED;
            no_card_cnt = 0;
            return DOOR_GRANTED;
        }
    }
    printf("Denied: not in whitelist\r\n");
    door_event = DOOR_EVENT_DENIED;
    PcdHalt();
    state = STATE_PROCESSED;
    return DOOR_DENIED;
}

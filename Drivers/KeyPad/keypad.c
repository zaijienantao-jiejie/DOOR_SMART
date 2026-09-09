#include "keypad.h"

/* 4x4 keymap */
static const uint8_t KeyMap[4][4] =
{
  {'1', '2', '3', 'A'},   /* ROW0 = PF0 */
  {'4', '5', '6', 'B'},   /* ROW1 = PF1 */
  {'7', '8', '9', 'C'},   /* ROW2 = PF2 */
  {'*', '0', '#', 'D'},   /* ROW3 = PF3 */
};

static const uint16_t ROW_Pin[4] = {GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3};
static const uint16_t COL_Pin[4] = {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7};

#define KEYPAD_PORT   GPIOF

static void KeyPad_Delay(uint32_t n)
{
  while(n--) __NOP();
}

static void KeyPad_RowsHigh(void)
{
  GPIO_SetBits(KEYPAD_PORT, ROW_Pin[0] | ROW_Pin[1] | ROW_Pin[2] | ROW_Pin[3]);
}

/* Scan once, edge-detect: returns key only on press (not while held) */
uint8_t KeyPad_Scan(void)
{
  static uint8_t last_key = KEY_NONE;
  uint8_t  curr_key = KEY_NONE;
  int8_t   row, col;
  int8_t   hit_row = -1, hit_col = -1;

  for(row = 0; row < 4; row++)
  {
    KeyPad_RowsHigh();
    GPIO_ResetBits(KEYPAD_PORT, ROW_Pin[row]);
    KeyPad_Delay(500);

    for(col = 0; col < 4; col++)
    {
      if(GPIO_ReadInputDataBit(KEYPAD_PORT, COL_Pin[col]) == 0)
      {
        hit_row = row; hit_col = col;
        break;
      }
    }
    if(hit_row != -1) break;
  }
  KeyPad_RowsHigh();

  if(hit_row != -1)
  {
    KeyPad_Delay(5000);
    KeyPad_RowsHigh();
    GPIO_ResetBits(KEYPAD_PORT, ROW_Pin[hit_row]);
    KeyPad_Delay(500);
    if(GPIO_ReadInputDataBit(KEYPAD_PORT, COL_Pin[hit_col]) == 0)
      curr_key = KeyMap[hit_row][hit_col];
    KeyPad_RowsHigh();
  }

  if(curr_key != KEY_NONE && last_key == KEY_NONE)
  {
    last_key = curr_key;
    return curr_key;
  }

  if(curr_key == KEY_NONE)
    last_key = KEY_NONE;

  return KEY_NONE;
}

uint8_t KeyPad_IsAnyKeyPressed(void){
	uint8_t i;
	GPIO_ResetBits(KEYPAD_PORT,ROW_Pin[0]|ROW_Pin[1]|ROW_Pin[2]|ROW_Pin[3]);
	KeyPad_Delay(500);
	for(i=0;i<4;i++)
	{
		if(GPIO_ReadInputDataBit(KEYPAD_PORT,COL_Pin[i])==0){
			KeyPad_RowsHigh();
			return 1;
		}
	}
	KeyPad_RowsHigh();
	return 0;
}

/* Init: rows = push-pull output (high), cols = pull-up input */
void KeyPad_Init(void)
{
  GPIO_InitTypeDef gpio;
  uint8_t i;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);

  gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  for(i = 0; i < 4; i++)
  {
    gpio.GPIO_Pin = ROW_Pin[i];
    GPIO_Init(KEYPAD_PORT, &gpio);
  }
  KeyPad_RowsHigh();

  gpio.GPIO_Mode = GPIO_Mode_IPU;
  for(i = 0; i < 4; i++)
  {
    gpio.GPIO_Pin = COL_Pin[i];
    GPIO_Init(KEYPAD_PORT, &gpio);
  }
}

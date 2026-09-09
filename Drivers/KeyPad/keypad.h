#ifndef __KEYPAD_H
#define __KEYPAD_H

#include "stm32f10x.h"

/* ============================================================
 * 4x4 Matrix Keypad (row-column scan)
 *
 * Pins:
 *   ROW0~3 = PF0~PF3  (output, pull one row low at a time)
 *   COL0~3 = PF4~PF7  (input, pull-up, detect which column goes low)
 *
 * Layout:
 *       COL0 COL1 COL2 COL3
 * ROW0   1    2    3    A
 * ROW1   4    5    6    B
 * ROW2   7    8    9    C
 * ROW3   *    0    #    D
 * ============================================================ */

#define KEY_NONE   0xFF

void    KeyPad_Init(void);
uint8_t KeyPad_Scan(void);
uint8_t KeyPad_IsAnyKeyPressed(void);
#endif

#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY_SWITCH      9   // 切换候选字母键
#define KEY_BACKSPACE  10   // 退格键
#define KEY_CONFIRM    11   // 确认键
#define KEY_CANCEL     12   // 取消键

// ==== 菜单系统专用按键 ====
#define KEY_UP          13  
#define KEY_DOWN        14  
#define KEY_MENU_ENTER  15  
#define KEY_MENU_EXIT   16

uint8_t InputName(char *name, uint8_t maxLen);
uint8_t Key_Scan(void);
uint8_t InputPassword(char *pwd, uint8_t maxLen);
uint16_t InputNumber(uint8_t maxDigits, uint8_t x, uint8_t y);


void Bluetooth_Init(void);
void Bluetooth_Process(void);

#endif

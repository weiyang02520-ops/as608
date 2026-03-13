#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY_SWITCH      9   // 切换候选字母键
#define KEY_BACKSPACE  10   // 退格键
#define KEY_CONFIRM    11   // 确认键
#define KEY_CANCEL     12   // 取消键

#define KEY_DEL_SPEC    13   // 按ID删除
#define KEY_MOD_NAME    14   // 修改名字
#define KEY_ADD         15   // 增加录入
#define KEY_LIST        16   // 列表

uint8_t InputName(char *name, uint8_t maxLen);
uint8_t Key_Scan(void);
uint8_t InputPassword(char *pwd, uint8_t maxLen);
uint16_t InputNumber(uint8_t maxDigits, uint8_t x, uint8_t y);


void Bluetooth_Init(void);
void Bluetooth_Process(void);

#endif

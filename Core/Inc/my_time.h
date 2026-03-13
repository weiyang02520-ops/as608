/*
 * time.h
 *
 *  Created on: Mar 5, 2026
 *      Author: peng
 */

#ifndef __TIME_H
#define __TIME_H

#include <stdint.h>

extern uint16_t MYRTC_Time[6];

void MYRTC_ReadTime(void);
void MYRTC_SetTime(void);
void OLED_ShowTime(void);

#endif




/*
 * time.c
 *
 *  Created on: Mar 5, 2026
 *      Author: peng
 */

#include "my_time.h"
#include "rtc.h"  
#include "oled.h"


uint16_t MYRTC_Time[6] = {2025, 1, 1, 0, 0, 0}; 


void MYRTC_ReadTime(void)
{
    RTC_DateTypeDef sDate;
    RTC_TimeTypeDef sTime;
    
    //STM32 硬件规定：只要你调用 GetTime 读取了时间，硬件就会立刻把‘日期’寄存器锁死
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    MYRTC_Time[0] = 2000 + sDate.Year;
    MYRTC_Time[1] = sDate.Month;
    MYRTC_Time[2] = sDate.Date;
    MYRTC_Time[3] = sTime.Hours;
    MYRTC_Time[4] = sTime.Minutes;
    MYRTC_Time[5] = sTime.Seconds;
}


void MYRTC_SetTime(void)
{
    RTC_DateTypeDef sDate;
    RTC_TimeTypeDef sTime;

    sDate.Year = MYRTC_Time[0] - 2000;
    sDate.Month = MYRTC_Time[1];
    sDate.Date = MYRTC_Time[2];
    sDate.WeekDay = 0;

    sTime.Hours = MYRTC_Time[3];
    sTime.Minutes = MYRTC_Time[4];
    sTime.Seconds = MYRTC_Time[5];

    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}



void OLED_ShowTime(void)
{
    uint8_t x = 0, y = 6;
    MYRTC_ReadTime();


    OLED_ShowNum(x, y, MYRTC_Time[0], 4, 16);
    x += 32;
    OLED_ShowChar(x, y, '-');
    x += 8;

    if (MYRTC_Time[1] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[1], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[1], 2, 16);
    }
    x += 16; 
    OLED_ShowChar(x, y, '-');
    x += 8;


    if (MYRTC_Time[2] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[2], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[2], 2, 16);
    }
    x += 16;
    OLED_ShowChar(x, y, ' ');
    x += 8;

    if (MYRTC_Time[3] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[3], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[3], 2, 16);
    }
    x += 16;
    OLED_ShowChar(x, y, ':');
    x += 8;

    if (MYRTC_Time[4] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[4], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[4], 2, 16);
    }
}

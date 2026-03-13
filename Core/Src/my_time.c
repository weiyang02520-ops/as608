/*
 * time.c
 *
 *  Created on: Mar 5, 2026
 *      Author: peng
 */

#include "my_time.h"
#include "rtc.h"  // 包含 CubeMX 生成的 RTC 头文件
#include "oled.h"


uint16_t MYRTC_Time[6] = {2025, 1, 1, 0, 0, 0};  // 默认时间

// 从内部 RTC 读取时间到 MYRTC_Time 数组
void MYRTC_ReadTime(void)
{
    RTC_DateTypeDef sDate;
    RTC_TimeTypeDef sTime;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    MYRTC_Time[0] = 2000 + sDate.Year;   // HAL 库中 Year 是 0-99
    MYRTC_Time[1] = sDate.Month;
    MYRTC_Time[2] = sDate.Date;
    MYRTC_Time[3] = sTime.Hours;
    MYRTC_Time[4] = sTime.Minutes;
    MYRTC_Time[5] = sTime.Seconds;
}

// 将 MYRTC_Time 数组的值写入内部 RTC
void MYRTC_SetTime(void)
{
    RTC_DateTypeDef sDate;
    RTC_TimeTypeDef sTime;

    sDate.Year = MYRTC_Time[0] - 2000;   // 只需后两位
    sDate.Month = MYRTC_Time[1];
    sDate.Date = MYRTC_Time[2];
    sDate.WeekDay = 0;  // 自动计算

    sTime.Hours = MYRTC_Time[3];
    sTime.Minutes = MYRTC_Time[4];
    sTime.Seconds = MYRTC_Time[5];
//    sTime.TimeFormat = RTC_HOURFORMAT_24;
//    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
//    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}



// 修复后的时间显示：严丝合缝铺满一行128像素，绝对不越界！
void OLED_ShowTime(void)
{
    uint8_t x = 0, y = 6;
    MYRTC_ReadTime();

    // 1. 显示年 (4个字符，占32像素)
    OLED_ShowNum(x, y, MYRTC_Time[0], 4, 16);
    x += 32;
    OLED_ShowChar(x, y, '-');
    x += 8;

    // 2. 显示月 (2个字符，占16像素)
    if (MYRTC_Time[1] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[1], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[1], 2, 16);
    }
    x += 16;  // 无论如何，画完月份统一向右挪16像素
    OLED_ShowChar(x, y, '-');
    x += 8;

    // 3. 显示日 (2个字符，占16像素)
    if (MYRTC_Time[2] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[2], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[2], 2, 16);
    }
    x += 16;
    OLED_ShowChar(x, y, ' '); // 打印中间的空格
    x += 8;

    // 4. 显示时 (2个字符，占16像素)
    if (MYRTC_Time[3] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[3], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[3], 2, 16);
    }
    x += 16;
    OLED_ShowChar(x, y, ':');
    x += 8;

    // 5. 显示分 (2个字符，占16像素，刚好到达 x=128 边界！)
    if (MYRTC_Time[4] < 10) {
        OLED_ShowChar(x, y, '0');
        OLED_ShowNum(x + 8, y, MYRTC_Time[4], 1, 16);
    } else {
        OLED_ShowNum(x, y, MYRTC_Time[4], 2, 16);
    }
}

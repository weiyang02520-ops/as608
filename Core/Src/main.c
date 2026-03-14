/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "AS608.h"
#include "oled.h"
#include "key.h"
#include <string.h>
#include <stdio.h>


extern uint16_t MYRTC_Time[6];
extern void MYRTC_ReadTime(void);
extern void MYRTC_SetTime(void);
extern void OLED_ShowTime(void);

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AT24C32_ADDR    0x57
#define HAL_AT24C32_ADDR (AT24C32_ADDR << 1)
#define RECORD_COUNT_ADDR 0x0000
#define RECORD_DATA_START 0x0002
#define DURATION_START_ADDR 0x0E00
#define BT_RX_BUF_SIZE 64

typedef struct {
    uint16_t id;
    uint32_t timestamp;  // 打卡时间戳(秒)
    uint8_t type;        // 打卡类型 0:上班 1:下班
} __attribute__((packed)) Record_t;

// 计算两次打卡的时间差(秒) - 增强版本
uint32_t CalculateTimeDifference(uint32_t check_in, uint32_t check_out) {
    // 确保check_in <= check_out
    if(check_in > check_out) return 0;
    
    // 防止溢出，如果差超过24小时(86400秒)则认为异常返回0
    uint32_t diff = check_out - check_in;
    return (diff > 86400) ? 0 : diff;
}

// 获取当前时间戳(秒)
uint32_t GetCurrentTimestamp(void) {
    return HAL_RTCEx_GetTimeStamp(&hrtc);
}

// 保存总时长到EEPROM
void SaveTotalDuration(uint16_t id, uint32_t duration) {
    uint16_t addr = DURATION_START_ADDR + id * 4;
    uint8_t buf[4];
    
    buf[0] = (duration >> 24) & 0xFF;
    buf[1] = (duration >> 16) & 0xFF;
    buf[2] = (duration >> 8) & 0xFF;
    buf[3] = duration & 0xFF;
    
    HAL_I2C_Mem_Write(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, 4, 100);
    
    HAL_Delay(10);  // 关键延时，确保写入完成
  }

// 从EEPROM读取总时长
uint32_t ReadTotalDuration(uint16_t id) {
    uint16_t addr = DURATION_START_ADDR + id * 4;
    uint8_t buf[4];
    
    HAL_I2C_Mem_Read(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, 4, 100);
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

// 累计指纹ID的总工作时长(秒)
void AccumulateDuration(uint16_t id, uint32_t duration_seconds) {
    uint32_t current_total = 0;
    uint16_t addr = DURATION_START_ADDR + id * 4;
    
    // 读取现有的总时长(4字节)
    uint8_t buf[4];
    HAL_I2C_Mem_Read(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, 4, 100);
    current_total = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    
    // 累计新时长
    current_total += duration_seconds;
    
    // 写回EEPROM (分4次写入确保跨页安全)
    buf[0] = (current_total >> 24) & 0xFF;
    buf[1] = (current_total >> 16) & 0xFF;
    buf[2] = (current_total >> 8) & 0xFF;
    buf[3] = current_total & 0xFF;
    
    for(uint8_t i=0; i<4; i++) {
        AT24C32_WriteByte(addr+i, buf[i]);
        HAL_Delay(5);  // 关键延时，确保跨页写入可靠
    }
}
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
SysPara AS608Para;                // 指纹模块参数
uint16_t ValidN;                   // 模块内有效模板个数
uint16_t recordCount = 0;          // EEPROM记录数

uint8_t aRxBuffer[RXBUFFERSIZE];   // AS608 接收缓冲区
uint8_t RX_len;                     // AS608 接收字节计数

uint8_t bt_rx_buf[BT_RX_BUF_SIZE]; // 蓝牙接收缓冲区
uint8_t bt_rx_len = 0;
uint8_t bt_rx_complete = 0;
uint8_t bt_rx_byte;

extern UART_HandleTypeDef huart3;  // 蓝牙串口句柄
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Menu_ShowMain(void);
void Process_AddFR(void); 
void Process_DelFR(void);
void Process_ModName(void);
void Process_ListFR(void);
void Process_ScanFR(void);
void Bluetooth_Init(void);
void Bluetooth_Process(void);
void Send_All_Ranks_To_Bluetooth(void);


// EEPROM 相关函数（若不需要可注释）
uint8_t AT24C32_ReadByte(uint16_t addr);
void AT24C32_WriteRecord(uint16_t index, Record_t *rec);
Record_t AT24C32_ReadRecord(uint16_t index);
uint8_t SaveRecord(uint16_t id);
uint8_t IsSameDay(uint32_t t1, uint32_t t2);
uint8_t AT24C32_Check(void);
void Send_Duration_To_Bluetooth(uint16_t id);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ================== AS608 空闲中断接收处理 ==================
void UsartReceive_IDLE(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    RX_len = RXBUFFERSIZE - huart1.RxXferCount;
    HAL_UART_AbortReceive_IT(&huart1);
    HAL_UART_Receive_IT(&huart1, aRxBuffer, RXBUFFERSIZE);
  }
}

// ================== 录指纹函数（带名字） ==================
void Add_FR(char *name)
{
  uint8_t i = 0, ensure, processnum = 0;
  uint16_t ID;

  while (1)
  {
    switch (processnum)
    {
      case 0:
        i++;
        OLED_Clear();
        OLED_ShowCHinese(18,0,0); // 指
        OLED_ShowCHinese(36,0,1); // 纹
        OLED_ShowCHinese(54,0,2); // 打
        OLED_ShowCHinese(72,0,3); // 卡
        OLED_ShowCHinese(90,0,4); // 机
        OLED_ShowTime();
        OLED_ShowCHinese(36,3,5); // 请
        OLED_ShowCHinese(52,3,6); // 按
        OLED_ShowCHinese(68,3,7); // 手
        OLED_ShowCHinese(84,3,8); // 指


        ensure = GZ_GetImage();
        if (ensure == 0x00)
        {
          ensure = GZ_GenChar(CharBuffer1);
          if (ensure == 0x00)
          {
            i = 0;
            processnum = 1;
          }
        }
        break;
      case 1:
        i++;
        OLED_Clear();
        OLED_ShowCHinese(18,0,0); // 指
        OLED_ShowCHinese(36,0,1); // 纹
        OLED_ShowCHinese(54,0,2); // 打
        OLED_ShowCHinese(72,0,3); // 卡
        OLED_ShowCHinese(90,0,4); // 机
        OLED_ShowTime();
        OLED_ShowCHinese(52,3,5); // 请
        OLED_ShowCHinese(68,3,6); // 按

        ensure = GZ_GetImage();
        if (ensure == 0x00) {
          ensure = GZ_GenChar(CharBuffer2);
          if (ensure == 0x00) {
            i = 0;
            processnum = 2;
          }
        }
        break;
      case 2:
        OLED_Clear();
        OLED_ShowCHinese(18,0,0); // 指
        OLED_ShowCHinese(36,0,1); // 纹
        OLED_ShowCHinese(54,0,2); // 打
        OLED_ShowCHinese(72,0,3); // 卡
        OLED_ShowCHinese(90,0,4); // 机
        OLED_ShowTime();
        OLED_ShowCHinese(36,3,21); // 匹
        OLED_ShowCHinese(52,3,22); // 配
        OLED_ShowCHinese(68,3,11); // 成
        OLED_ShowCHinese(84,3,12); // 功
        ensure = GZ_Match();
        if (ensure == 0x00) processnum = 3;
        else {
          i = 0;
          processnum = 0;
        }
        HAL_Delay(1000);
        break;
      case 3:
        OLED_Clear();
        OLED_ShowCHinese(18,0,0); // 指
        OLED_ShowCHinese(36,0,1); // 纹
        OLED_ShowCHinese(54,0,2); // 打
        OLED_ShowCHinese(72,0,3); // 卡
        OLED_ShowCHinese(90,0,4); // 机
        OLED_ShowCHinese(52,3,5); // 请
        OLED_ShowCHinese(68,3,35); // 稍
        OLED_ShowCHinese(68,3,36); // 等
        OLED_ShowTime();

        ensure = GZ_RegModel();
        if (ensure == 0x00) processnum = 4;
        else processnum = 0;
        HAL_Delay(1000);
        break;
      case 4:
        OLED_Clear();
        OLED_ShowCHinese(18,0,0); // 指
        OLED_ShowCHinese(36,0,1); // 纹
        OLED_ShowCHinese(54,0,2); // 打
        OLED_ShowCHinese(72,0,3); // 卡
        OLED_ShowCHinese(90,0,4); // 机
        OLED_ShowCHinese(52,3,5); // 请
        OLED_ShowCHinese(68,3,35); // 稍
        OLED_ShowCHinese(68,3,36); // 等
        OLED_ShowTime();

        ID = ValidN + 1;
        // 1. 先把指纹特征存进模块
        ensure = GZ_StoreChar(CharBuffer2, ID);
        if (ensure == 0x00)
        {
            // 存储名字到记事本 (只支持前15个)
            if (ID <= 15)
            {
               // 【极其关键的延时】给模块足够的时间去把指纹存入Flash！
               HAL_Delay(300);

               uint8_t buf[32] = {0};
               uint8_t len = 0;
               while (name[len] != 0 && len < 31)
               {
                   buf[len] = name[len];
                    len++;
                }

                // 写记事本并获取结果
                uint8_t write_res = GZ_WriteNotepad(ID, buf);

                OLED_ShowCHinese(0,2,19); // 指
                OLED_ShowCHinese(16,2,20); // 纹
                OLED_ShowString(32, 2, (uint8_t*)"ID:");

                uint8_t digits = (ID >= 100) ? 3 : (ID >= 10 ? 2 : 1);
                OLED_ShowNum(60, 2, ID, digits, 16);


                if (write_res == 0x00)
                {
                	OLED_ShowCHinese(68,3,11); // 成
                	OLED_ShowCHinese(84,3,12); // 功
                } else
                {
                    OLED_ShowCHinese(52,3,23); // 错
                    OLED_ShowCHinese(68,3,24); // 误
                }

                HAL_Delay(2000);
             }

             OLED_Clear();
             OLED_ShowCHinese(36,3,9);  // 录
             OLED_ShowCHinese(52,3,10); // 入
             OLED_ShowCHinese(68,3,11); // 成
             OLED_ShowCHinese(84,3,12); // 功

             GZ_ValidTempleteNum(&ValidN);
             HAL_Delay(1000);
             OLED_Clear();
             return;
        }
        else
        {
            processnum = 0;
         }
         break;
      default: break;
    }






    HAL_Delay(800);




    if (i == 10)
    {
      OLED_ShowCHinese(36,3,13); // 录
      OLED_ShowCHinese(52,3,14); // 入
      OLED_ShowCHinese(68,3,15); // 超
      OLED_ShowCHinese(84,3,16); // 时
      HAL_Delay(1000);
      break;
    }



  }




}


// ================== 刷指纹函数 (防闪屏优化版) ==================
void press_FR(void)
{
  SearchResult seach;
  uint8_t ensure;

  // 【优化1】删掉这里的 OLED_Clear() 和静态汉字绘制！
  // 直接覆盖刷新时间，不闪屏！
  OLED_ShowTime();

  ensure = GZ_GetImage();
  if (ensure == 0x00) {
    ensure = GZ_GenChar(CharBuffer1);
    if (ensure == 0x00) {
      ensure = GZ_HighSpeedSearch(CharBuffer1, 0, 300, &seach);
      if (ensure == 0x00) {
        if (seach.mathscore > 60) {
          uint8_t work_type = SaveRecord(seach.pageID);
          


          char name[32] = {0};
          if (seach.pageID <= 15) {
            GZ_ReadNotepad(seach.pageID, (uint8_t*)name);
            name[31] = 0;
            if (name[0] == 0xFF || name[0] == 0x00) {
                strcpy(name, "No Name");
            }
          } else {
            strcpy(name, "Unknown");
          }

          OLED_Clear();
          OLED_ShowCHinese(18,0,0);
          OLED_ShowCHinese(36,0,1);
          OLED_ShowCHinese(54,0,2);
          OLED_ShowCHinese(72,0,3);
          OLED_ShowCHinese(90,0,4);

          OLED_ShowCHinese(0,2,33); // 名
          OLED_ShowCHinese(16,2,34); // 字
          OLED_ShowString(33, 2, (uint8_t*)name);

          OLED_ShowCHinese(0,4,21); // 匹
          OLED_ShowCHinese(16,4,22); // 配
          OLED_ShowString(32, 4, (uint8_t*)"ID:");

          uint8_t digits = (seach.pageID >= 100) ? 3 : (seach.pageID >= 10 ? 2 : 1);
          OLED_ShowNum(60, 4, seach.pageID, digits, 16);

          if (work_type == 0) {
              OLED_ShowString(90, 2, (uint8_t*)"Work");
          } else {
              OLED_ShowString(90, 2, (uint8_t*)"Off");
          }

          OLED_ShowTime();
          HAL_Delay(2000);
        } else {
          // 分数太低
          OLED_Clear();
          OLED_ShowCHinese(18,0,0);
          OLED_ShowCHinese(36,0,1);
          OLED_ShowCHinese(54,0,2);
          OLED_ShowCHinese(72,0,3);
          OLED_ShowCHinese(90,0,4);
          OLED_ShowTime();
          OLED_ShowCHinese(52,3,23); // 错
          OLED_ShowCHinese(68,3,24); // 误
          HAL_Delay(1000);
        }
      } else {
        // 没搜到
        OLED_Clear();
        OLED_ShowCHinese(18,0,0);
        OLED_ShowCHinese(36,0,1);
        OLED_ShowCHinese(54,0,2);
        OLED_ShowCHinese(72,0,3);
        OLED_ShowCHinese(90,0,4);
        OLED_ShowTime();
        OLED_ShowCHinese(52,3,23); // 错
        OLED_ShowCHinese(68,3,24); // 误
        HAL_Delay(1000);
      }

      // 【优化2】指纹处理完（无论是成功还是失败），强制恢复主界面！
      Menu_ShowMain();
    }
  }
}
// ================== 显示主界面 ==================
void Menu_ShowMain(void)
{
  OLED_Clear();
  OLED_ShowCHinese(18, 0, 0);
  OLED_ShowCHinese(36, 0, 1);
  OLED_ShowCHinese(54, 0, 2);
  OLED_ShowCHinese(72, 0, 3);
  OLED_ShowCHinese(90, 0, 4);


  OLED_ShowTime();
}

// ================== 录入指纹（带名字输入）==================
void Process_AddFR(void)
{
  char pwd[7];
  if (InputPassword(pwd, 7)) {
    if (strcmp(pwd, "123456") == 0) {
      char name[32];
      if (InputName(name, 32)) Add_FR(name);
    } else {
      OLED_Clear();
      OLED_ShowString(20, 2, (uint8_t*)"Wrong PWD");
      HAL_Delay(1000);
    }
  }
  Menu_ShowMain();
}

// ================== 按ID删除指纹 ==================
void Process_DelFR(void)
{
  char pwd[7];
  if (InputPassword(pwd, 7)) {
    if (strcmp(pwd, "123456") == 0) {
      OLED_Clear();
      OLED_ShowString(0, 0, (uint8_t*)"Enter ID to del:");
      OLED_ShowString(0, 2, (uint8_t*)"(1-300)");
      uint16_t id = InputNumber(3, 0, 4);
      if (id != 0xFFFF && id >= 1 && id <= 300) {
        uint8_t res = GZ_DeletChar(id, 1);
        if (res == 0) {
          uint8_t zero[32] = {0};
          GZ_WriteNotepad(id, zero);
          GZ_ValidTempleteNum(&ValidN);
          OLED_Clear();
          OLED_ShowString(20, 2, (uint8_t*)"Delete OK");
          OLED_ShowString(20, 4, (uint8_t*)"Count:");
          OLED_ShowNum(80, 4, ValidN, 2, 16);
          HAL_Delay(2000);
        } else {
          OLED_Clear();
          OLED_ShowString(20, 2, (uint8_t*)"Delete Fail");
          OLED_ShowNum(80, 3, res, 2, 16);
          HAL_Delay(2000);
        }
      } else {
        OLED_Clear();
        OLED_ShowString(20, 2, (uint8_t*)"Invalid ID");
        HAL_Delay(1000);
      }
    } else {
      OLED_Clear();
      OLED_ShowString(20, 2, (uint8_t*)"Wrong PWD");
      HAL_Delay(1000);
    }
  }
  Menu_ShowMain();
}

// ================== 修改指纹名字 ==================
void Process_ModName(void)
{
  char pwd[7];
  if (InputPassword(pwd, 7)) {
    if (strcmp(pwd, "123456") == 0) {
      OLED_Clear();
      OLED_ShowString(0, 0, (uint8_t*)"Enter ID to mod:");
      OLED_ShowString(0, 2, (uint8_t*)"(1-15)");
      uint16_t id = InputNumber(3, 0, 4);
      if (id != 0xFFFF && id >= 1 && id <= 15) {
        uint8_t old_name[32] = {0};
        GZ_ReadNotepad(id, old_name);
        OLED_Clear();
        OLED_ShowString(0, 0, (uint8_t*)"Old name:");
        OLED_ShowString(0, 2, old_name);
        HAL_Delay(1000);
        char new_name[32];
        if (InputName(new_name, 32)) {
          uint8_t buf[32] = {0};
          uint8_t len = 0;
          while (new_name[len] != 0 && len < 31) {
            buf[len] = new_name[len];
            len++;
          }
          uint8_t write_res = GZ_WriteNotepad(id, buf);
          if (write_res == 0) {
            OLED_Clear();
            OLED_ShowString(20, 2, (uint8_t*)"Modify OK");
          } else {
            OLED_Clear();
            OLED_ShowString(20, 2, (uint8_t*)"Write Fail");
            OLED_ShowNum(80, 3, write_res, 2, 16);
          }
          HAL_Delay(1000);
        }
      } else {
        OLED_Clear();
        OLED_ShowString(20, 2, (uint8_t*)"ID out of range");
        HAL_Delay(1000);
      }
    } else {
      OLED_Clear();
      OLED_ShowString(20, 2, (uint8_t*)"Wrong PWD");
      HAL_Delay(1000);
    }
  }
  Menu_ShowMain();
}

// ================== 显示指纹列表（翻页）==================
void Process_ListFR(void)
{
  OLED_Clear();
  OLED_ShowString(0, 0, (uint8_t*)"Finger List");
  uint8_t page = 0;
  uint8_t totalPages = (ValidN + 2) / 3;
  if (totalPages == 0) totalPages = 1;
  while (1) {
    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t*)"Finger List");
    OLED_ShowNum(100, 0, page+1, 1, 16);
    OLED_ShowChar(108, 0, '/');
    OLED_ShowNum(116, 0, totalPages, 1, 16);
    for (uint8_t i = 0; i < 3; i++) {
      uint8_t id = page*3 + i + 1;
      if (id > ValidN) break;
      OLED_ShowNum(0, 2 + i*2, id, 2, 16);
      OLED_ShowChar(20, 2 + i*2, ':');
      char name[32] = {0};
      if (id <= 15) {
        GZ_ReadNotepad(id, (uint8_t*)name);
        name[31] = 0;
      } else strcpy(name, "<none>");
      OLED_ShowString(32, 2 + i*2, (uint8_t*)name);
    }
    uint8_t key = Key_Scan();
    if (key == KEY_CANCEL) break;
    else if (key == KEY_DEL_SPEC && page > 0) page--;
    else if (key == KEY_MOD_NAME && page < totalPages-1) page++;
    HAL_Delay(100);
  }
  Menu_ShowMain();
}

// ================== EEPROM 底层驱动 (HAL 库重写) ==================
uint8_t AT24C32_Check(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c1, HAL_AT24C32_ADDR, 3, 100) == HAL_OK) return 1;
    return 0;
}

uint8_t AT24C32_WriteByte(uint16_t addr, uint8_t data)
{
    HAL_StatusTypeDef res = HAL_I2C_Mem_Write(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, &data, 1, 100);
    HAL_Delay(5);
    return (res == HAL_OK) ? 0 : 1;
}

uint8_t AT24C32_ReadByte(uint16_t addr)
{
    uint8_t data = 0;
    HAL_I2C_Mem_Read(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, &data, 1, 100);
    return data;
}

void AT24C32_WriteRecord(uint16_t index, Record_t *rec)
{
    uint16_t addr = RECORD_DATA_START + index * sizeof(Record_t);
    uint8_t *p = (uint8_t*)rec;
    for (uint8_t i = 0; i < sizeof(Record_t); i++) {
        AT24C32_WriteByte(addr + i, p[i]);
    }
}

Record_t AT24C32_ReadRecord(uint16_t index)
{
    Record_t rec = {0};
    uint16_t addr = RECORD_DATA_START + index * sizeof(Record_t);
    HAL_I2C_Mem_Read(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)&rec, sizeof(Record_t), 100);
    return rec;
}

// ================== 极简版打卡逻辑 (摆脱 time.h 报错) ==================
uint8_t SaveRecord(uint16_t id)
{
    if (recordCount >= 580) return 0;

    MYRTC_ReadTime();
    uint32_t current_time = MYRTC_Time[3] * 60 + MYRTC_Time[4] + MYRTC_Time[5]; // 小时转分钟 + 分钟
    uint8_t determined_type = 0;
    uint32_t last_check_in = 0;

    // 查找该指纹ID的最后一条记录
    if (recordCount > 0) {
        for (int16_t i = recordCount - 1; i >= 0; i--) {
            Record_t rec = AT24C32_ReadRecord(i);
            if (rec.id == id) {
                if(rec.type == 0) { // 找到最后一条上班记录
                    last_check_in = rec.timestamp;
                }
                determined_type = (rec.type == 0) ? 1 : 0;
                break;
            }
        }
    }

    // 如果是下班打卡且找到对应的上班记录，则计算工作时间
    if(determined_type == 1 && last_check_in != 0) {
        uint32_t duration = CalculateTimeDifference(last_check_in, current_time);
        if(duration > 0) {
            uint32_t total_duration = ReadTotalDuration(id);
            total_duration += duration;
            SaveTotalDuration(id, total_duration);
            
            // 通过蓝牙发送更新
            Send_Duration_To_Bluetooth(id);
        }
    }

    Record_t rec;
    rec.id = id;
    rec.timestamp = current_time; // 使用RTC时间戳
    rec.type = determined_type;

    AT24C32_WriteRecord(recordCount, &rec);
    recordCount++;
    AT24C32_WriteByte(RECORD_COUNT_ADDR, recordCount & 0xFF);
    AT24C32_WriteByte(RECORD_COUNT_ADDR + 1, (recordCount >> 8) & 0xFF);

    return determined_type;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();

  /* USER CODE BEGIN 2 */
  Bluetooth_Init();

  if (HAL_UART_Receive_IT(&huart1, aRxBuffer, RXBUFFERSIZE) != HAL_OK) {
    Error_Handler();
  }
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

  OLED_Init();
  OLED_Clear();

  while (GZ_HandShake(&AS608Addr)) {
    HAL_Delay(1000);
  }
  HAL_Delay(100);
  GZ_ValidTempleteNum(&ValidN);
  GZ_ReadSysPara(&AS608Para);

  if (AT24C32_Check()) {
      uint8_t low = AT24C32_ReadByte(RECORD_COUNT_ADDR);
      uint8_t high = AT24C32_ReadByte(RECORD_COUNT_ADDR + 1);
      recordCount = (high << 8) | low;
      if (recordCount > 1000) recordCount = 0;
  } else {
      recordCount = 0;
  }

  Menu_ShowMain();


  

// --- 暴力测试开始 ---
uint32_t force_val = 60; // 强行存入 60 秒
uint8_t test_buf[4] = {0, 0, 0, 60}; 

// 直接用最原始的 HAL 库命令写一次
HAL_I2C_Mem_Write(&hi2c1, 0xAE, 0x0E00 + (5 * 4), I2C_MEMADD_SIZE_16BIT, test_buf, 4, 100);
HAL_Delay(20); // 必须等！

// --- 暴力测试结束 ---




  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    uint8_t key = Key_Scan();
    if (key != 0) {
      if (key == KEY_ADD) Process_AddFR();
      else if (key == KEY_LIST) Process_ListFR();
      else if (key == KEY_DEL_SPEC) Process_DelFR();
      else if (key == KEY_MOD_NAME) Process_ModName();
      else if (key == 8) Send_All_Ranks_To_Bluetooth();
    } else {
      press_FR();
    }

    Bluetooth_Process();
    HAL_Delay(50);
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

/* USER CODE BEGIN 4 */





/**
  * @brief  一键发送所有人的打卡排行榜到手机
  */
void Send_All_Ranks_To_Bluetooth(void)
{
    char buffer[50];
    uint32_t total_duration = 0;
    
    OLED_Clear();
    OLED_ShowString(0, 2, (uint8_t*)"Sending...");

    // 🚀【核心测试：强行发一个 99 号员工，时长 3600 秒】
    // 如果手机收到了这个，说明整个蓝牙通道、小程序解析全部是完美的！
    char test_msg[] = "USER:99,TOTAL:3600\n"; 
    HAL_UART_Transmit(&huart3, (uint8_t*)test_msg, strlen(test_msg), 100);
    HAL_Delay(100); // 给蓝牙一点喘息时间

    // 原有的遍历逻辑
    for(uint16_t id = 1; id <= ValidN; id++) 
    {
        uint16_t addr = 0x0E00 + (id * 4);
        HAL_I2C_Mem_Read(&hi2c1, 0xA0, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)&total_duration, 4, 100);
        
        if(total_duration > 0)
        {
            sprintf(buffer, "USER:%02d,TOTAL:%lu\n", id, total_duration);
            HAL_UART_Transmit(&huart3, (uint8_t*)buffer, strlen(buffer), 100);
            HAL_Delay(50); 
        }
    }
    
    OLED_Clear();
    OLED_ShowString(0, 2, (uint8_t*)"Send OK!");
    HAL_Delay(1000);
    Menu_ShowMain();
}















/**

  * @brief  发送指定用户的累计打卡时长到蓝牙串口

  * @param  id: 用户指纹ID

  */
void Send_Duration_To_Bluetooth(uint16_t id)
{
    char buffer[50];
    uint32_t total_duration = 0;

    // 第一步：直接调用你写好的读取工具，它会自动去 0xAE 地址拿数据，并拼好字节
    total_duration = ReadTotalDuration(id); 

    // 第二步：把拿到的数变成手机认得的字符串
    // 注意这里最后一定要带 \n，那是给手机的“句号”
    sprintf(buffer, "USER:%02d,TOTAL:%lu\n", id, total_duration);

    // 第三步：通过 huart3（蓝牙）发出去
    HAL_UART_Transmit(&huart3, (uint8_t*)buffer, strlen(buffer), 100);
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */


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
#define RECORD_COUNT_ADDR 0x0000// EEPROM地址0x0000存储记录数
#define RECORD_DATA_START 0x0002// 记录数据从地址0x0002开始存储，每条记录6字节（ID:2字节，时间戳:4字节）
#define DURATION_START_ADDR 0x0E00// EEPROM地址0x0E00开始存储累计时长，每个ID占4字节，支持最多512个ID的累计时长存储
#define BT_RX_BUF_SIZE 64

typedef struct {
    uint16_t id;
    uint32_t timestamp;  // 打卡时间戳(秒)
    uint8_t type;        // 打卡类型 0:上班 1:下班
} __attribute__((packed)) Record_t;

// 计算两次打卡的时间差
uint32_t CalculateTimeDifference(uint32_t check_in, uint32_t check_out) {

    if(check_in > check_out) return 0;
    
    // 防止溢出，如果差超过24小时(86400秒)则认为异常返回0
    uint32_t diff = check_out - check_in;
    return (diff > 86400) ? 0 : diff;
}

// 获取当前时间戳
uint32_t GetCurrentTimestamp(void) {
    return HAL_RTCEx_GetTimeStamp(&hrtc);
}

// 保存总时长到EEPROM
void SaveTotalDuration(uint16_t id, uint32_t duration) 
{
    uint16_t addr = DURATION_START_ADDR + id * 4;
    uint8_t buf[4];
    
    buf[0] = (duration >> 24) & 0xFF;
    buf[1] = (duration >> 16) & 0xFF;
    buf[2] = (duration >> 8) & 0xFF;
    buf[3] = duration & 0xFF;
    
    HAL_I2C_Mem_Write(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, 4, 100);
    
    HAL_Delay(10); 
}

// 从EEPROM读取总时长
uint32_t ReadTotalDuration(uint16_t id) {
    uint16_t addr = DURATION_START_ADDR + id * 4;
    uint8_t buf[4];
    
    HAL_I2C_Mem_Read(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, 4, 100);
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

// 累计指纹ID的总工作时长
void AccumulateDuration(uint16_t id, uint32_t duration_seconds) {
    uint32_t current_total = 0;
    uint16_t addr = DURATION_START_ADDR + id * 4;
    
    // 读取现有的总时长
    uint8_t buf[4];
    HAL_I2C_Mem_Read(&hi2c1, HAL_AT24C32_ADDR, addr, I2C_MEMADD_SIZE_16BIT, buf, 4, 100);
    current_total = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    
    current_total += duration_seconds;
    
    buf[0] = (current_total >> 24) & 0xFF;
    buf[1] = (current_total >> 16) & 0xFF;
    buf[2] = (current_total >> 8) & 0xFF;
    buf[3] = current_total & 0xFF;
    
    for(uint8_t i=0; i<4; i++) {
        AT24C32_WriteByte(addr+i, buf[i]);
        HAL_Delay(5);  
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
void Process_ChangePwd(void);
void Process_EmptyFR(void);
void Bluetooth_Init(void);
void Bluetooth_Process(void);
void Send_All_Ranks_To_Bluetooth(void);
void UI_DrawMenu(void);
void InitAdminPassword(void);
void Process_ResetTime(void);

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

// AS608 空闲中断接收处理 
// 这个函数在 USART1 的中断服务程序里被调用
// 只要 USART1 收到数据并触发空闲中断，就会调用
// 这个函数的设计原则是：一旦 USART1 收到数据并触发空闲中断，就立刻处理接收的数据，确保 AS608 的通信不会因为异常数据而死锁
// 这个函数的核心是：先清除空闲中断标志，再读取接收长度，最后重置接收状态，准备下一次接收
void UsartReceive_IDLE(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    // 清除空闲中断标志
    // 关键：必须先清除标志，再读取接收长度，否则可能会死锁！
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    RX_len = RXBUFFERSIZE - huart1.RxXferCount;

    HAL_UART_AbortReceive_IT(&huart1);

    HAL_UART_Receive_IT(&huart1, aRxBuffer, RXBUFFERSIZE);
  }
}

//  录指纹函数
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
            // 存储名字到记事本 
            if (ID <= 15)
            {

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

//刷指纹函数
void press_FR(void)
{
  SearchResult seach;
  uint8_t ensure;

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
//              OLED_ShowString(90, 2, (uint8_t*)"Work");
              OLED_ShowCHinese(90,2,57); // 打
              OLED_ShowCHinese(106,2,58); // 卡
          } else {
//              OLED_ShowString(90, 2, (uint8_t*)"Off");
                OLED_ShowCHinese(90,4,59); // 退
                OLED_ShowCHinese(106,4,58); // 卡
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


      Menu_ShowMain();
    }
  }
}

//显示主界面
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

//  菜单系统与密码管理 
uint8_t system_mode = 0; // 0: 待机打卡, 1: 菜单模式
uint8_t menu_cursor = 0;
uint8_t menu_offset = 0;

#define MENU_ITEM_COUNT 7
const char* menu_items[MENU_ITEM_COUNT] = {
    "1.Add User",
    "2.Del User",
    "3.Mod Name",
    "4.View List",
    "5.Change PWD",
    "6.Del All FR",
    "7.Reset Time"
};

char admin_pwd[7] = "123456";

// 初始化密码 
void InitAdminPassword(void) {
    uint8_t is_valid = 1;
    
    for(int i = 0; i < 6; i++) {
        uint8_t c = AT24C32_ReadByte(0x0100 + i);
        if (c < '0' || c > '9') {
            is_valid = 0; 
            break;
        }
    }

    if (!is_valid) {
        strcpy(admin_pwd, "123456");
        for(int i = 0; i < 6; i++) {
            AT24C32_WriteByte(0x0100 + i, admin_pwd[i]);
        }
    } else {

        for(int i = 0; i < 6; i++) {
            admin_pwd[i] = AT24C32_ReadByte(0x0100 + i);
        }
    }
    admin_pwd[6] = '\0';
}


// 菜单 
void UI_DrawMenu(void) {

    OLED_ShowCHinese(48, 0, 55); //菜
    OLED_ShowCHinese(80, 0, 56);//单

    for(uint8_t i = 0; i < 3; i++) {
        uint8_t idx = menu_offset + i;
        if(idx < MENU_ITEM_COUNT) {
            if(idx == menu_cursor) OLED_ShowString(0, 2+i*2, (uint8_t*)"->");
            else OLED_ShowString(0, 2+i*2, (uint8_t*)"  ");
            
            char buf[15];
            sprintf(buf, "%-14s", menu_items[idx]);
            OLED_ShowString(16, 2+i*2, (uint8_t*)buf);
        } else {
            OLED_ShowString(0, 2+i*2, (uint8_t*)"                ");
        }
    }
}



// 1. 录入指纹
void Process_AddFR(void) {
    char name[32];
    if (InputName(name, 32)) Add_FR(name);
}

// 2. 按ID删除指纹
void Process_DelFR(void) {
    OLED_Clear();

    OLED_ShowCHinese(0, 0, 51); //想
    OLED_ShowCHinese(16, 0, 52); //要
    OLED_ShowCHinese(32, 0, 53); //删
    OLED_ShowCHinese(48, 0, 54);//除
    OLED_ShowString(0, 2, (uint8_t*)"(1-300)");
    uint16_t id = InputNumber(3, 0, 4);
    if (id != 0xFFFF && id >= 1 && id <= 300) {
        uint8_t res = GZ_DeletChar(id, 1);
        if (res == 0) {
            uint8_t zero[32] = {0};
            GZ_WriteNotepad(id, zero);
            GZ_ValidTempleteNum(&ValidN);
            OLED_Clear();
            OLED_ShowCHinese(16, 2, 11); // 成
            OLED_ShowCHinese(64, 2, 12); // 功
            HAL_Delay(1500);
        }
    }
}

// 3. 修改指纹名字
void Process_ModName(void) {
    OLED_Clear();


    OLED_ShowCHinese(0, 0, 37); //输
    OLED_ShowCHinese(16, 0, 38); //入
    OLED_ShowCHinese(32, 0, 33); //名
    OLED_ShowCHinese(48, 0, 34);//字
    OLED_ShowCHinese(64, 0, 48); //修
    OLED_ShowCHinese(80, 0, 49);//改
    OLED_ShowString(0, 2, (uint8_t*)"(1-15)");
    uint16_t id = InputNumber(3, 0, 4);
    if (id != 0xFFFF && id >= 1 && id <= 15) {
        uint8_t old_name[32] = {0};
        GZ_ReadNotepad(id, old_name);
        OLED_Clear();

        OLED_ShowCHinese(16, 0, 50); //旧
        OLED_ShowCHinese(32, 0, 33); //名
        OLED_ShowCHinese(48, 0, 34);//字
        OLED_ShowString(0, 2, old_name);
        HAL_Delay(1000);
        char new_name[32];
        if (InputName(new_name, 32)) {
            uint8_t buf[32] = {0};
            uint8_t len = 0;
            while (new_name[len] != 0 && len < 31) { buf[len] = new_name[len]; len++; }
            if (GZ_WriteNotepad(id, buf) == 0) {
                OLED_Clear();

                OLED_ShowCHinese(16, 2, 11); // 成
                OLED_ShowCHinese(64, 2, 12); // 功
                HAL_Delay(1500);
            }
        }
    }
}

// 4. 显示指纹列表
void Process_ListFR(void) {
    OLED_Clear();
    uint8_t page = 0;
    uint8_t totalPages = (ValidN + 2) / 3;
    if (totalPages == 0) totalPages = 1;
    while (1) {
        OLED_ShowCHinese(50, 0, 0); //指
        OLED_ShowCHinese(66, 0, 1); //纹
        OLED_ShowNum(100, 0, page+1, 1, 16);
        OLED_ShowChar(108, 0, '/');
        OLED_ShowNum(116, 0, totalPages, 1, 16);
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t id = page*3 + i + 1;
            if (id > ValidN) {
                OLED_ShowString(0, 2 + i*2, (uint8_t*)"                "); // 补空行
                continue;
            }
            OLED_ShowNum(0, 2 + i*2, id, 2, 16);
            OLED_ShowChar(20, 2 + i*2, ':');
            char name[32] = {0};
            if (id <= 15) { GZ_ReadNotepad(id, (uint8_t*)name); name[31] = 0; }
            else strcpy(name, "<none>");
            char buf[12];
            sprintf(buf, "%-11s", name);
            OLED_ShowString(32, 2 + i*2, (uint8_t*)buf);
        }
        uint8_t key = Key_Scan();
        if (key == KEY_CANCEL) break;
        else if (key == KEY_UP && page > 0) page--;                 // 13 翻上一页
        else if (key == KEY_DOWN && page < totalPages-1) page++;    // 14 翻下一页
        HAL_Delay(100);
    }
}

// 5.修改管理员密码
void Process_ChangePwd(void) {
    char new_pwd[7];
    OLED_Clear();
    OLED_ShowCHinese(0, 0, 45); //新
    OLED_ShowCHinese(16, 0, 46); //密
    OLED_ShowCHinese(32, 0, 47); //码
    if (InputPassword(new_pwd, 7)) {
        for(int i = 0; i < 6; i++) {
            AT24C32_WriteByte(0x0100 + i, new_pwd[i]);
            admin_pwd[i] = new_pwd[i];
        }
        OLED_Clear();
        OLED_ShowCHinese(16, 2, 11); // 成
        OLED_ShowCHinese(64, 2, 12); // 功
        HAL_Delay(1500);
    }
}

// 6.清空所有数据
void Process_EmptyFR(void) {
    OLED_Clear();
    OLED_ShowCHinese(0, 0, 39); //是
    OLED_ShowCHinese(16, 0, 40); //否
    OLED_ShowCHinese(32, 0, 41); //清
    OLED_ShowCHinese(48, 0, 42);//除
    OLED_ShowCHinese(64, 0, 0); //指
    OLED_ShowCHinese(80, 0, 1); //纹
    OLED_ShowString(0, 2, (uint8_t*)"15:");
    OLED_ShowCHinese(40, 2, 39); //是
    OLED_ShowString(0, 4, (uint8_t*)"16:");
    OLED_ShowCHinese(40, 4, 40); //否
    while(1) {
        uint8_t key = Key_Scan();
        if (key == KEY_MENU_ENTER) { // 15
            OLED_Clear();
            OLED_ShowCHinese(16, 2, 35); // 稍
            OLED_ShowCHinese(64, 2, 36); // 等
            GZ_Empty();
            ValidN = 0;
            recordCount = 0;
            AT24C32_WriteByte(0x0000, 0); 
            AT24C32_WriteByte(0x0001, 0);
            OLED_Clear();
            OLED_ShowCHinese(16, 2, 11); // 成
            OLED_ShowCHinese(64, 2, 12); // 功
            HAL_Delay(1500);
            break;
        } else if (key == KEY_MENU_EXIT) { // 16
            break;
        }
        HAL_Delay(50);
    }
}

// 7.每周重置 
void Process_ResetTime(void) {
    OLED_Clear();

    OLED_ShowCHinese(0, 0, 39); //是
    OLED_ShowCHinese(16, 0, 40); //否
    OLED_ShowCHinese(32, 0, 41); //清
    OLED_ShowCHinese(48, 0, 42);//除
    OLED_ShowCHinese(64, 0, 43); //时
    OLED_ShowCHinese(80, 0, 44); //间
    OLED_ShowString(0, 2, (uint8_t*)"15:");
    OLED_ShowCHinese(40, 2, 39); //是
    OLED_ShowString(0, 4, (uint8_t*)"16:");
    OLED_ShowCHinese(40, 4, 40); //否

    while(1) {
        uint8_t key = Key_Scan();
        if (key == KEY_MENU_ENTER) { // 按下了 15 确认键
            OLED_Clear();

            OLED_ShowCHinese(16, 2, 35); // 稍
            OLED_ShowCHinese(64, 2, 36); // 等

            for (uint16_t i = 1; i <= 300; i++) {
                SaveTotalDuration(i, 0); 
            }
            

            recordCount = 0;
            AT24C32_WriteByte(0x0000, 0); 
            AT24C32_WriteByte(0x0001, 0);
            
            OLED_Clear();
            OLED_ShowCHinese(16, 2, 11); // 成
            OLED_ShowCHinese(64, 2, 12); // 功

            HAL_Delay(1500);
            break;
        } else if (key == KEY_MENU_EXIT) { // 按下了 16 取消键
            break;
        }
        HAL_Delay(50);
    }
}


//  EEPROM 底层驱动 (HAL 库重写) 
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
//打卡记录
uint8_t SaveRecord(uint16_t id)
{
    if (recordCount >= 580) return 0;

    MYRTC_ReadTime();
    uint32_t current_time = MYRTC_Time[3] * 3600 + MYRTC_Time[4] * 60 + MYRTC_Time[5];
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

    // 如果上次是上班打卡且时间不为0，计算时长并累计
    if(determined_type == 1 && last_check_in != 0) {
        uint32_t duration = CalculateTimeDifference(last_check_in, current_time);
        if(duration > 0) {
            uint32_t total_duration = ReadTotalDuration(id);
            total_duration += duration;
            SaveTotalDuration(id, total_duration);
            

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

  // 初始化密码缓存
  InitAdminPassword(); 
  
  Menu_ShowMain();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint8_t key = Key_Scan();


    //模式 0：待机打卡模式 

    if (system_mode == 0) 
    {
        if (key == KEY_MENU_EXIT) // 16 键：申请进菜单
        {
            char input_pwd[7];
            if (InputPassword(input_pwd, 7)) 
            {
                if (strcmp(input_pwd, admin_pwd) == 0) 
                {
                    system_mode = 1; 
                    menu_cursor = 0;
                    menu_offset = 0;
                    OLED_Clear();
                    UI_DrawMenu();   
                } 
                else 
                {
                    OLED_Clear();
                   
                    OLED_ShowCHinese(16, 2, 23);  
                    OLED_ShowCHinese(64, 3, 24); // 错误
                    HAL_Delay(1500);
                    Menu_ShowMain();
                }
            } 
            else 
            {
                Menu_ShowMain(); 
            }
        } 
        else if (key == 0) 
        {
            press_FR(); // 没按键时扫描指纹
        }
    }

    // 模式 1：后台管理菜单模式

    else if (system_mode == 1) 
    {
        if (key != 0) 
        {
            if (key == KEY_UP) // 13 上
            {
                if (menu_cursor > 0) menu_cursor--;
                if (menu_cursor < menu_offset) menu_offset = menu_cursor; 
                UI_DrawMenu();
            }
            else if (key == KEY_DOWN) // 14 下
            {
                if (menu_cursor < MENU_ITEM_COUNT - 1) menu_cursor++;
                if (menu_cursor > menu_offset + 2) menu_offset = menu_cursor - 2;
                UI_DrawMenu();
            }
            else if (key == KEY_MENU_ENTER) // 15 确认进入功能
            {
                OLED_Clear(); 
                switch(menu_cursor) {
                    case 0: Process_AddFR(); break;
                    case 1: Process_DelFR(); break;
                    case 2: Process_ModName(); break;
                    case 3: Process_ListFR(); break;
                    case 4: Process_ChangePwd(); break;
                    case 5: Process_EmptyFR(); break;
                    case 6: Process_ResetTime(); break;
                }
                OLED_Clear();
                UI_DrawMenu(); 
            }
            else if (key == KEY_MENU_EXIT) // 16 退出菜单
            {
                system_mode = 0; 
                Menu_ShowMain(); 
            }
        }
    }

    Bluetooth_Process();
    HAL_Delay(50);
  }
  /* USER CODE END WHILE */
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
















//  蓝牙发送所有用户的累计时长
void Send_All_Ranks_To_Bluetooth(void)
{
    char buffer[50];
    uint32_t total_duration = 0;
    
    OLED_Clear();
    OLED_ShowCHinese(16, 2, 35); // 稍
    OLED_ShowCHinese(64, 2, 36); // 等

    for(uint16_t id = 1; id <= ValidN; id++) 
    {


        total_duration = ReadTotalDuration(id);
        
  

        if(total_duration > 0 && total_duration != 0xFFFFFFFF)
        {
            sprintf(buffer, "USER:%02d,TOTAL:%lu\n", id, total_duration);
            HAL_UART_Transmit(&huart3, (uint8_t*)buffer, strlen(buffer), 100);
            HAL_Delay(50); 
        }
    }
    
    OLED_Clear();
    OLED_ShowCHinese(16, 2, 11); // 成
    OLED_ShowCHinese(64, 2, 12); // 功
    HAL_Delay(1000);
}













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
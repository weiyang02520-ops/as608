/*
 * key.c
 *
 *  Created on: Feb 28, 2026
 *      Author: peng
 */
#include "my_time.h"
#include <string.h>
#include <stdio.h>
#include "key.h"
#include "OLED.h"
#include "AS608.h"

#define BT_RX_BUF_SIZE 64
static uint8_t bt_rx_byte;
static uint8_t bt_rx_buf[BT_RX_BUF_SIZE];
static uint8_t bt_rx_len = 0;
static uint8_t bt_rx_complete = 0;

extern UART_HandleTypeDef huart3;
extern uint16_t MYRTC_Time[6];

extern uint16_t ValidN;
extern uint32_t AS608Addr;
extern void Add_FR(void);
extern void press_FR(void);
uint8_t res;








// 字母表：键1~8对应的字母
static const char keyLetters[8][5] = 
{
    {'A','B','C',0},
    {'D','E','F',0},
    {'G','H','I',0},
    {'J','K','L',0},
    {'M','N','O',0},
    {'P','Q','R','S',0},
    {'T','U','V',0},
    {'W','X','Y','Z',0}
};














// 显示名字（第2行）和光标（第3行）
static void ShowName(char *name, uint8_t pos)
{
    OLED_ShowString(0, 2, (uint8_t*)name);
    // 清除第3行
    for (uint8_t i = 0; i < 16; i++)
    {
        OLED_ShowChar(i*8, 3, ' ');
    }
    if (pos < 16)
    {
        OLED_ShowChar(pos*8, 3, '_');
    }
}













// 名字输入函数
uint8_t InputName(char *name, uint8_t maxLen)
{
    uint8_t pos = 0;// 当前的输入位置
    uint8_t currentKey = 0;// 当前按下的字母键
    uint8_t letterIndex = 0; // 当前按键对应的字母
    uint8_t key;
    uint8_t len=0;

    for(uint8_t i=0;i<maxLen;i++)
    {
    	name[i]=0;
    }

    OLED_Clear();

//    OLED_ShowString(0,0,(uint8_t*)"Input Name:");
    OLED_ShowCHinese(0,0,17); // 请
    OLED_ShowCHinese(16,0,37); // 输
    OLED_ShowCHinese(32,0,38); // 入
    OLED_ShowCHinese(48,0,33); // 名
    OLED_ShowCHinese(64,0,34); // 字

    ShowName(name,pos);

//    for (uint8_t i = 0; i < 16; i++)
//   {
//       OLED_ShowChar(i*8, 3, ' ');
//    }
//   if (pos < 16)
//    {
//        OLED_ShowChar(pos*8, 3, '_');
//    }

    while(1)
    {
    	key=Key_Scan();

    	if(key!=0)
    	{
    		if(key>=1 && key<=8)
    		{
    			if(currentKey==key)
    			{
    				// 同一个键再次按下：切换到下一个字母
    				letterIndex++;
    				if(keyLetters[key-1][letterIndex]==0)
    				{
    					letterIndex=0;
    				}

    			}
    			else
    			{
    				if(currentKey!=0 && pos<maxLen-1)
    				{
    					name[pos++]=keyLetters[currentKey][letterIndex];
    					name[pos]=0;
                        for (uint8_t i = len; i < 16; i++)
                            OLED_ShowChar(i*8, 3, ' ');
    					OLED_ShowString(0,2,(uint8_t*)name);

    				}

    				currentKey=key;
    				letterIndex=0;
    				len++;

    			}

    			if(pos<maxLen-1)
    			{
                    // 先清除第3行
                    for (uint8_t i = len; i < 16; i++)
                        OLED_ShowChar(i*8, 3, ' ');
                    // 显示候选字母
                    OLED_ShowChar(pos*8, 3, keyLetters[currentKey-1][letterIndex]);

    			}


    		}
    		else if(key==KEY_SWITCH)// 切换候选键
    		{
    				if(currentKey != 0)
    				{
    					letterIndex++;
    					if(keyLetters[currentKey-1][letterIndex] == 0)
    					{
    						 letterIndex = 0;
    					}
    					if (pos < maxLen-1)
    					{
    					     OLED_ShowChar(pos*8, 3, keyLetters[currentKey-1][letterIndex]);
    					}

    				}

    		}
    		else if(key == KEY_BACKSPACE)// 退格
    		{
                if (pos > 0)
                {
                    pos--;
                    name[pos] = 0;
                    currentKey = 0;
                    OLED_Clear();
                    OLED_ShowCHinese(0, 0, 37); //输
                    OLED_ShowCHinese(16, 0, 38); //入
                    OLED_ShowCHinese(32, 0, 33); //名
                    OLED_ShowCHinese(48, 0, 34);//字
                    ShowName(name, pos);

                }
    		}
            else if (key == KEY_CONFIRM) // 确认
            {
           
                if (currentKey != 0 && pos < maxLen-1)
                {
                    name[pos++] = keyLetters[currentKey-1][letterIndex];
                    name[pos] = 0;
                }
                if (pos > 0) // 至少输入了一个字符
                {
                    OLED_Clear();
                    return 1; // 成功
                }
            }
            else if (key == KEY_CANCEL) // 取消
            {
                OLED_Clear();
                return 0;
            }
        }

        HAL_Delay(50); // 改为 HAL 延时




    }

}























uint8_t Key_Scan(void)
{
    uint8_t r, c;
    uint16_t r_pins[4] = {R1_Pin, R2_Pin, R3_Pin, R4_Pin};
    uint16_t c_pins[4] = {C1_Pin, C2_Pin, C3_Pin, C4_Pin};

    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_RESET);

    HAL_Delay(10);

    if(HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)&&HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)&&HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)&&HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin))
    {
    		HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    	    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);

    	    return 0;
    }


    for(r=0;r<4;r++)
    {
		HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
	    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
	    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
	    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);


	    HAL_GPIO_WritePin(GPIOA,r_pins[r], GPIO_PIN_RESET);

    	HAL_Delay(10);

    	for(c=0;c<4;c++)
    	{
    		if(HAL_GPIO_ReadPin(GPIOA, c_pins[c])==GPIO_PIN_RESET)
    		{
    			while(HAL_GPIO_ReadPin(GPIOA, c_pins[c])== GPIO_PIN_RESET)
    			{}

    			HAL_Delay(10);

    			HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    		    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    		    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    		    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);

    		    return r * 4 + c + 1;
    		}

    	}

    }



	HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);


    return 0;

}









// 密码输入
uint8_t InputPassword(char *pwd, uint8_t maxLen)
{
    uint8_t pos = 0;
    uint8_t key;
    //uint8_t digit = 0;

    for (uint8_t i = 0; i < maxLen; i++) pwd[i] = 0;

    OLED_Clear();

    OLED_ShowCHinese(0, 0, 17); // 请
    OLED_ShowCHinese(16, 0, 37); // 输
    OLED_ShowCHinese(32, 0, 38); // 入
    OLED_ShowCHinese(48, 0, 46); // 密
    OLED_ShowCHinese(64, 0, 47); // 码

    OLED_ShowString(0, 2, (uint8_t*)"______");

    while (1)
    {
        key = Key_Scan();
        if (key != 0)
        {
            uint8_t digit = 0;

            // 数字键：键1-8 => 1-8, 键9 => 9, 键10 => 0
            // 键1~8  数字1~8
            if (key >= 1 && key <= 8)
            {
                digit = key + '0'; // 1 -> '1', 2 -> '2', ...
            }
            // 键9  数字9
            else if (key == 9)
            {
                digit = '9';
            }
            // 键10  数字0
            else if (key == 10)
            {
                digit = '0';
            }
            // 退格键
            else if (key == KEY_BACKSPACE)
            {
                if (pos > 0)
                {
                    pos--;
                    pwd[pos] = 0;
                    // 将对应位置恢复为下划线
                    OLED_ShowChar(0 + pos * 8, 2, '_');
                }
            }
            // 确认键
            else if (key == KEY_CONFIRM)
            {
                if (pos == maxLen - 1)
                {   // 密码位数满
                    OLED_Clear();
                    return 1;
                }
                // 否则忽略，继续输入
            }
            // 取消键
            else if (key == KEY_CANCEL)
            {
                OLED_Clear();
                return 0;
            }

            // 如果按下了数字键，且还有空位
            if (digit != 0xFF && pos < maxLen - 1)
            {
                pwd[pos] = digit;

                OLED_ShowChar(0 + pos * 8, 2, '*');
                pos++;
            }
        }
        HAL_Delay(50);
    }
}




uint16_t InputNumber(uint8_t maxDigits, uint8_t x, uint8_t y)
{
    uint8_t pos = 0;
    uint8_t digits[3] = {0};
    uint8_t key;
    uint16_t value = 0;

    // 清除显示区域
    for (uint8_t i = 0; i < maxDigits; i++)
        OLED_ShowChar(x + i*8, y, '_');

    while (1)
    {
        key = Key_Scan();
        if (key != 0)
        {
            uint8_t digit = 0xFF;
            // 数字键处理：键1-8 => 1-8, 键9 => 9, 键10 => 0
            if (key >= 1 && key <= 8) {
                digit = key;
            } else if (key == 9) {
                digit = 9;
            } else if (key == 10) {
                digit = 0;
            } else if (key == KEY_BACKSPACE) { // 退格
                if (pos > 0) {
                    pos--;
                    digits[pos] = 0;
                    OLED_ShowChar(x + pos*8, y, '_');
                }
            } else if (key == KEY_CONFIRM) { // 确认
                if (pos > 0) {
                    value = 0;
                    for (uint8_t i = 0; i < pos; i++) {
                        value = value * 10 + digits[i];
                    }
                    return value;
                }
            } else if (key == KEY_CANCEL) { // 取消
                return 0xFFFF;
            }

            if (digit != 0xFF && pos < maxDigits) {
                digits[pos] = digit;
                OLED_ShowChar(x + pos*8, y, digit + '0');
                pos++;
            }
        }
        HAL_Delay(50);
    }




}




void Bluetooth_Init(void)
{
    // 强制开启 USART3 的中断通道
    HAL_NVIC_SetPriority(USART3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);

    // 清除溢出错误标志（防死锁）
    __HAL_UART_CLEAR_OREFLAG(&huart3);

    // 启动第一次接收
    HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);
}

// 防死锁：万一被垃圾数据卡死，自动复活！
// 这个函数在 USART3 的中断服务程序里被调用
// 这个函数不需要在 main 循环里调用，因为它是 USART3 的中断回调函数
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart3);
        HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);
    }
}

// 串口接收完成回调
// 这个函数在 USART3 收到一个字节时被调用
// 它会把接收到的字节存入 bt_rx_buf，并设置 bt_rx_complete 标志，通知主循环有新数据了
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        if (bt_rx_len < BT_RX_BUF_SIZE - 1)
        {
            bt_rx_buf[bt_rx_len++] = bt_rx_byte;
        }

        // 兼容不同的手机助手，无论是发 \n 还是 \r 都能识别！
        if (bt_rx_byte == '\n' || bt_rx_byte == '\r')
        {
            bt_rx_buf[bt_rx_len] = '\0';
            bt_rx_complete = 1;
        }

        HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);  // 继续接收下一个字节
    }
}

// 主循环中调用
// 这个函数会检查 bt_rx_complete 标志，如果有新数据了，就处理它
// 处理完后会清空缓冲区，准备下一次接收void Bluetooth_Process(void)
void Bluetooth_Process(void)
{
    if (bt_rx_complete)
    {
        bt_rx_buf[bt_rx_len] = '\0';  // 添加字符串结束符，确保 strstr 正常工作

        // 1. 处理同步时间指令
        if (strncmp((char*)bt_rx_buf, "SET_TIME=", 9) == 0) {
            int year, month, day, hour, min, sec; 
            int ret = sscanf((char*)bt_rx_buf + 9, "%d-%d-%d %d:%d:%d",
                             &year, &month, &day, &hour, &min, &sec);
            if (ret == 6) {
                extern uint16_t MYRTC_Time[6];
                MYRTC_Time[0] = year;
                MYRTC_Time[1] = month;
                MYRTC_Time[2] = day;
                MYRTC_Time[3] = hour;
                MYRTC_Time[4] = min;
                MYRTC_Time[5] = sec;

                extern void MYRTC_SetTime(void);
                MYRTC_SetTime();

                OLED_Clear();
                OLED_ShowCHinese(16, 2, 11); // 成
                OLED_ShowCHinese(64, 2, 12); // 功
                HAL_Delay(1000);
            }
        }        
        // 2.处理索要排行榜指令
        else if (strstr((char*)bt_rx_buf, "GET_RANK") != NULL) 
        {
            // 收到手机呼叫，立刻启动“全量数据上报”大招
            // 这个函数会把所有用户的累计打卡时长发送给手机，格式是 "USER:XX,TOTAL:YYYYY\n"
            // 你可以在手机端解析这个数据，显示排行榜或者做其他处理
            // 这个函数内部已经处理了字节序和乱码问题，确保发送的数据是正确的累计时长，而不是乱码
            Send_All_Ranks_To_Bluetooth();
        }

        // 处理完一定要彻底清空，迎接下一次暗号
        memset(bt_rx_buf, 0, BT_RX_BUF_SIZE);
        bt_rx_len = 0;
        bt_rx_complete = 0;
    }
}


# AS608 Fingerprint Attendance Platform

> 一套从嵌入式固件到微信小程序控制台的端侧指纹考勤系统。项目围绕 STM32F103C8Tx、AS608、AT24C32、RTC、OLED、矩阵键盘和 BLE 串口模块构建，实现“指纹身份识别、本机离线管理、考勤数据持久化、移动端同步与排行榜展示”的完整闭环。

![Platform](https://img.shields.io/badge/platform-STM32F103C8Tx-03234B)
![Framework](https://img.shields.io/badge/framework-STM32CubeIDE-0B6E99)
![Module](https://img.shields.io/badge/module-AS608-fingerprint-2F855A)
![Client](https://img.shields.io/badge/client-WeChat_Mini_Program-07C160)
![Storage](https://img.shields.io/badge/storage-AT24C32_EEPROM-805AD5)
![Status](https://img.shields.io/badge/status-active_development-F6AD55)

## 项目简介

`as608` 不只是一个单片机指纹识别 Demo，而是一个完整的轻量级考勤终端原型。固件侧负责指纹采集、模板管理、离线打卡、时间维护和 EEPROM 数据持久化；小程序侧负责 BLE 连接、时间同步、排行榜请求和可视化展示。两端通过简单、可调试的文本协议通信，方便用串口助手、小程序或未来的上位机替换接入。

仓库同时开源配套微信小程序源码。小程序通过 BLE 连接蓝牙串口模块，可向硬件端同步手机时间、请求累计工时排行榜，并展示员工打卡排行。小程序当前未发布到微信平台，使用者可以自行导入微信开发者工具，替换自己的 AppID 后调试或发布。

## 设计目标

- **端侧自治**：断网、无服务器场景下仍可完成指纹打卡和本地数据保存。
- **低成本可复现**：使用 STM32F103C8Tx、AS608、AT24C32、OLED、矩阵键盘等常见模块搭建。
- **人机交互完整**：提供 OLED 菜单、管理员密码、姓名输入、用户列表和操作提示。
- **移动端协同**：通过微信小程序完成 BLE 连接、RTC 校时和排行榜读取。
- **协议简单透明**：硬件端和小程序端使用可读文本协议，便于抓包、调试和二次开发。
- **适合继续工程化**：保留清晰的模块边界，后续可扩展为更稳健的数据表、上位机或云端同步。

## 能力矩阵

| 层级 | 能力 | 当前实现 |
| --- | --- | --- |
| 生物识别 | 指纹采集与识别 | AS608 图像采集、特征生成、模板合成、存储、高速搜索 |
| 设备管理 | 用户生命周期 | 录入指纹、删除指定 ID、修改姓名、查看用户列表、清空指纹库 |
| 权限控制 | 管理员入口 | 6 位管理员密码、密码持久化、后台菜单保护 |
| 交互界面 | 本机操作面板 | OLED 中文提示、时间显示、菜单滚动、矩阵键盘输入 |
| 数据持久化 | 考勤记录 | AT24C32 保存记录数、用户打卡记录、累计工作时长 |
| 时间系统 | RTC 校时 | RTC 读取、设置、备份寄存器防重复初始化、蓝牙同步手机时间 |
| 通信协议 | UART / BLE 桥接 | USART1 对接 AS608，USART3 对接蓝牙串口，小程序通过 BLE 访问 |
| 移动端 | 微信小程序控制台 | BLE 扫描连接、通道发现、时间同步、排行榜请求、数据解析展示 |
| 数据展示 | 排行榜 | 按累计工时排序，格式化秒 / 分钟 / 小时 |

## 项目亮点

### 1. 固件与小程序成套开源

仓库同时包含 STM32 固件和微信小程序源码。硬件端可以独立运行，小程序端可以作为移动控制台使用。使用者不需要从零补客户端，只需导入 `wechat-miniprogram/`，替换自己的 AppID 后即可继续调试或发布。

### 2. 离线优先的考勤架构

核心考勤流程不依赖云端服务。指纹识别、打卡判断、记录写入和累计时长计算都在设备端完成，适合课程设计、宿舍/实验室小型管理、嵌入式系统综合实训等场景。

### 3. 可读、可抓包、可替换的通信协议

移动端和硬件端使用文本协议：

```text
SET_TIME=2026-07-15 09:30:00
GET_RANK
USER:01,TOTAL:3600
```

协议足够简单，可以直接用微信小程序、串口助手、手机 BLE 调试工具或 PC 上位机复用。

### 4. 完整的人机交互链路

项目覆盖“录入用户、维护用户、打卡识别、统计时长、移动端查看”的完整链路，而不仅是单个传感器驱动。OLED、键盘、指纹模块、EEPROM、RTC 和小程序都有明确职责。

### 5. 保留真实工程问题

README 没把原型阶段的问题藏起来：EEPROM 地址规划、跨天打卡、ID 复用、串口包解析都列为已知限制。这些点可以作为后续工程化优化方向，也能帮助阅读者理解嵌入式项目从 Demo 到产品的差距。

## 系统架构

```text
              +---------------------------+
              |      STM32F103C8Tx        |
              |                           |
              |  main loop / menu / logic |
              +------+----------+---------+
                     |          |
       USART1 57600  |          | USART3 9600
                     |          |
              +------+--+    +--+---------+
              | AS608   |    | Bluetooth  |
              | Finger  |    | UART       |
              +---------+    +------------+
                                  |
                                  | BLE
                           +------+------+
                           | WeChat Mini |
                           | Program     |
                           +-------------+
                     |
                     |
        +------------+-------------+
        |                          |
   +----+-----+              +-----+------+
   | OLED     |              | 4x4 Keypad |
   | Display  |              | PA0-PA7    |
   +----------+              +------------+
                     |
                     | I2C
              +------+------+
              | AT24C32     |
              | EEPROM      |
              +-------------+
```

## 端到端流程

### 用户录入

```text
管理员输入密码
  -> 后台菜单选择 Add User
  -> 键盘输入用户姓名
  -> AS608 两次采集指纹
  -> 生成并合成模板
  -> 写入指纹库 ID
  -> 将姓名写入 AS608 记事本页
```

### 日常打卡

```text
用户按下手指
  -> AS608 采集图像
  -> 生成特征
  -> 高速搜索指纹库
  -> 根据上一条记录判断上班 / 下班
  -> 写入 AT24C32 打卡记录
  -> 下班时累计工作时长
  -> OLED 展示姓名、ID、打卡类型和时间
```

### 移动端查看

```text
微信小程序扫描 BLE
  -> 连接蓝牙串口模块
  -> 发现 FFE0 服务和可写 / 可通知特征
  -> 发送 GET_RANK
  -> 固件返回 USER:XX,TOTAL:YYYY
  -> 小程序解析、排序并展示排行榜
```

## 技术实现要点

| 模块 | 实现点 |
| --- | --- |
| AS608 驱动 | 封装握手、采图、生成特征、合成模板、存储模板、高速搜索、读写记事本等指令 |
| USART1 接收 | 使用 UART 中断和空闲中断处理 AS608 应答，避免长时间阻塞等待 |
| USART3 蓝牙 | 使用中断逐字节接收手机 / 小程序指令，并在主循环中解析完整命令 |
| 矩阵键盘 | 4x4 行列扫描，支持数字、确认、取消、上下选择、退格等输入语义 |
| OLED UI | 中文字库显示、菜单绘制、状态提示、时间展示和结果反馈 |
| EEPROM 记录 | 基于 AT24C32 的 I2C 字节读写，保存记录数、考勤记录和累计时长 |
| RTC | 通过备份寄存器判断是否已初始化，避免每次复位都重置时间 |
| 小程序 BLE | 自动扫描目标蓝牙模块，发现服务和特征，处理通知数据分包并拼接解析 |
| 排行榜 | 根据 `TOTAL` 秒数排序，并格式化为秒、分钟、小时展示 |

## 技术栈

| 方向 | 技术 / 工具 |
| --- | --- |
| MCU | STM32F103C8Tx, ARM Cortex-M3 |
| 固件框架 | STM32CubeIDE, STM32CubeMX, STM32 HAL |
| 指纹识别 | AS608 UART command protocol |
| 本地存储 | AT24C32 EEPROM over I2C |
| 时间系统 | STM32 RTC, backup register |
| 本机交互 | OLED, 4x4 matrix keypad |
| 无线通信 | BLE serial module, WeChat Mini Program BLE API |
| 移动端 | WeChat Mini Program, WXML, WXSS, JavaScript |

## 硬件清单

| 硬件 | 用途 | 当前配置 |
| --- | --- | --- |
| STM32F103C8Tx | 主控芯片 | Cortex-M3 |
| AS608 | 指纹识别模块 | `USART1`, 57600 baud |
| 蓝牙串口模块 | 手机 / 上位机通信 | `USART3`, 9600 baud |
| AT24C32 | 外部 EEPROM | I2C 地址 `0x57` |
| OLED | 信息显示 | GPIO 模拟 SPI |
| 4x4 矩阵键盘 | 本机输入 | PA0-PA7 |
| ST-Link | 下载和调试 | SWD |

## 固件启动流程

```text
HAL_Init
  -> SystemClock_Config
  -> GPIO / USART1 / USART3 / I2C / RTC / TIM init
  -> Bluetooth_Init
  -> USART1 interrupt receive + IDLE interrupt enable
  -> OLED_Init
  -> AS608 handshake
  -> read valid template count
  -> read EEPROM record count
  -> init admin password
  -> enter main loop
```

主循环中同时处理：

- 键盘扫描
- 待机指纹识别
- 后台菜单操作
- 蓝牙命令解析
- OLED 状态刷新

## 功能菜单

管理员输入密码后可进入后台菜单：

```text
1. Add User      录入新指纹
2. Del User      删除指定 ID 指纹
3. Mod Name      修改用户姓名
4. View List     查看用户列表
5. Change PWD    修改管理员密码
6. Del All FR    清空全部指纹
7. Reset Time    重置累计时长和记录数
```

默认管理员密码：

```text
123456
```

## 蓝牙协议

硬件端和小程序端通过简单文本协议通信，便于串口助手调试，也便于后续替换为其他移动端或上位机。

### 同步 RTC 时间

手机或串口助手发送：

```text
SET_TIME=2026-07-15 09:30:00
```

设备解析后写入 RTC，并在 OLED 上显示成功提示。

### 请求累计时长排行

手机或串口助手发送：

```text
GET_RANK
```

设备返回格式：

```text
USER:01,TOTAL:3600
USER:02,TOTAL:5400
```

其中 `TOTAL` 单位为秒。

## 配套微信小程序

小程序源码位于：

```text
wechat-miniprogram/
```

小程序端负责：

- 扫描并连接 `BT24` / `DX` 蓝牙串口模块
- 自动发现 `FFE0` BLE 服务和可写 / 可通知特征
- 发送 `SET_TIME=...` 指令同步手机时间到打卡机
- 发送 `GET_RANK` 指令请求累计时长排行榜
- 解析 `USER:XX,TOTAL:YYYY` 数据并按累计时长排序展示

使用方式：

1. 打开微信开发者工具。
2. 导入 `wechat-miniprogram/` 目录。
3. 本地调试可使用 `touristappid`。
4. 如需发布，请在微信公众平台注册自己的小程序，并替换 `wechat-miniprogram/project.config.json` 中的 `appid`。

本仓库没有提交 `project.private.config.json`，该文件属于微信开发者工具生成的本机私有配置。

## 数据存储

当前项目使用 AT24C32 保存本地数据：

| 数据 | 说明 |
| --- | --- |
| 管理员密码 | 用于进入后台菜单 |
| 打卡记录数 | 保存当前记录数量 |
| 打卡记录 | 包含用户 ID、时间戳和打卡类型 |
| 累计时长 | 按用户 ID 保存累计工作秒数 |

注意：当前 EEPROM 地址规划仍有进一步整理空间，长期使用前建议先重构存储布局，避免记录区、密码区和累计时长区发生地址重叠。

## 快速开始

1. 克隆仓库：

```bash
git clone https://github.com/weiyang02520-ops/as608.git
```

2. 使用 STM32CubeIDE 导入项目。

3. 打开 `as608.ioc` 检查外设配置。

4. 构建 Debug 或 Release 固件。

5. 使用 ST-Link 下载到 STM32F103C8Tx 开发板。

6. 上电后连接 OLED、键盘、AS608、AT24C32 和蓝牙串口模块进行联调。

## 仓库结构

```text
.
├── Core/
│   ├── Inc/                    # 应用头文件和 CubeMX 生成头文件
│   ├── Src/                    # 主程序、指纹、按键、OLED、RTC、串口等代码
│   └── Startup/                # STM32 启动文件
├── Drivers/                    # STM32 HAL 和 CMSIS 驱动
├── wechat-miniprogram/          # 配套微信小程序源码
├── as608.ioc                   # STM32CubeMX 配置文件
├── STM32F103C8TX_FLASH.ld      # Flash 链接脚本
├── as608 Debug.launch          # CubeIDE 调试配置
└── README.md
```

## 关键源码

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 主流程、菜单、打卡记录、EEPROM 存储、蓝牙发送 |
| `Core/Src/AS608.c` | AS608 指纹模块命令封装 |
| `Core/Src/key.c` | 矩阵键盘扫描、密码输入、姓名输入、蓝牙接收回调 |
| `Core/Src/oled.c` | OLED 底层显示和字符输出 |
| `Core/Src/my_time.c` | RTC 读写和时间显示 |
| `Core/Src/usart.c` | USART1 / USART3 初始化 |
| `Core/Src/rtc.c` | RTC 初始化和备份寄存器处理 |
| `wechat-miniprogram/pages/index/index.js` | 小程序 BLE 连接、时间同步、排行榜请求和数据解析 |
| `wechat-miniprogram/pages/index/index.wxml` | 小程序排行榜界面 |

## 当前状态

项目已经具备完整原型能力，固件和小程序端都已开源。当前适合用于嵌入式综合设计、BLE 小程序联调、指纹模块驱动学习和本地数据持久化实验。

## Roadmap

| 阶段 | 方向 | 说明 |
| --- | --- | --- |
| v0.1 | 原型闭环 | 指纹录入、打卡、EEPROM 保存、OLED 菜单、小程序 BLE 排行榜 |
| v0.2 | 存储重构 | 重新规划 EEPROM 地址表，增加记录版本、容量边界和数据校验 |
| v0.3 | 协议升级 | 将文本协议升级为带长度、类型、校验和的二进制帧 |
| v0.4 | 考勤规则 | 支持跨天打卡、异常打卡、补卡标记和每日统计 |
| v0.5 | 移动端增强 | 小程序增加用户名称映射、历史记录、导出和发布说明 |
| v0.6 | 工程化拆分 | 拆分 storage、attendance、bluetooth、menu 等模块，降低 `main.c` 复杂度 |

## 已知限制

- 当前打卡时长主要按当天秒数计算，跨天打卡场景还需要补充处理。
- 指纹 ID 当前按有效模板数量递增，删除中间 ID 后再次新增时需要注意覆盖风险。
- AS608 应答包解析仍可优化，建议后续改成基于长度和校验和的二进制协议解析。
- EEPROM 地址规划建议在正式长期使用前重构。
- 本仓库暂未指定开源许可证；如果希望他人明确复用、修改和分发，建议补充 `LICENSE` 文件。

## 开发记录

| 日期 | 事项 |
| --- | --- |
| 2026-07-15 | 上传 GitHub，公开仓库，补充项目 README |
| 2026-07-15 | 升级 README，补充架构、协议、硬件和开发说明 |
| 2026-07-15 | 开源配套微信小程序源码，补充 BLE 小程序说明 |
| 2026-07-15 | 重构 README 为方案型文档，补充能力矩阵、端到端流程、技术实现要点和 Roadmap |

## License

No license has been selected yet. Please add a `LICENSE` file before using this repository as a formal open-source distribution.

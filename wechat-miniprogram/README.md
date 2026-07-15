# WeChat Mini Program

这是 AS608 指纹打卡机的配套微信小程序端。它通过微信小程序 BLE API 连接蓝牙串口模块，向硬件端发送时间同步和排行榜请求，并解析硬件返回的累计工时数据。

## 功能

- 扫描并连接 `BT24` / `DX` 蓝牙串口模块
- 自动发现 `FFE0` 服务和可写 / 可通知特征
- 向打卡机发送手机当前时间
- 向打卡机发送 `GET_RANK` 请求
- 解析 `USER:XX,TOTAL:YYYY` 数据并生成排行榜
- 将累计秒数格式化为秒、分钟或小时展示

## 和固件端的协议

小程序发送：

```text
SET_TIME=2026-07-15 09:30:00
GET_RANK
```

固件端返回：

```text
USER:01,TOTAL:3600
USER:02,TOTAL:5400
```

## 使用方式

1. 打开微信开发者工具。
2. 选择“导入项目”。
3. 项目目录选择本目录：`wechat-miniprogram/`。
4. 如果只是本地调试，可以使用 `touristappid`。
5. 如果要发布自己的小程序，请在微信公众平台注册小程序，并把 `project.config.json` 中的 `appid` 替换成自己的 AppID。
6. 真机调试时打开手机蓝牙，连接硬件端的蓝牙串口模块。

## 关键文件

| 文件 | 作用 |
| --- | --- |
| `pages/index/index.js` | BLE 连接、时间同步、排行榜请求和数据解析 |
| `pages/index/index.wxml` | 排行榜界面结构 |
| `pages/index/index.wxss` | 页面样式 |
| `app.json` | 小程序页面和窗口配置 |
| `project.config.json` | 微信开发者工具项目配置 |

## 注意

- 本目录没有包含 `project.private.config.json`，这是微信开发者工具生成的本机私有配置，不适合开源提交。
- 设备名匹配逻辑当前偏向 `BT24` / `DX` 蓝牙串口模块，如使用其他模块名称，需要修改 `pages/index/index.js` 中的匹配条件。
- 当前小程序未发布到微信平台，源码开放给使用者自行导入、调试和发布。

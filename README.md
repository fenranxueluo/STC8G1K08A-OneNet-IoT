# STC8G1K08A OneNet 物联网温湿度监测终端

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-STC8G1K08A-orange.svg)](http://www.stcmcudata.com/)
[![Protocol](https://img.shields.io/badge/protocol-MQTT-green.svg)](https://mqtt.org/)

Warning：本项目从未发布至 GitCode，如您发现请截图并保留证据

基于 STC8G1K08A 单片机的物联网温湿度监测系统，通过 ESP8266 WiFi 模块连接 OneNet 云平台，实现数据上传和远程控制。

## 功能特性

- 温湿度采集：DHT11 传感器实时监测环境温湿度
- 本地显示：OLED 128x64 显示屏实时显示数据
- 云端连接：通过 MQTT 协议上传数据到 OneNet 平台
- 远程控制：支持通过云端下发指令控制 LED
- 看门狗保护：系统异常自动复位，提高稳定性
- 错误处理：完善的错误检测和重试机制

## 硬件平台

| 组件 | 型号 | 说明 |
|------|------|------|
| 主控芯片 | STC8G1K08A | 1KB RAM / 8KB Flash |
| 温湿度传感器 | DHT11 | 单总线协议 |
| 显示模块 | OLED 128x64 | I2C 接口 |
| WiFi 模块 | ESP8266 | AT 指令控制 |
| 系统时钟 | 11.0592MHz | 便于串口波特率计算 |

### 引脚分配

| 功能 | 引脚 | 配置 |
|------|------|------|
| DHT11 数据 | P3.2 | 输入模式 |
| LED 控制 | P3.3 | 推挽输出 |
| I2C SCL | P5.5 | 准双向模式 |
| I2C SDA | P5.4 | 准双向模式 |
| UART TX | P3.1 | 默认 |
| UART RX | P3.0 | 默认 |

## 软件架构

```
┌─────────────────────────────────────────┐
│              应用层 (main.c)             │
│  - 系统初始化 / 任务调度 / 错误处理       │
├─────────────────────────────────────────┤
│              驱动层                      │
│  ├─ dht11.c    DHT11 温湿度驱动         │
│  ├─ oled.c     OLED 显示驱动            │
│  ├─ uart.c     ESP8266 WiFi 通信        │
│  └─ Delay.c    延时函数                 │
├─────────────────────────────────────────┤
│              硬件抽象层                  │
│  └─ STC8G.h    寄存器定义               │
└─────────────────────────────────────────┘
```

## 快速开始

### 1. 环境准备

- **Keil C51** 开发环境
- **STC-ISP** 烧录软件
- USB 转 TTL 串口模块

### 2. 配置参数

编辑 [uart.c](uart.c#L15-L21) 中的网络配置：

```c
#define NAME  "your_device_name"      // OneNet 设备名称
#define ID    "your_device_id"        // OneNet 设备 ID
#define TOKEN "your_token"            // OneNet 设备 Token
#define WIFI_SSID "your_wifi_ssid"    // WiFi 名称
#define WIFI_PASS "your_wifi_password" // WiFi 密码
```

### 3. 编译烧录

1. 使用 Keil 打开 `demo_onenet.uvproj`
2. 编译项目生成 HEX 文件
3. 使用 STC-ISP 烧录到单片机

### 4. 查看数据

- **本地显示**：OLED 屏幕实时显示温湿度
- **云端查看**：登录 [OneNet 平台](https://open.iot.10086.cn/) 查看数据流

## 代码优化亮点

### 内存优化

| 优化项 | 优化前 | 优化后 | 节省 |
|--------|--------|--------|------|
| 串口缓冲区 | 256B | 128B | 128B |
| 动态缓冲区 | 812B | 0B | 812B |
| **总计** | **1068B** | **128B** | **940B** |

- 移除 `sprintf`，改用轻量级字符串拼接
- 字符串常量存储在 Flash，节省 RAM

### 可靠性设计

- **看门狗机制**：2秒超时保护，防止死机
- **中断保护**：DHT11 时序关键段关中断
- **超时机制**：所有等待操作都有超时保护
- **重试机制**：网络连接和传感器读取支持重试

### 性能优化

- 整数运算替代浮点运算（放大10倍存储）
- 中断中仅设置标志位，无耗时操作
- 4秒采集周期，平衡实时性和功耗

## 项目结构

```
.
├── main.c              # 主程序入口
├── main.h              # 主程序头文件
├── dht11.c/h           # DHT11 驱动
├── oled.c/h            # OLED 驱动
├── uart.c/h            # WiFi 通信
├── Delay.c/h           # 延时函数
├── STC8G.h             # 芯片寄存器定义
├── STARTUP.A51         # 启动文件
├── demo_onenet.uvproj  # Keil 工程文件
└── README.md           # 项目说明
```

## OneNet 平台配置

### 1. 创建产品

1. 登录 [OneNet 平台](https://open.iot.10086.cn/)
2. 进入 "多协议接入" → "MQTT 协议"
3. 点击 "添加产品"
4. 填写产品信息，记录产品 ID

### 2. 添加设备

1. 进入产品详情页
2. 点击 "添加设备"
3. 填写设备名称，选择设备类型
4. 记录设备的 **Name**、**ID**、**Token**

### 3. 数据流模板

创建以下数据流：

| 数据流名称 | 单位 | 类型 |
|-----------|------|------|
| temp | °C | 浮点数 |
| hum | % | 浮点数 |

### 4. 下发命令

支持通过以下 Topic 下发控制命令：

```
$sys/{ID}/{NAME}/thing/property/set
```

命令格式：

```json
{
  "LED": 1  // 1-开灯, 0-关灯
}
```

## 通信协议

### 数据上传格式

```json
{
  "id": "123",
  "params": {
    "temp": {
      "value": 25.3
    },
    "hum": {
      "value": 60.5
    }
  }
}
```

### AT 指令流程

```
AT+RST                    # 复位模块
AT+CWMODE=1               # 设置 STA 模式
AT+CWJAP="SSID","PASS"    # 连接 WiFi
AT+MQTTUSERCFG=...        # 配置 MQTT 用户
AT+MQTTCONN=...           # 连接 MQTT 服务器
AT+MQTTSUB=...            # 订阅控制主题
AT+MQTTPUB=...            # 发布数据
```

## 故障排查

### OLED 无显示

- 检查 I2C 引脚连接（P5.4-SDA, P5.5-SCL）
- 确认 OLED 地址为 0x78
- 检查电源电压（3.3V/5V）

### WiFi 连接失败

- 确认 WiFi 名称和密码正确
- 检查 ESP8266 波特率是否为 115200
- 确认 WiFi 为 2.4GHz 频段

### 数据上传失败

- 检查 OneNet 设备三元组配置
- 确认设备已启用
- 查看串口调试信息

### DHT11 读取失败

- 检查数据线连接（P3.2）
- 确认上拉电阻（4.7K-10K）
- 检查传感器供电（3.3V-5V）

## 开发计划

- [ ] 添加 EEPROM 存储配置参数
- [ ] 实现低功耗模式
- [ ] 添加本地数据缓存（离线记录）
- [ ] 支持 OTA 固件升级
- [ ] 添加更多传感器支持（BMP280、BH1750 等）

## 许可证

本项目采用 [MIT 许可证](LICENSE) 开源。

## 致谢

- [STC 宏晶科技](http://www.stcmcudata.com/) - 单片机支持
- [OneNet 平台](https://open.iot.10086.cn/) - 云服务支持
- [ESP8266 AT 指令集](https://docs.espressif.com/) - WiFi 模块文档

## 联系方式

如有问题或建议，欢迎提交 Issue 或 Pull Request。

---

**注意**：使用前请修改 `uart.c` 中的 WiFi 和 OneNet 配置参数。

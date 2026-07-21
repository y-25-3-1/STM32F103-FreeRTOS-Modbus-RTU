# STM32F103 FreeRTOS Modbus RTU 从机

基于 STM32F103C8T6、FreeRTOS 和 MAX3485 的 Modbus RTU 工业通信终端示例。

项目通过 RS485 总线实现 Modbus RTU 从机通信，支持读取保持寄存器、写单个保持寄存器、标准异常响应、动态运行数据统计及 PC13 LED 远程控制。

## 1. 项目功能

- STM32F103C8T6 + FreeRTOS
- MAX3485 半双工 RS485 通信
- USART2：9600、8 数据位、无校验、1 停止位
- Modbus RTU 从机地址：`0x01`
- 支持功能码：
  - `0x03`：读取保持寄存器
  - `0x06`：写单个保持寄存器
- 支持标准异常响应：
  - `0x01`：非法功能码
  - `0x02`：非法数据地址
  - `0x03`：非法数据值
- Modbus CRC16 校验
- USART2 单字节中断接收
- 128 字节接收缓冲区
- 通过 5 ms 总线静默时间判断一帧结束
- FreeRTOS 独立任务周期更新动态寄存器
- 使用临界区保护共享寄存器
- 通过保持寄存器远程控制 PC13 板载 LED
- USART1 输出调试日志

## 2. 硬件环境

| 模块 | 配置 |
|---|---|
| MCU | STM32F103C8T6 |
| 系统时钟 | 72 MHz |
| RTOS | FreeRTOS / CMSIS-RTOS V1 |
| RS485 收发器 | MAX3485，3.3 V 供电 |
| 调试串口 | USART1，115200，8N1 |
| Modbus 串口 | USART2，9600，8N1 |
| 状态 LED | PC13，低电平点亮 |
| RS485 方向控制 | PB0 |

## 3. 接线

```text
STM32 PA2  -> MAX3485 RXD
STM32 PA3  -> MAX3485 TXD
STM32 PB0  -> MAX3485 EN
STM32 3.3V -> MAX3485 VCC
STM32 GND  -> MAX3485 GND

MAX3485 A   -> USB-RS485 A
MAX3485 B   -> USB-RS485 B
MAX3485 GND -> USB-RS485 GND
```

注意：

- MAX3485 使用 3.3 V 供电。
- STM32、MAX3485 和 USB-RS485 必须共地。
- A/B 接线端子必须压紧，接触不良会导致丢字节和误码。
- A/B 线尽量短，并尽量使用双绞线。

## 4. 保持寄存器映射

| 地址 | 名称 | 读写属性 | 含义 |
|---:|---|---|---|
| 0 | Uptime Seconds | 只读 | 系统运行秒数的低 16 位 |
| 1 | Valid Frame Count | 只读 | CRC 正确且发送给本机的报文数量 |
| 2 | CRC Error Count | 只读 | CRC 错误报文数量 |
| 3 | LED Control | 读写 | `0` 关闭 LED，`1` 点亮 LED |
| 4 | LED Actual State | 只读 | LED 实际状态 |
| 5 | Last Function Code | 只读 | 最近记录的 Modbus 功能码 |
| 6 | Exception Count | 只读 | 已生成的异常响应数量 |
| 7 | Device Status | 只读 | 设备综合状态位 |

### 设备状态寄存器

| 位 | 掩码 | 含义 |
|---:|---:|---|
| bit0 | `0x0001` | 设备正在运行 |
| bit1 | `0x0002` | LED 实际点亮 |
| bit2 | `0x0004` | 至少收到过一条有效帧 |
| bit3 | `0x0008` | 曾经出现 CRC 错误 |
| bit4 | `0x0010` | 曾经生成异常响应 |

状态位使用按位或组合，例如：

```text
0x0001 | 0x0004 = 0x0005
```

表示设备正在运行，并且已经收到过有效帧。

## 5. 软件结构

```text
Core/
├─ Inc/
│  ├─ modbus_crc.h
│  ├─ modbus_slave.h
│  └─ rs485.h
└─ Src/
   ├─ modbus_crc.c
   ├─ modbus_slave.c
   ├─ rs485.c
   └─ freertos.c
```

### `rs485.c`

负责：

- MAX3485 收发方向切换
- USART2 中断接收
- 接收缓冲区管理
- 5 ms 静默时间判帧
- RS485 数据发送

### `modbus_crc.c`

负责：

- Modbus CRC16 计算
- 接收报文 CRC 校验

### `modbus_slave.c`

负责：

- 保持寄存器映射
- 功能码 `0x03`
- 功能码 `0x06`
- 标准异常响应
- LED 控制
- 动态统计数据
- 设备状态位
- 共享寄存器临界区保护

### `freertos.c`

负责：

- `SystemTask`：读取 RS485 帧、校验 CRC、处理 Modbus 请求并发送响应
- `DeviceDataTask`：每秒更新运行时间、LED 实际状态和设备状态

## 6. 测试指令

### 读取 8 个保持寄存器

发送：

```text
01 03 00 00 00 08 44 0C
```

正常响应格式：

```text
01 03 10 [16 字节寄存器数据] [CRC 低字节] [CRC 高字节]
```

### 点亮 LED

发送：

```text
01 06 00 03 00 01 B8 0A
```

正常响应原样回显：

```text
01 06 00 03 00 01 B8 0A
```

### 关闭 LED

发送：

```text
01 06 00 03 00 00 79 CA
```

正常响应原样回显：

```text
01 06 00 03 00 00 79 CA
```

### CRC 错误测试

发送故意写错 CRC 的报文：

```text
01 03 00 00 00 08 44 0D
```

从机应丢弃该报文、不返回响应，并将 CRC 错误计数加 1。

## 7. 编译与运行

1. 使用 STM32CubeIDE 打开工程。
2. 确认芯片型号为 STM32F103C8T6。
3. 确认系统时钟为 72 MHz。
4. 编译工程。
5. 使用 ST-Link 下载程序。
6. USART1 连接串口助手，配置为 115200、8N1。
7. USB-RS485 连接 USART2 对应的 MAX3485 总线。
8. Modbus 主机配置为 9600、8N1、RTU、从机地址 1。

## 8. 运行日志示例

```text
[BOOT] Modbus functions 03/06 slave started
[CRC SELF TEST] PASS

[MODBUS RX] 01 03 00 00 00 08 44 0C
[MODBUS] CRC OK
[MODBUS TX] 01 03 10 ...
[MODBUS] TX OK
```

## 9. 项目结果

项目已完成以下验证：

- 功能码 `0x03` 正常读取
- 功能码 `0x06` 正常写入并控制 LED
- 异常码 `0x01`、`0x02`、`0x03` 正常返回
- CRC 错误报文能够被识别并丢弃
- 动态寄存器能够周期更新
- 连续 20 次自动轮询测试通过
- LED 控制值与实际状态保持一致
- A/B 接线端子压紧后通信稳定

## 10. 后续可扩展方向

- 增加看门狗
- 增加 UART 错误自动恢复
- 增加功能码 `0x10`
- 增加传感器数据映射
- 增加 Flash 参数保存
- 增加上位机监控界面

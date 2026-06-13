# PWMADC 当前引脚接线表

本文档记录当前 `PWMADC.ioc` 中已经启用、并且在项目中有实际功能的 STM32F103C8T6 引脚。下一位接线的人可以直接按这张表接线。

- 芯片型号：`STM32F103C8T6`
- 封装：`LQFP48`
- CubeMX 工程：`PWMADC.ioc`
- 最后更新：`2026-06-07`

## 引脚总表

| STM32 引脚 | 代码标签 | CubeMX 功能 | 面包板/模块接线 | 用途和注意事项 |
|---|---|---|---|---|
| `PA0` | - | `ADC1_IN0` | 采样电阻 `R_sense` 电压采样点 | 用于计算输出电流。输入必须限制在 `0~3.3V`。当前采样电阻按 `5.17Ω` 计算。 |
| `PA1` | - | `ADC2_IN1` | 输出端/负载端总电压采样点 | 用于测量采样电阻加负载的总电压。输入必须限制在 `0~3.3V`。 |
| `PA7` | - | `TIM1_CH1N` | 功率驱动芯片 `LIN/LI` 输入 | TIM1 互补 PWM 输出。与 `PA8` 成对使用。 |
| `PA8` | - | `TIM1_CH1` | 功率驱动芯片 `HIN/HI` 输入 | TIM1 主 PWM 输出。程序通过改变占空比调节输出电流。 |
| `PA9` | - | `USART1_TX` | USB-TTL 模块 `RXD` | STM32 串口发送脚，打印 ADC、PID、占空比等调试信息。 |
| `PA10` | - | `USART1_RX` | USB-TTL 模块 `TXD` | STM32 串口接收脚，用于接收 `SET=...`、`DUTY=...`、`KP=...`、`START`、`STOP` 等命令。 |
| `PA13` | - | `SYS_JTMS-SWDIO` | ST-Link `SWDIO` | 下载/调试数据线，不要接其他外设。 |
| `PA14` | - | `SYS_JTCK-SWCLK` | ST-Link `SWCLK` | 下载/调试时钟线，不要接其他外设。 |
| `PB6` | `UP` | `GPIO_Input`，上拉 | 加 1 按键输出端 | 按键公共端接 `GND`。松开为高电平，按下为低电平，设定电流加 `1mA`。 |
| `PB7` | `DOWN` | `GPIO_Input`，上拉 | 减 1 按键输出端 | 按键公共端接 `GND`。松开为高电平，按下为低电平，设定电流减 `1mA`。 |
| `PB8` | `ENC_SW` | `GPIO_Input`，上拉 | EC11 编码器按键 `C/SW` | 编码器下压按键输入。当前硬件上该脚曾不稳定，如暂时不用可不接。 |
| `PB9` | `BUZZER` | `GPIO_Output`，初始高电平 | 低电平触发蜂鸣器模块输入端 | 蜂鸣器为低电平触发：`PB9=Low` 响，`PB9=High` 不响。当前设定电流 `>=550mA` 时触发报警。 |
| `PB10` | - | `I2C2_SCL` | I2C LCD1602 模块 `SCL/SCK` | I2C2 时钟线，当前用于带 I2C 背包板的 LCD1602。 |
| `PB11` | - | `I2C2_SDA` | I2C LCD1602 模块 `SDA` | I2C2 数据线，当前用于带 I2C 背包板的 LCD1602。 |
| `PB12` | `ENC_A` | `GPIO_EXTI12`，上拉，双边沿中断 | EC11 编码器 `A` | 编码器 A 相输入，用于检测旋转。 |
| `PB13` | `ENC_B` | `GPIO_EXTI13`，上拉，双边沿中断 | EC11 编码器 `B` | 编码器 B 相输入，用于判断旋转方向。 |
| `PD0` | - | `RCC_OSC_IN` | 外部高速晶振输入 | 开发板上通常已接好，不要接面包板外设。 |
| `PD1` | - | `RCC_OSC_OUT` | 外部高速晶振输出 | 开发板上通常已接好，不要接面包板外设。 |

## 当前未用于外设的相关引脚

| STM32 引脚 | 当前状态 | 说明 |
|---|---|---|
| `PA2` | 未启用/释放 | 之前用于并口 LCD `D4`，现在换成 I2C LCD 后不再使用。 |
| `PA3` | 未启用/释放 | 之前用于并口 LCD `D5`，现在换成 I2C LCD 后不再使用。 |
| `PA4` | 未启用/释放 | 之前用于并口 LCD `D6`，现在换成 I2C LCD 后不再使用。 |
| `PA5` | 未启用/释放 | 之前用于并口 LCD `D7`，现在换成 I2C LCD 后不再使用。 |

## I2C LCD1602 接线

当前显示屏为背面带 4 针 I2C 转接板的 LCD1602。

```text
LCD GND     -> STM32 GND
LCD VCC     -> 5V 或 3.3V
LCD SDA     -> PB11 / I2C2_SDA
LCD SCL/SCK -> PB10 / I2C2_SCL
```

注意事项：

```text
外部 5V 电源 GND、STM32 GND、LCD GND 必须共地。
I2C2 当前速度为 100 kHz Standard Mode。
常见 I2C 地址为 0x27 或 0x3F，当前固件会自动尝试常见地址。
```

如果 LCD 不显示，优先检查：

```text
1. SDA/SCL 是否接反
2. LCD、STM32、外部电源是否共地
3. LCD 背包板是否有上拉电阻
4. VCC 用 5V 不稳定时，尝试 3.3V
5. LCD 对比度电位器是否调到能看见字符
```

## ADC 采样接线

```text
PA0 / ADC1_IN0 -> R_sense 采样电阻电压点
PA1 / ADC2_IN1 -> 采样电阻 + 负载串联后的总电压采样点
GND            -> 采样电路地
```

当前软件中的主要计算关系：

```text
R_sense = 5.17Ω
ADC1_true = ADC1_pin * 11 / 10
ADC2_true = ADC2_pin * 11
I_adc_raw = ADC1_true / R_sense
I_real_mA = 0.866337 * I_adc_raw_mA + 0.533330
```

注意：`PA0` 和 `PA1` 输入电压必须始终保持在 `0~3.3V`。如果实际采样点可能超过 `3.3V`，必须先经过分压、缓冲或保护电路。

## PWM 到功率驱动接线

```text
PA8 / TIM1_CH1  -> HIN / HI
PA7 / TIM1_CH1N -> LIN / LI
GND             -> 功率驱动板信号地
```

当前 TIM1 设定：

```text
PWM 频率：约 10 kHz
互补输出：开启
死区时间：CubeMX 中 `DeadTime = 72`
```

`PA8` 和 `PA7` 只是逻辑控制信号，不要直接接到高压、大电流节点。

## 按键模块接线

按键模块使用公共地，上拉输入：

```text
按键公共 GND -> STM32 GND
加 1 按键输出 -> PB6 / UP
减 1 按键输出 -> PB7 / DOWN
```

逻辑：

```text
松开 = 高电平
按下 = 低电平
```

## EC11 旋转编码器接线

```text
编码器 UCC/VCC -> 3.3V
编码器 GND     -> GND
编码器 A       -> PB12 / ENC_A
编码器 B       -> PB13 / ENC_B
编码器 C/SW    -> PB8  / ENC_SW
```

说明：

- `PB12` 和 `PB13` 是双边沿外部中断输入。
- 旋转编码器用于改变设定电流。
- 如果旋转方向反了，可以交换 `A` 和 `B`，也可以在代码里反向。

## 蜂鸣器接线

当前蜂鸣器为低电平触发。

```text
PB9 / BUZZER -> 蜂鸣器模块信号输入
蜂鸣器 VCC   -> 模块要求的供电
蜂鸣器 GND   -> STM32 GND
```

固件行为：

```text
设定电流 <  550mA -> PB9 输出 High，蜂鸣器不响
设定电流 >= 550mA -> PB9 输出 Low，蜂鸣器响
```

## USB-TTL 串口接线

```text
PA9  / USART1_TX -> USB-TTL RXD
PA10 / USART1_RX -> USB-TTL TXD
GND              -> USB-TTL GND
```

串口参数：

```text
波特率：115200
数据位：8
校验位：无
停止位：1
换行：\r\n
```

常用命令：

```text
SET=100
DUTY=50.0
DUTYMAX=90.0
KP=0.030
KI=0.001
KD=0.000
START
STOP
STATUS
HELP
```

## ST-Link 下载调试接线

```text
ST-Link SWDIO -> PA13
ST-Link SWCLK -> PA14
ST-Link GND   -> STM32 GND
ST-Link 3.3V  -> 目标板 3.3V 参考电压，视 ST-Link 类型决定是否需要
```

`PA13` 和 `PA14` 必须保留给下载和调试，不要接其他模块。

## 当前启用外设汇总

| 外设 | 使用引脚 | 项目作用 |
|---|---|---|
| `ADC1` | `PA0` | 采样电阻电压，用于计算电流。 |
| `ADC2` | `PA1` | 输出端/负载端总电压采样。 |
| `TIM1` | `PA8`, `PA7` | 主 PWM 和互补 PWM，控制功率级。 |
| `USART1` | `PA9`, `PA10` | 串口调试和 PID 参数命令接口。 |
| `I2C2` | `PB10`, `PB11` | I2C LCD1602 显示屏通信。 |
| `GPIO` | `PB6`, `PB7`, `PB8`, `PB9` | 按键、编码器按键、蜂鸣器。 |
| `EXTI15_10` | `PB12`, `PB13` | 旋转编码器 A/B 相中断。 |
| `SYS/SWD` | `PA13`, `PA14` | 下载和调试。 |
| `RCC/HSE` | `PD0`, `PD1` | 外部高速晶振。 |

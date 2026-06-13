# PWMADC 恒流源项目交接文档

> 生成时间: 2026-06-12
> 硬件: STM32F103C8T6
> 目标: 恒流源闭环控制，0~600mA，精度 ±0.5%

---

## 一、项目目标

STM32F103C8T6 控制恒流源：
- 通过旋转编码器/按键/串口设置目标电流 `Set`（0~600mA）
- ADC1 采样电阻电压 → 计算实际电流
- ADC2 采样负载端电压
- TIM1 互补 PWM（PA8 CH1 + PA7 CH1N）驱动外部功率芯片 HIN/LIN
- PI(D) 闭环调节占空比，使输出电流稳定在设定值
- I2C LCD1602 显示设定电流、实际电流、占空比
- USART1 串口调试输出 + 接收命令
- 蜂鸣器 ≤550mA 时静默，≥550mA 报警

**最终精度要求**: 全量程 0~100mA，每 5mA 一个台阶，误差 ≤ ±0.5% of setpoint，对照 DM3058 万用表真值。

---

## 二、硬件配置

### 2.1 MCU
- 型号: STM32F103C8T6
- 封装: LQFP48
- 外部高速晶振: PD0/PD1
- 调试: SWD (PA13 SWDIO, PA14 SWCLK)

### 2.2 引脚分配（完整）

| 引脚 | 代码标签 | 功能 | 外部连接 |
|------|---------|------|---------|
| PA0 | - | ADC1_IN0 | 采样电阻 R_sense 电压 |
| PA1 | - | ADC2_IN1 | 输出端/负载端总电压 |
| PA7 | - | TIM1_CH1N | 互补 PWM，接驱动芯片 LIN/LI |
| PA8 | - | TIM1_CH1 | 主 PWM，接驱动芯片 HIN/HI |
| PA9 | - | USART1_TX | USB-TTL RXD |
| PA10 | - | USART1_RX | USB-TTL TXD |
| PA13 | - | SYS_JTMS-SWDIO | ST-Link SWDIO |
| PA14 | - | SYS_JTCK-SWCLK | ST-Link SWCLK |
| PB6 | UP | GPIO_Input 上拉 | 电流+按键 (公共端GND，按下为低) |
| PB7 | DOWN | GPIO_Input 上拉 | 电流-按键 |
| PB8 | ENC_SW | GPIO_Input 上拉 | EC11 编码器按键 |
| PB9 | BUZZER | GPIO_Output 初始高 | 低电平触发蜂鸣器 |
| PB10 | - | I2C2_SCL | LCD1602 I2C SCL |
| PB11 | - | I2C2_SDA | LCD1602 I2C SDA |
| PB12 | ENC_A | GPIO_EXTI12 上拉 双边沿 | EC11 编码器 A 相 |
| PB13 | ENC_B | GPIO_EXTI13 上拉 双边沿 | EC11 编码器 B 相 |
| PD0 | - | RCC_OSC_IN | 外部晶振 |
| PD1 | - | RCC_OSC_OUT | 外部晶振 |

### 2.3 关键硬件参数

```
采样电阻 R_sense: 5.62Ω（代码中 5620 mΩ）
ADC: 12-bit, 参考电压 3.3V, 满量程 4095
电源: 32V DC
负载范围: 0~50Ω
PWM 频率: 约 1.5kHz（TIM1 预分频 8，ARR 6000-1，72MHz APB2）
PWM 模式: 边沿对齐，互补输出，死区时间 72
```

### 2.4 LCD 配置

- LCD1602 + PCF8574T I2C 转接板
- I2C2: PB10(SCL), PB11(SDA), 100kHz Standard Mode
- I2C 地址: 0x27（自动扫描）
- 位映射: RS=P0, RW=P1, E=P2, BL=P3, D4=P4, D5=P5, D6=P6, D7=P7

### 2.5 外部仪器

- **DM3058 万用表**: Rigol DM3058, VISA 资源名 `USB0::0x1AB1::0x09C4::DM3L204800759::INSTR`
  - 用途: 真实电流参考（串联在输出回路中）
  - 通过 pyvisa 读取 `MEAS:CURR:DC?`
  - 注意 Remote/Local 模式切换

- **ST-Link V2**: 烧录调试
  - SWD 接口，目标电压 ~3.22V
  - OpenOCD 0.12.0

- **USB-TTL 串口模块**: COM11, 115200, 8N1, 无流控, \r\n 换行

---

## 三、软件架构

### 3.1 项目结构

```
D:\PWMADC
├── Core/
│   ├── Inc/
│   │   ├── main.h              # CubeMX 生成的引脚宏
│   │   ├── stm32f1xx_hal_conf.h
│   │   └── stm32f1xx_it.h
│   └── Src/
│       ├── main.c              # ★ 核心文件，所有业务逻辑在此
│       ├── stm32f1xx_hal_msp.c
│       ├── stm32f1xx_it.c
│       ├── syscalls.c
│       ├── sysmem.c
│       └── system_stm32f1xx.c
├── Drivers/
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
├── docs/
│   └── pinout.md               # 引脚接线文档
├── tools/
│   ├── auto_duty_control.py    # 自动占空比控制脚本
│   ├── serial_live_dashboard.py # 串口实时监视 GUI
│   └── serial_pid_console.py   # 串口 PID 调试控制台
├── build/Debug/
│   └── PWMADC.elf              # 当前可烧录固件
├── PWMADC.ioc                  # CubeMX 配置文件（不要随意改）
├── CMakeLists.txt
├── CMakePresets.json
├── STM32F103C8Tx_FLASH.ld
├── *.csv                       # 低电流扫描实验数据
└── *.txt                       # 调试日志
```

### 3.2 main.c 代码结构

main.c 包含几乎所有业务逻辑，约 1600 行。按区域划分：

| 区域 | 位置 | 内容 |
|------|------|------|
| 宏定义区 | `USER CODE BEGIN PD` | ADC/PID/PWM/LCD 所有可调参数 |
| 全局变量区 | `USER CODE BEGIN PV` | ADC 值、PID 状态、串口缓冲、LCD 状态 |
| 函数原型 | `USER CODE BEGIN PFP` | 所有私有函数声明 |
| 函数实现 | `USER CODE BEGIN 0` | ADC 读取、PID 控制、串口命令、LCD 驱动等 |
| 初始化 | `USER CODE BEGIN 1/2` | 外设初始化后的自定义配置 |
| 主循环 | `USER CODE BEGIN 3` | while(1) 主循环 |

### 3.3 核心函数

| 函数 | 功能 |
|------|------|
| `Read_Dual_ADC_Once()` | 采样 32 组 ADC1+ADC2，去高低 8 个后取平均 |
| `ADC_Trimmed_Average()` | 排序后去极值的平均算法 |
| `Get_Measured_Current_uA()` | ADC 值 → 电流(uA)，含 DM3058 校准校正 |
| `Update_PID_Control()` | PID 闭环计算，含测量滤波、分区参数、步长限制 |
| `Process_UART_Command_Line()` | 串口命令解析（SET/DUTY/KP/KI/KD/START/STOP 等） |
| `Set_PWM_Duty_Permille()` | 设置 PWM 占空比（千分比单位） |
| `Prepare_PID_Start()` | PID 启动前状态初始化 |
| `Reset_PID_State()` | PID 状态重置、积分清零 |
| `Apply_Current_Setpoint()` | 设定值变更处理 |
| `LCD_Init()` / `LCD_Show_Status()` | I2C LCD 驱动 |
| `Process_Encoder_Transition()` | 编码器 A/B 相中断处理 |
| `Update_Buzzer()` | 蜂鸣器报警判断 |

### 3.4 串口协议

- **端口**: COM11, 115200 baud, 8N1
- **输出周期**: 500ms
- **输出格式**:
  ```
  Set:xxmA IMEAS:xx.xxxmA ERR:+/-xx.xxxmA | ADC1:raw,x.xxxV ADC2:raw,x.xxxV | FF:x.x% TRIM:x.x% P:x.x% I:x.x% OUT:x.x% LIM:x.x% | SW:x EVT:x | I2C:0x27 | PID:ON/OFF KP:x.xxx KI:x.xxx KD:x.xxx
  ```
- **支持命令**:
  ```
  SET=<mA>       设定目标电流
  DUTY=<0-100>   手动占空比（自动关PID）
  DUTYMAX=<0-90> 占空比上限
  START          开启 PID
  STOP           停止输出（占空比归零，关PID）
  PID=ON/OFF     PID 开关
  KP=<x.xxx>     设置 KP
  KI=<x.xxx>     设置 KI
  KD=<x.xxx>     设置 KD
  PIDRESET       重置 PID 状态
  STATUS         查询当前状态
  ```

### 3.5 LCD 显示内容

```
第一行: Set: xxxmA
第二行: I:xxx.xx D:xx%
```

LCD 刷新周期: 500ms（避免阻塞编码器响应）

---

## 四、当前固件参数（已烧录）

### 4.1 ADC 参数

| 宏 | 值 | 说明 |
|----|-----|------|
| `CURRENT_SENSE_RESISTOR_MOHM` | `5620L` | 采样电阻 5.62Ω |
| `ADC_REFERENCE_MV` | `3300U` | ADC 参考电压 |
| `ADC_FULL_SCALE` | `4095U` | 12-bit ADC |
| `ADC1_TRUE_NUMERATOR` | `11UL` | ADC1 外部分压比分子 |
| `ADC1_TRUE_DENOMINATOR` | `10UL` | ADC1 外部分压比分母 |
| `ADC2_TRUE_NUMERATOR` | `11UL` | ADC2 外部分压比分子 |
| `ADC2_TRUE_DENOMINATOR` | `1UL` | ADC2 外部分压比分母 |
| `ADC_AVERAGE_SAMPLES` | `32U` | 每次 ADC 读取采样 32 组 |
| `ADC_TRIM_DISCARD_SAMPLES` | `8U` | 排序后丢弃最高/最低各 8 个 |
| `ADC_SAMPLE_SPACING_MS` | `1U` | 组间间隔 1ms |
| `ADC_VERY_LOW_CURRENT_THRESHOLD_UA` | `10000UL` | <10mA 触发极低电流校正 |
| `ADC_VERY_LOW_CURRENT_CORRECTION_NUMERATOR` | `940UL` | 极低电流校正系数 0.940 |
| `ADC_VERY_LOW_CURRENT_CORRECTION_DENOMINATOR` | `1000UL` | |
| `ADC_HIGH_CORRECTION_START_UA` | `100000UL` | >100mA 校正触发点 |
| `ADC_HIGH_CORRECTION_NUMERATOR` | `2170UL` | 高电流校正 |
| `ADC_HIGH_CORRECTION_DENOMINATOR` | `100000UL` | |

### 4.2 PID 参数

| 宏 | 值 | 说明 |
|----|-----|------|
| `PID_CONTROL_PERIOD_MS` | `50U` | PID 控制周期 50ms |
| `PID_STEP_LIMIT_PERMILLE` | `20L` | 通用占空比步长限制 2.0% |
| `PID_LOW_CURRENT_STEP_LIMIT_PERMILLE` | `20L` | 低电流步长限制 2.0% |
| `PID_MEAS_FILTER_SHIFT` | `3U` | 通用测量低通滤波（α≈0.125） |
| `PID_LOW_CURRENT_MEAS_FILTER_SHIFT` | `1U` | ≤20mA 滤波（α≈0.5） |
| `PID_VERY_LOW_CURRENT_MEAS_FILTER_SHIFT` | `1U` | ≤10mA 滤波（α≈0.5） |
| `PID_DUTY_FINE_SCALE` | `1000L` | 占空比精细计算比例 |
| `PID_USE_FEEDFORWARD` | `0U` | 前馈关闭 |

### 4.3 PWM 参数

| 宏 | 值 | 说明 |
|----|-----|------|
| `TIM1_FINE_PWM_PRESCALER` | `8U` | TIM1 预分频 |
| `TIM1_FINE_PWM_PERIOD_COUNTS` | `6000U` | ARR=6000，分辨率 6000 计数 |
| `PWM_DUTY_DEFAULT_LIMIT_PERMILLE` | `900U` | 默认占空比上限 90% |
| `PWM_DUTY_HARD_LIMIT_PERMILLE` | `900U` | 硬限制 90% |

PWM 频率 = 72MHz / (8 × 6000) ≈ 1.5kHz

### 4.4 默认 PID 参数

| 参数 | 值 |
|------|-----|
| `pid_kp_milli` | `50` (即 KP=0.050) |
| `pid_ki_milli` | `80` (即 KI=0.080) |
| `pid_kd_milli` | `0` (即 KD=0.000) |

### 4.5 PID 分区策略

| 设定值范围 | KP有效值 | KI有效值 | 步长限制 | 测量滤波 |
|-----------|---------|---------|---------|---------|
| <20mA | 50 (全值) | 80 (全值) | 2.0% | shift=1 |
| 20~50mA | 50 (全值) | 80 (全值) | 1.0% | shift=3 |
| ≥50mA | 50 (全值) | 80 (全值) | 2.0% | shift=3 |

### 4.6 ADC 电流计算逻辑

```
1. V_sense = ADC1_raw * 3.3V * (11/10) / 4095
2. I_raw = V_sense / 5.62Ω
3. 校正:
   - I_raw ≤ 10mA: I = I_raw * 0.940
   - 10mA < I_raw ≤ 12mA: I = I_raw (不校正，过渡区)
   - I_raw > 12mA: I = I_raw * 1.020
4. 限幅: I ≤ 65535mA
```

> **注意**: 步骤 3 的 >12mA 的 1.020x 校正是在最后一次迭代中添加的，**尚未进行闭环测试验证**。

---

## 五、调试迭代历史

### 迭代 0: 初始状态
- **参数**: R=5620, 校正=0.388, 32样本, shift=0@极低, KP减半@低电流, step=2@20-50mA
- **状态**: 烧录成功但未系统测试

### 迭代 1: 首次修正 + 闭环测试
- **修改**:
  1. 极低校正: 0.388 → 0.910（DM3058 实测表明旧系数严重低估）
  2. 极低滤波: shift 0 → 1
  3. <20mA KP: 恢复全值 50（之前被减半为25）
  4. 20-50mA step: 硬编码 2 → 10
- **测试结果**: 5/21 通过
  - 5mA: IMEAS=4.899, DM3058=5.081, 误差=-0.182mA ✅
  - 10mA: +0.254mA ✅
  - 15mA: -0.932mA ✅
  - 20mA: -0.118mA ✅
  - 40mA: -0.095mA ✅
  - 其余 16 点 FAIL，主要是 IMEAS < DM3058（ADC 偏低 1-6%）

### 迭代 2: 开环校准测试
- **目的**: 排除 PID 干扰，直接看 ADC vs DM3058
- **方法**: PID=OFF, 固定 DUTY 0~17%
- **发现**: 
  - ADC_raw 与 DM3058 基本线性，斜率约 0.1589 mA/count
  - 代码斜率 0.1577 mA/count，差异仅 0.7%
  - 但存在 ~4% 的测量噪声（因异步采样）
  - ADC1 电压与电流关系非常线性，ADC 本身没问题

### 迭代 3: 48样本 + 3段分段校正 → 失败 ❌
- **修改**: 样本 32→48, 丢弃 8→6, R=5620, 3段校正（10-25mA×1.055, 25-75mA×1.012, >75mA×1.005）
- **结果**: SET=5mA 彻底崩溃（IMEAS=19.2mA, OUT=0.0%）
- **根因**: 10-25mA 的 1.055 倍校正放大 ADC 噪声，PID 读到虚假高电流后归零死锁

### 迭代 4: R=5550 + 回退简单校正 → 失败 ❌
- **修改**: R=5620→5550, 回退到仅极低校正, 48样本 6丢弃
- **结果**: 5mA 仍崩溃，其余点多点 IMEAS > DM3058（过校正）
- **根因**: R=5550 增益过高（1.26% 增幅），100mA 超调

### 迭代 5: 64样本 + 12丢弃 + R=5550 → 失败 ❌
- **修改**: 样本 48→64, 丢弃 6→12, R=5550
- **结果**: 16/21 FAIL, ADC 读取 64ms 阻塞主循环
- **根因**: 64ms ADC > 50ms PID 周期，时序混乱

### 迭代 6: R=5600 → 失败 ❌
- **修改**: R=5550→5600, 64样本, 12丢弃, 加 Kick-start
- **结果**: 16/21 FAIL, 5mA 仍崩溃（IMEAS=14.3mA, OUT=0.0%）
- **根因**: Kick-start 无效，增益仍偏高

### 迭代 7: 回到 v1 基线 + 微调 → 有所改善
- **修改**: R=5600→5620, 64→32样本, 12→8丢弃, 校正 0.910→0.940, 去除 Kick-start
- **结果**: 5mA 通过 ✅, 100mA 通过 ✅, 18/21 FAIL（所有中间点偏低 ~2%）

### 迭代 8: 最终校准（当前烧录版本）
- **修改**: 在迭代 7 基础上，对 >12mA 添加 1.020x 全局校正
- **校准逻辑**:
  ```
  ≤10mA:  ×0.940
  10-12mA: ×1.000 (过渡区)
  >12mA:  ×1.020
  ```
- **测试结果**: ⚠️ **尚未测试**（用户在此时要求生成交接文档）

---

## 六、当前代码修改汇总

相对于 CubeMX 生成的原始代码，在 `USER CODE BEGIN/END` 区域内的修改：

### 6.1 ADC 电流计算 (Get_Measured_Current_uA)
```c
// 原始: I = V/R，仅有一个 0.388 校正
// 现在: I = V/R，分段校正:
//   ≤10mA → ×0.940
//   >12mA → ×1.020
```

### 6.2 PID 控制 (Update_PID_Control)
```c
// 原始: <20mA KP减半，20-50mA step硬编码2
// 现在: <20mA KP全值，20-50mA step=10
```

### 6.3 参数变更
```
ADC_VERY_LOW_CURRENT_CORRECTION_NUMERATOR: 388 → 940
PID_VERY_LOW_CURRENT_MEAS_FILTER_SHIFT: 0 → 1
```

---

## 七、构建和烧录

### 构建
```powershell
cmake --build build/Debug
```
编译器: GNU Arm Embedded Toolchain 14.3.1
构建系统: CMake + Ninja

### 烧录 (OpenOCD)
```powershell
C:/Users/Lenovo/scoop/shims/openocd.exe `
  -s C:/Users/Lenovo/scoop/apps/openocd/0.12.0/share/openocd/scripts `
  -f interface/stlink.cfg `
  -f target/stm32f1x.cfg `
  -c "adapter speed 1000" `
  -c "program D:/PWMADC/build/Debug/PWMADC.elf verify reset exit"
```

### 烧录成功标志
```
Programming Finished
Verify Started
Verified OK
Resetting Target
```

---

## 八、调试工具脚本

### 8.1 串口验证脚本 (临时文件)
路径: `/tmp/duty_test.py` (每次生成)
功能: 连接 COM11，发送 DUTY 命令，读取响应

### 8.2 PID 闭环测试脚本 (临时文件)
路径: `/tmp/pid_test2.py`, `/tmp/full_sweep2.py`, `/tmp/sweep_v2.py`
功能: 
- 连接 COM11 串口 + DM3058 VISA
- 发送 SET 命令从 0→100mA 步进
- 每步稳定 12 秒后取 IMEAS 和 DM3058 平均值
- 输出误差和通过/失败判定

### 8.3 开环校准脚本
路径: `/tmp/openloop_test.py`
功能: PID=OFF，固定 DUTY 0~17%，读取 ADC_raw、IMEAS、DM3058

---

## 九、已知问题和教训

### 9.1 硬件限制
1. **低占空比死区**: 占空比 <0.5% 时 MOSFET 无法正常导通（最小导通时间不足）
2. **ADC 与 PWM 异步**: 无同步触发，ADC 采样窗口随机落在 PWM 纹波的不同相位
3. **12-bit ADC 量化噪声**: 5mA 时信号仅 ~30mV，量化误差 ~0.14mA (2.8%)
4. **采样电阻实际值**: 标称 5.66Ω，万用表测 5.62Ω，可能有 PCB 走线额外电阻

### 9.2 软件教训
1. **分段校正的边界效应**: 在校正系数不连续的边界处（如 10mA），PID 可能振荡或死锁
2. **校正系数 >1.0 的危险**: 在低电流噪声区，>1.0 的校正会放大 ADC 噪声，导致 PID 读假值
3. **ADC 采样时间不宜过长**: 32 样本已经阻塞主循环 32ms，64 样本会打乱 PID 时序
4. **Kick-start 无效**: 在 Prepare_PID_Start 中设置初始占空比被后续 PID 周期立即覆盖
5. **避免全局增益大幅调整**: R 值从 5620 改到 5550 导致高端过冲；建议每次调整 ≤1%

### 9.3 回退过的方案（不要再试）
1. ❌ 分段校正（边界死锁）
2. ❌ Kick-start 占空比（无效）
3. ❌ 64 样本 ADC（阻塞主循环）
4. ❌ R 值大幅下调（过校正）
5. ❌ 自适应采样（之前迭代已回退）

---

## 十、下一步建议

### 10.1 立即测试
当前烧录的固件（迭代 8）尚未闭环测试。应：
1. 连接所有硬件（STM32 + DM3058 + 负载）
2. 烧录 `D:/PWMADC/build/Debug/PWMADC.elf`
3. 运行 `/tmp/sweep_v2.py` 进行 0-100mA 全量程测试
4. 查看 1.020x 全局校正是否让中间段通过

### 10.2 如果还有故障点
- 极低电流 (<10mA): 调 `ADC_VERY_LOW_CURRENT_CORRECTION_NUMERATOR` (当前 940)
- 全局偏低/高: 调 `Get_Measured_Current_uA()` 中 1020 系数
- 响应速度: 调 `pid_kp_milli` / `pid_ki_milli` (当前 50/80)
- 注意: 每次只改一个参数，编译→烧录→测试→对比

### 10.3 项目约束
- 代码只在 `USER CODE BEGIN/END` 区域修改
- 不要修改 `.ioc` CubeMX 配置文件
- 不要 `git reset --hard`（工作区有大量未提交数据）
- 每次改动必须对照 DM3058 验证，不能只看 IMEAS

---

## 十一、Git 状态

当前工作区有大量修改和未跟踪文件，不可假设为干净状态：

```
MM .mxproject
A  .vscode/launch.json
MM Core/Inc/main.h
MM Core/Inc/stm32f1xx_hal_conf.h
M  Core/Inc/stm32f1xx_it.h
MM Core/Src/main.c
MM Core/Src/stm32f1xx_hal_msp.c
M  Core/Src/stm32f1xx_it.c
MM PWMADC.ioc
MM cmake/stm32cubemx/CMakeLists.txt
AM docs/pinout.md
AM gdbClient_log.txt
AM stlinkgdbserver_log.txt
A  tools/auto_duty_control.py
A  tools/serial_live_dashboard.py
A  tools/serial_pid_console.py
?? .vscode/tasks.json
?? low_current_sweep_correction_curve.csv
?? low_current_sweep_latest.csv
?? low_current_sweep_start_duty.csv
?? low_current_test.csv
?? low_current_test_after_adaptive.csv
```

main 分支，最近提交: `54cac78 first commit`

---

## 十二、DM3058 连接参考

```python
import pyvisa
rm = pyvisa.ResourceManager()
print(rm.list_resources())  # 应包含 'USB0::0x1AB1::0x09C4::DM3L204800759::INSTR'
inst = rm.open_resource("USB0::0x1AB1::0x09C4::DM3L204800759::INSTR")
inst.timeout = 3000
inst.write("CONF:CURR:DC")  # 设置为直流电流模式
current_a = float(inst.query("MEAS:CURR:DC?").strip())
current_ma = current_a * 1000.0
```

---

## 十三、最终一句话

> STM32F103C8T6 恒流源，PID 闭环，当前已烧录 2 区段 DM3058 校准版本（≤10mA ×0.940, >12mA ×1.020, 10-12mA 过渡区无校正）。上一次闭环测试显示 5mA 和 100mA 通过 ±0.5% 标准，中间段偏低约 2%。最新固件添加了 1.020x 全局校正但尚未测试。DM3058 已连接，COM11 可用，OpenOCD 烧录正常。

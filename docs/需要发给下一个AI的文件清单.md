# 需要发给下一个 AI 的文件清单

下面这些文件是建议发给下一个 AI 的完整材料。

---

## 一、必须发送的说明文档

| 文件 | 用途 |
|---|---|
| `docs/给下一个AI的项目交接总说明.md` | 最完整的项目交接文档，包含原理、需求、经验、例子 |
| `docs/ADC数据给下一个AI的说明.md` | 解释 ADC 数据每一列是什么意思，避免误读 |
| `docs/可复用代码模块与保护逻辑说明.md` | 说明当前项目哪些代码逻辑可复用，以及短路/开路/过流保护思路 |
| `docs/next_ai_requirements.md` | 较简洁的新需求清单，适合快速浏览 |

---

## 二、必须发送的 ADC 数据

| 文件 | 用途 |
|---|---|
| `docs/adc_duty_reference_stable.csv` | 稳定状态下的原始测量点，包含 DM3058 电流、占空比、ADC 数据 |
| `docs/adc_duty_reference_stable_averaged.csv` | 聚合后的稳定数据，更适合做拟合 |

如果只能发一个 ADC 数据文件，优先发：

```text
docs/adc_duty_reference_stable_averaged.csv
```

---

## 三、建议发送的当前工程参考文件

这些文件不是让下一个 AI 原样照搬，而是让它理解当前工程状态。

| 文件 | 用途 |
|---|---|
| `Core/Src/main.c` | 当前所有业务逻辑集中在这里，仅供参考 |
| `Core/Inc/main.h` | 当前 CubeMX 生成的引脚宏定义 |
| `PWMADC.ioc` | 当前 CubeMX 配置 |
| `docs/pinout.md` | 当前引脚连接说明 |

---

## 四、可选发送的清洗过程文件

如果下一个 AI 想知道 ADC 数据是怎么清洗出来的，可以发：

| 文件 | 用途 |
|---|---|
| `docs/adc_measurements_stable_dm3058_only_summary.md` | 数据清洗规则和每组保留情况 |
| `tools/clean_adc_measurements.py` | 清洗 ADC 历史数据的脚本 |
| `tools/make_adc_duty_reference_table.py` | 生成稳定原始点表的脚本 |
| `tools/make_adc_duty_reference_averaged.py` | 生成聚合表的脚本 |

---

## 五、发给下一个 AI 时的建议说明

可以直接对下一个 AI 说：

```text
请先阅读 docs/给下一个AI的项目交接总说明.md。

这个项目是 STM32F103C8T6 恒流源控制项目。前一个 AI 的代码只作为经验参考，不要照搬复杂算法。

请重点根据四档输入电压、ADC 分段校准、自适应逼近控制、轻量 PID 稳定维持、跨档位保护、短路/开路保护、模块化代码结构这些要求重新设计。

ADC 数据中，DM3058 实际电流才是真值，STM32 ADC/IMEAS 只是被校准对象。
```


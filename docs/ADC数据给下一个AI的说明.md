# ADC 数据给下一个 AI 的说明

ADC 数据没有放进总交接文档里，而是单独保存成 CSV 表格。

下一个 AI 使用这些数据时，必须记住：

```text
DM3058 实际电流 = 真值
STM32 ADC 数据 = 被校准对象
STM32 IMEAS = 当时代码算出来的估计值，不是真值
```

---

## 一、推荐使用的两个数据文件

| 文件 | 用途 |
|---|---|
| `docs/adc_duty_reference_stable.csv` | 稳定状态下的原始测量点 |
| `docs/adc_duty_reference_stable_averaged.csv` | 按设定电流和占空比聚合后的均值表，更适合拟合 |

如果要拟合 ADC 或分析规律，优先使用：

```text
docs/adc_duty_reference_stable_averaged.csv
```

如果要追溯某一个原始点，再看：

```text
docs/adc_duty_reference_stable.csv
```

---

## 二、原始稳定点表说明

文件：

```text
docs/adc_duty_reference_stable.csv
```

主要列：

| 列名 | 中文含义 | 是否是真值 |
|---|---|---|
| `actual_current_dm3058_ma` | DM3058 实际测得电流，单位 mA | 是 |
| `output_duty_pct` | 当时输出的 PWM 占空比，单位 % | 控制输出 |
| `adc1_raw` | STM32 ADC1 原始值 | 否，被校准对象 |
| `adc1_voltage_v` | ADC1 换算电压，单位 V | 否，被校准对象 |
| `adc2_raw` | STM32 ADC2 原始值 | 否，被校准对象 |
| `adc2_voltage_v` | ADC2 换算电压，单位 V | 否，被校准对象 |
| `set_current_ma` | 当时设定电流，单位 mA | 目标值 |
| `stm32_reported_current_ma` | 当时代码根据 ADC 算出来的电流 | 否，只能参考 |
| `source_file` | 原始数据来源文件 | 追溯用 |
| `source_row` | 原始数据来源行号 | 追溯用 |
| `time_s` | 该点在测试过程中的时间 | 追溯用 |
| `sample_count` | 当时 ADC 采样点数 | 参考 |
| `dropped_each_side` | 当时去掉的高/低离群采样数 | 参考 |

---

## 三、聚合表说明

文件：

```text
docs/adc_duty_reference_stable_averaged.csv
```

这张表按：

```text
设定电流 + 输出占空比
```

对多个稳定测量点做了聚合。

主要列：

| 列名 | 中文含义 |
|---|---|
| `set_current_ma` | 设定电流，单位 mA |
| `output_duty_pct` | 输出占空比，单位 % |
| `sample_count_rows` | 参与聚合的原始稳定点数量 |
| `actual_current_dm3058_mean_ma` | DM3058 实际电流平均值，单位 mA |
| `actual_current_dm3058_median_ma` | DM3058 实际电流中位数，单位 mA |
| `actual_current_dm3058_std_ma` | DM3058 实际电流标准差，表示波动 |
| `adc1_raw_mean` | ADC1 原始值平均值 |
| `adc1_raw_median` | ADC1 原始值中位数 |
| `adc1_voltage_mean_v` | ADC1 电压平均值，单位 V |
| `stm32_reported_current_mean_ma` | STM32 当时代码估计电流平均值 |
| `source_files` | 数据来源文件 |

这张表更适合做拟合，因为它减少了单点波动。

---

## 四、不要误读的数据现象

有些低电流点可能出现：

```text
DM3058 实际测到约 1 mA
但 adc1_raw = 0
```

这不一定是要删除的异常点。

它说明：

```text
在极低电流段，STM32 ADC 当时可能分辨不出真实电流。
```

这对后续 ADC 校准非常重要。

因此：

1. 不要因为 `adc1_raw = 0` 就直接删掉。
2. 只要 DM3058 数据稳定，它就是有效的硬件事实。
3. 这类点说明低电流档需要更高 ADC 分辨率、更好采样方式，或更合理的硬件放大/分压设计。

---

## 五、已经删除的数据

原始数据中已经删除：

1. DM3058 为负值的数据。
2. DM3058 接近 0、疑似开路或未输出的数据。
3. 刚切换占空比后的过渡数据。
4. 同一组里偏离 DM3058 稳态中心太远的离群点。
5. 整组 DM3058 波动过大、不适合拟合的数据。

清洗说明见：

```text
docs/adc_measurements_stable_dm3058_only_summary.md
```

---

## 六、给下一个 AI 的建议

拟合 ADC 时建议：

1. 优先使用 `adc_duty_reference_stable_averaged.csv`。
2. 以 `actual_current_dm3058_mean_ma` 或 `actual_current_dm3058_median_ma` 为真实电流。
3. 用 `adc1_raw_mean` 或 `adc1_voltage_mean_v` 作为输入。
4. 不要把 `stm32_reported_current_mean_ma` 当真值。
5. 不要全范围强行拟合一条曲线。
6. 应按电压档位和电流小区间分段拟合。


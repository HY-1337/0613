# 稳定 ADC/DM3058 测量数据清洗说明

输出文件：`docs/adc_measurements_stable_dm3058_only.csv`

清洗原则：以 DM3058 实测电流为真值，只保留每个“来源文件 + 设定电流”分组中进入稳态后的数据。STM32 的 `IMEAS/ADC1/ADC2` 只作为被校准对象，不参与真值判断。

规则摘要：

- 删除非数值 DM3058 读数。
- 对非零设定电流，删除 DM3058 小于等于 0.02 mA 的开路、未输出或明显瞬态读数。
- 每个分组删除前段过渡期：至少前 2 秒，通常取前 25%，最多按 8 秒处理。
- 用剩余 DM3058 读数中位数作为稳态中心，按中位绝对偏差和相对误差阈值删除离群点。
- 若清洗后样本太少或 DM3058 波动范围仍过大，则整组舍弃。

## 汇总

| 来源文件 | 原始行数 | 保留行数 | 分组数 | 接受分组数 |
|---|---:|---:|---:|---:|
| low_current_sweep_correction_curve.csv | 138 | 92 | 7 | 7 |
| low_current_sweep_latest.csv | 162 | 93 | 3 | 3 |
| low_current_sweep_start_duty.csv | 139 | 89 | 7 | 6 |
| low_current_test.csv | 136 | 0 | 7 | 0 |
| low_current_test_after_adaptive.csv | 136 | 92 | 3 | 3 |

总原始行数：711

总保留行数：366

## 分组清洗结果

| 来源文件 | 设定 mA | 原始 | 有效 DM3058 | 稳态候选 | 保留 | DM3058 中位 mA | 保留均值 mA | 保留标准差 mA | 保留极差 mA | 结果 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| low_current_sweep_correction_curve.csv | 1.0000 | 16 | 11 | 7 | 5 | 1.0743 | 1.1122 | 0.0539 | 0.1065 | accepted |
| low_current_sweep_correction_curve.csv | 3.0000 | 19 | 19 | 14 | 13 | 2.7814 | 2.7739 | 0.0269 | 0.0973 | accepted |
| low_current_sweep_correction_curve.csv | 5.0000 | 20 | 20 | 14 | 12 | 5.0804 | 5.0568 | 0.0820 | 0.2841 | accepted |
| low_current_sweep_correction_curve.csv | 8.0000 | 20 | 20 | 15 | 15 | 8.1057 | 8.2825 | 0.2491 | 0.6151 | accepted |
| low_current_sweep_correction_curve.csv | 10.0000 | 19 | 19 | 14 | 14 | 10.1212 | 10.1702 | 0.2233 | 0.6172 | accepted |
| low_current_sweep_correction_curve.csv | 15.0000 | 22 | 22 | 16 | 16 | 15.4436 | 15.4111 | 0.0550 | 0.1669 | accepted |
| low_current_sweep_correction_curve.csv | 20.0000 | 22 | 22 | 17 | 17 | 20.3095 | 20.3095 | 0.0002 | 0.0007 | accepted |
| low_current_sweep_latest.csv | 1.0000 | 83 | 74 | 53 | 34 | 1.0749 | 1.1132 | 0.0850 | 0.2820 | accepted |
| low_current_sweep_latest.csv | 10.0000 | 51 | 51 | 38 | 38 | 11.1816 | 11.1321 | 0.2581 | 1.2315 | accepted |
| low_current_sweep_latest.csv | 20.0000 | 28 | 28 | 21 | 21 | 20.3277 | 20.2299 | 0.2306 | 0.7464 | accepted |
| low_current_sweep_start_duty.csv | 1.0000 | 16 | 14 | 10 | 0 | 1.3814 |  |  |  | rejected_unstable_or_too_few range=0.6687 |
| low_current_sweep_start_duty.csv | 3.0000 | 20 | 20 | 15 | 14 | 2.7842 | 2.7841 | 0.0001 | 0.0002 | accepted |
| low_current_sweep_start_duty.csv | 5.0000 | 20 | 20 | 15 | 13 | 5.0836 | 5.1303 | 0.0993 | 0.3164 | accepted |
| low_current_sweep_start_duty.csv | 8.0000 | 20 | 20 | 14 | 14 | 8.7280 | 8.7632 | 0.0734 | 0.2054 | accepted |
| low_current_sweep_start_duty.csv | 10.0000 | 19 | 19 | 14 | 14 | 11.1837 | 11.2741 | 0.2725 | 0.8122 | accepted |
| low_current_sweep_start_duty.csv | 15.0000 | 22 | 22 | 17 | 17 | 16.0700 | 15.9981 | 0.1501 | 0.6113 | accepted |
| low_current_sweep_start_duty.csv | 20.0000 | 22 | 22 | 17 | 17 | 20.3320 | 20.3318 | 0.0002 | 0.0009 | accepted |
| low_current_test.csv | 1.0000 | 19 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test.csv | 3.0000 | 18 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test.csv | 5.0000 | 18 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test.csv | 8.0000 | 20 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test.csv | 10.0000 | 19 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test.csv | 15.0000 | 22 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test.csv | 20.0000 | 20 | 0 | 0 | 0 |  |  |  |  | valid_dm3058_too_few |
| low_current_test_after_adaptive.csv | 5.0000 | 59 | 59 | 44 | 37 | 5.0833 | 5.0993 | 0.0449 | 0.1811 | accepted |
| low_current_test_after_adaptive.csv | 8.0000 | 20 | 20 | 15 | 15 | 8.8206 | 8.9275 | 0.2661 | 0.7703 | accepted |
| low_current_test_after_adaptive.csv | 10.0000 | 57 | 55 | 40 | 40 | 11.0940 | 11.0896 | 0.3724 | 1.2323 | accepted |

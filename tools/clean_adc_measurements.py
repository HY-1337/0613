import csv
import math
from pathlib import Path
from statistics import mean, median, stdev


ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "docs" / "adc_measurements_combined.csv"
OUTPUT = ROOT / "docs" / "adc_measurements_stable_dm3058_only.csv"
SUMMARY = ROOT / "docs" / "adc_measurements_stable_dm3058_only_summary.md"


def to_float(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return math.nan


def group_key(row):
    return row.get("source_file", ""), row.get("set_ma", "")


def median_abs_deviation(values, center):
    return median([abs(v - center) for v in values]) if values else math.nan


def clean_group(rows):
    rows = sorted(rows, key=lambda r: r["_time_s"])
    set_ma = rows[0]["_set_ma"]

    valid = [r for r in rows if math.isfinite(r["_dm3058_ma"])]
    if set_ma > 0:
        # Treat very small/negative DMM values as open-loop, disconnected, or transient readings.
        valid = [r for r in valid if r["_dm3058_ma"] > 0.02]

    if len(valid) < 4:
        return [], {
            "reason": "valid_dm3058_too_few",
            "valid_dm3058": len(valid),
            "after_settle": 0,
        }

    t_min = min(r["_time_s"] for r in valid)
    t_max = max(r["_time_s"] for r in valid)
    duration = t_max - t_min

    # Drop the initial transition after each setpoint command.
    settle_cut = t_min + min(max(2.0, duration * 0.25), 8.0)
    settled = [r for r in valid if r["_time_s"] >= settle_cut]
    if len(settled) < max(4, len(valid) // 4):
        settled = valid[len(valid) // 2 :]
    if len(settled) < 4:
        settled = valid

    dmm_values = [r["_dm3058_ma"] for r in settled]
    center = median(dmm_values)
    mad = median_abs_deviation(dmm_values, center)

    rel_tol = max(abs(set_ma) * 0.12, abs(center) * 0.12)
    tolerance = max(0.30, rel_tol, 3.0 * mad)
    stable = [r for r in settled if abs(r["_dm3058_ma"] - center) <= tolerance]

    if len(stable) >= 4:
        dmm_values = [r["_dm3058_ma"] for r in stable]
        center = median(dmm_values)
        mad = median_abs_deviation(dmm_values, center)
        tolerance = max(0.25, max(abs(set_ma), abs(center)) * 0.08, 3.0 * mad)
        stable = [r for r in stable if abs(r["_dm3058_ma"] - center) <= tolerance]

    dmm_values = [r["_dm3058_ma"] for r in stable]
    if not dmm_values:
        dmm_range = math.inf
    else:
        dmm_range = max(dmm_values) - min(dmm_values)

    range_limit = max(0.6, max(abs(set_ma), abs(center)) * 0.18)
    accepted = len(stable) >= 3 and dmm_range <= range_limit

    if not accepted:
        return [], {
            "reason": f"rejected_unstable_or_too_few range={dmm_range:.4f}",
            "valid_dm3058": len(valid),
            "after_settle": len(settled),
            "dmm_median_ma": center,
        }

    return stable, {
        "reason": "accepted",
        "valid_dm3058": len(valid),
        "after_settle": len(settled),
        "dmm_median_ma": center,
        "dmm_mean_kept_ma": mean(dmm_values),
        "dmm_sd_kept_ma": stdev(dmm_values) if len(dmm_values) > 1 else 0.0,
        "dmm_range_kept_ma": dmm_range,
    }


def main():
    with INPUT.open("r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        fieldnames = list(reader.fieldnames or [])
        rows = list(reader)

    for row in rows:
        row["_time_s"] = to_float(row.get("time_s"))
        row["_set_ma"] = to_float(row.get("set_ma"))
        row["_dm3058_ma"] = to_float(row.get("dm3058_current_ma"))

    groups = {}
    for row in rows:
        if not math.isfinite(row["_time_s"]) or not math.isfinite(row["_set_ma"]):
            continue
        groups.setdefault(group_key(row), []).append(row)

    kept = []
    summaries = []
    for (source, set_text), group_rows in sorted(groups.items()):
        stable_rows, info = clean_group(group_rows)
        for row in stable_rows:
            out_row = {name: row.get(name, "") for name in fieldnames}
            out_row["cleaning_note"] = "stable_by_dm3058_group_median"
            kept.append(out_row)
        summaries.append(
            {
                "source_file": source,
                "set_ma": to_float(set_text),
                "total": len(group_rows),
                "kept": len(stable_rows),
                **info,
            }
        )

    out_fields = fieldnames + ["cleaning_note"]
    with OUTPUT.open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=out_fields)
        writer.writeheader()
        writer.writerows(kept)

    by_source = {}
    for item in summaries:
        bucket = by_source.setdefault(
            item["source_file"], {"total": 0, "kept": 0, "groups": 0, "accepted": 0}
        )
        bucket["total"] += item["total"]
        bucket["kept"] += item["kept"]
        bucket["groups"] += 1
        if item["kept"] > 0:
            bucket["accepted"] += 1

    def fmt(value):
        return f"{value:.4f}" if isinstance(value, float) and math.isfinite(value) else ""

    lines = [
        "# 稳定 ADC/DM3058 测量数据清洗说明",
        "",
        "输出文件：`docs/adc_measurements_stable_dm3058_only.csv`",
        "",
        "清洗原则：以 DM3058 实测电流为真值，只保留每个“来源文件 + 设定电流”分组中进入稳态后的数据。STM32 的 `IMEAS/ADC1/ADC2` 只作为被校准对象，不参与真值判断。",
        "",
        "规则摘要：",
        "",
        "- 删除非数值 DM3058 读数。",
        "- 对非零设定电流，删除 DM3058 小于等于 0.02 mA 的开路、未输出或明显瞬态读数。",
        "- 每个分组删除前段过渡期：至少前 2 秒，通常取前 25%，最多按 8 秒处理。",
        "- 用剩余 DM3058 读数中位数作为稳态中心，按中位绝对偏差和相对误差阈值删除离群点。",
        "- 若清洗后样本太少或 DM3058 波动范围仍过大，则整组舍弃。",
        "",
        "## 汇总",
        "",
        "| 来源文件 | 原始行数 | 保留行数 | 分组数 | 接受分组数 |",
        "|---|---:|---:|---:|---:|",
    ]

    for source, bucket in sorted(by_source.items()):
        lines.append(
            f"| {source} | {bucket['total']} | {bucket['kept']} | {bucket['groups']} | {bucket['accepted']} |"
        )

    lines.extend(
        [
            "",
            f"总原始行数：{len(rows)}",
            "",
            f"总保留行数：{len(kept)}",
            "",
            "## 分组清洗结果",
            "",
            "| 来源文件 | 设定 mA | 原始 | 有效 DM3058 | 稳态候选 | 保留 | DM3058 中位 mA | 保留均值 mA | 保留标准差 mA | 保留极差 mA | 结果 |",
            "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|",
        ]
    )

    for item in sorted(summaries, key=lambda x: (x["source_file"], x["set_ma"])):
        lines.append(
            "| "
            f"{item['source_file']} | "
            f"{fmt(item['set_ma'])} | "
            f"{item['total']} | "
            f"{item.get('valid_dm3058', 0)} | "
            f"{item.get('after_settle', 0)} | "
            f"{item['kept']} | "
            f"{fmt(item.get('dmm_median_ma'))} | "
            f"{fmt(item.get('dmm_mean_kept_ma'))} | "
            f"{fmt(item.get('dmm_sd_kept_ma'))} | "
            f"{fmt(item.get('dmm_range_kept_ma'))} | "
            f"{item.get('reason', '')} |"
        )

    SUMMARY.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")
    print(f"input_rows={len(rows)}")
    print(f"kept_rows={len(kept)}")
    print(f"output={OUTPUT}")
    print(f"summary={SUMMARY}")


if __name__ == "__main__":
    main()

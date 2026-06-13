import csv
from collections import defaultdict
from pathlib import Path
from statistics import mean, median, stdev


ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "docs" / "adc_duty_reference_stable.csv"
OUTPUT = ROOT / "docs" / "adc_duty_reference_stable_averaged.csv"


FIELDS = [
    "set_current_ma",
    "output_duty_pct",
    "sample_count_rows",
    "actual_current_dm3058_mean_ma",
    "actual_current_dm3058_median_ma",
    "actual_current_dm3058_std_ma",
    "adc1_raw_mean",
    "adc1_raw_median",
    "adc1_voltage_mean_v",
    "stm32_reported_current_mean_ma",
    "source_files",
]


def num(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def fmt(value):
    if value is None:
        return ""
    return f"{value:.6g}"


def main():
    with INPUT.open("r", encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))

    groups = defaultdict(list)
    for row in rows:
        set_current = num(row.get("set_current_ma"))
        duty = num(row.get("output_duty_pct"))
        if set_current is None or duty is None:
            continue
        groups[(set_current, duty)].append(row)

    output = []
    for (set_current, duty), group in sorted(groups.items()):
        actual = [num(r.get("actual_current_dm3058_ma")) for r in group]
        adc1 = [num(r.get("adc1_raw")) for r in group]
        adc1v = [num(r.get("adc1_voltage_v")) for r in group]
        stm = [num(r.get("stm32_reported_current_ma")) for r in group]
        actual = [x for x in actual if x is not None]
        adc1 = [x for x in adc1 if x is not None]
        adc1v = [x for x in adc1v if x is not None]
        stm = [x for x in stm if x is not None]

        output.append(
            {
                "set_current_ma": fmt(set_current),
                "output_duty_pct": fmt(duty),
                "sample_count_rows": str(len(group)),
                "actual_current_dm3058_mean_ma": fmt(mean(actual) if actual else None),
                "actual_current_dm3058_median_ma": fmt(median(actual) if actual else None),
                "actual_current_dm3058_std_ma": fmt(stdev(actual) if len(actual) > 1 else 0.0),
                "adc1_raw_mean": fmt(mean(adc1) if adc1 else None),
                "adc1_raw_median": fmt(median(adc1) if adc1 else None),
                "adc1_voltage_mean_v": fmt(mean(adc1v) if adc1v else None),
                "stm32_reported_current_mean_ma": fmt(mean(stm) if stm else None),
                "source_files": ";".join(sorted({r.get("source_file", "") for r in group})),
            }
        )

    with OUTPUT.open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(output)

    print(f"input_rows={len(rows)}")
    print(f"output_groups={len(output)}")
    print(f"output={OUTPUT}")


if __name__ == "__main__":
    main()

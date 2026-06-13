import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "docs" / "adc_measurements_stable_dm3058_only.csv"
OUTPUT = ROOT / "docs" / "adc_duty_reference_stable.csv"


OUTPUT_FIELDS = [
    "actual_current_dm3058_ma",
    "output_duty_pct",
    "adc1_raw",
    "adc1_voltage_v",
    "adc2_raw",
    "adc2_voltage_v",
    "set_current_ma",
    "stm32_reported_current_ma",
    "source_file",
    "source_row",
    "time_s",
    "sample_count",
    "dropped_each_side",
]


def as_float_text(value):
    try:
        number = float(value)
    except (TypeError, ValueError):
        return ""
    return f"{number:.6g}"


def main():
    with INPUT.open("r", encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))

    output_rows = []
    for row in rows:
        actual = as_float_text(row.get("dm3058_current_ma"))
        duty = as_float_text(row.get("output_duty_pct"))
        adc1_raw = row.get("adc1_raw", "").strip()

        if actual == "" or duty == "" or adc1_raw == "":
            continue

        output_rows.append(
            {
                "actual_current_dm3058_ma": actual,
                "output_duty_pct": duty,
                "adc1_raw": adc1_raw,
                "adc1_voltage_v": as_float_text(row.get("adc1_voltage_v")),
                "adc2_raw": row.get("adc2_raw", "").strip(),
                "adc2_voltage_v": as_float_text(row.get("adc2_voltage_v")),
                "set_current_ma": as_float_text(row.get("set_ma")),
                "stm32_reported_current_ma": as_float_text(row.get("stm_current_ma")),
                "source_file": row.get("source_file", ""),
                "source_row": row.get("source_row", ""),
                "time_s": as_float_text(row.get("time_s")),
                "sample_count": row.get("sample_count", ""),
                "dropped_each_side": row.get("dropped_each_side", ""),
            }
        )

    output_rows.sort(
        key=lambda r: (
            float(r["actual_current_dm3058_ma"]),
            float(r["output_duty_pct"]),
            r["source_file"],
            int(r["source_row"]),
        )
    )

    with OUTPUT.open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(output_rows)

    print(f"input_rows={len(rows)}")
    print(f"output_rows={len(output_rows)}")
    print(f"output={OUTPUT}")


if __name__ == "__main__":
    main()

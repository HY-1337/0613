import argparse
import re
import time

import serial


ADC_RE = re.compile(
    r"Set:\s*(?P<set>\d+)mA\s*\|\s*ADC1:\s*(?P<adc1>\d+),\s*(?P<v1>\d+\.\d+)V\s*\|\s*ADC2:\s*(?P<adc2>\d+),\s*(?P<v2>\d+\.\d+)V"
)


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def send_command(port: serial.Serial, command: str) -> None:
    port.write((command + "\r\n").encode("utf-8"))
    print(f"TX {command}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Conservative PC-side duty controller for PWMADC")
    parser.add_argument("--port", default="COM11")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--rsense", type=float, required=True, help="Sense resistor in ohms")
    parser.add_argument("--target-ma", type=float, required=True, help="Target current in mA")
    parser.add_argument("--max-duty", type=float, default=30.0, help="Software duty limit in percent")
    parser.add_argument("--step-limit", type=float, default=0.5, help="Max duty change per update in percent")
    parser.add_argument("--kp", type=float, default=0.02, help="PC-side proportional gain: duty_percent per mA error")
    parser.add_argument("--period", type=float, default=0.5, help="Control update period in seconds")
    parser.add_argument("--duration", type=float, default=30.0, help="Run time in seconds")
    args = parser.parse_args()

    duty = 0.0
    next_update = time.monotonic()
    end_time = time.monotonic() + args.duration

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        print(f"Opened {args.port} @ {args.baud}")
        print("Press Ctrl+C to stop. Script sends STOP before exit.")
        send_command(port, f"DUTYMAX={args.max-duty:.1f}")
        send_command(port, "DUTY=0.0")
        time.sleep(0.2)

        try:
            while time.monotonic() < end_time:
                line = port.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue

                match = ADC_RE.search(line)
                if not match:
                    print(f"RX {line}")
                    continue

                v_adc1 = float(match.group("v1"))
                measured_ma = (v_adc1 / args.rsense) * 1000.0
                error_ma = args.target_ma - measured_ma

                now = time.monotonic()
                if now >= next_update:
                    duty_delta = clamp(args.kp * error_ma, -args.step_limit, args.step_limit)
                    duty = clamp(duty + duty_delta, 0.0, args.max_duty)
                    send_command(port, f"DUTY={duty:.1f}")
                    next_update = now + args.period

                print(
                    f"RX target={args.target_ma:.1f}mA measured={measured_ma:.2f}mA "
                    f"error={error_ma:.2f}mA duty={duty:.1f}% raw='{line}'"
                )
        finally:
            send_command(port, "DUTY=0.0")
            send_command(port, "STOP")


if __name__ == "__main__":
    main()

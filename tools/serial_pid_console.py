import argparse
import threading
import time

try:
    import serial
except ImportError as exc:
    raise SystemExit("pyserial is required: pip install pyserial") from exc


def reader(port: serial.Serial, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            line = port.readline()
        except serial.SerialException as exc:
            print(f"\n[serial error] {exc}")
            stop_event.set()
            return

        if line:
            text = line.decode("utf-8", errors="replace").rstrip()
            print(f"\nRX {text}")
        else:
            time.sleep(0.01)


def main() -> None:
    parser = argparse.ArgumentParser(description="Serial PID console for PWMADC")
    parser.add_argument("--port", default="COM11")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    stop_event = threading.Event()
    with serial.Serial(args.port, args.baud, timeout=0.1) as port:
        print(f"Opened {args.port} @ {args.baud}")
        print("Commands: KP=0.100 KI=0.010 KD=0.000 SET=100 START STOP STATUS HELP")
        print("Type q and press Enter to quit.")

        thread = threading.Thread(target=reader, args=(port, stop_event), daemon=True)
        thread.start()

        while not stop_event.is_set():
            try:
                command = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                break

            if command.lower() in {"q", "quit", "exit"}:
                break
            if not command:
                continue

            port.write((command + "\r\n").encode("utf-8"))

    stop_event.set()


if __name__ == "__main__":
    main()

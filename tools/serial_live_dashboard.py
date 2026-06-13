import argparse
import queue
import re
import threading
import time
import tkinter as tk
from tkinter import ttk

import serial


LINE_RE = re.compile(
    r"Set:\s*(?P<set>\d+)mA\s*\|\s*ADC1:\s*(?P<adc1>\d+),\s*(?P<v1>\d+\.\d+)V\s*\|\s*"
    r"ADC2:\s*(?P<adc2>\d+),\s*(?P<v2>\d+\.\d+)V\s*\|\s*DUTY:(?P<duty>\d+\.\d+)%\s*LIM:(?P<limit>\d+\.\d+)%"
)


class SerialReader(threading.Thread):
    def __init__(self, port_name, baud, output_queue, stop_event):
        super().__init__(daemon=True)
        self.port_name = port_name
        self.baud = baud
        self.output_queue = output_queue
        self.stop_event = stop_event
        self.serial_port = None

    def run(self):
        try:
            with serial.Serial(self.port_name, self.baud, timeout=0.2) as port:
                self.serial_port = port
                self.output_queue.put(("status", f"已连接 {self.port_name} @ {self.baud}"))
                while not self.stop_event.is_set():
                    line = port.readline().decode("utf-8", errors="replace").strip()
                    if line:
                        self.output_queue.put(("line", line))
        except serial.SerialException as exc:
            self.output_queue.put(("status", f"串口错误: {exc}"))
        finally:
            self.serial_port = None

    def send(self, command):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write((command + "\r\n").encode("utf-8"))
            self.output_queue.put(("status", f"发送: {command}"))


class Dashboard:
    def __init__(self, root, args):
        self.root = root
        self.args = args
        self.queue = queue.Queue()
        self.stop_event = threading.Event()
        self.reader = SerialReader(args.port, args.baud, self.queue, self.stop_event)
        self.history = []
        self.max_points = 160

        root.title("PWMADC 串口实时监视")
        root.geometry("920x560")
        root.minsize(760, 480)

        self.set_var = tk.StringVar(value="-- mA")
        self.current_var = tk.StringVar(value="-- mA")
        self.duty_var = tk.StringVar(value="-- %")
        self.limit_var = tk.StringVar(value="-- %")
        self.adc1_var = tk.StringVar(value="-- V")
        self.adc2_var = tk.StringVar(value="-- V")
        self.status_var = tk.StringVar(value="等待连接")
        self.command_var = tk.StringVar(value="STATUS")

        self.build_ui()
        self.reader.start()
        self.root.after(80, self.poll_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def build_ui(self):
        top = ttk.Frame(self.root, padding=14)
        top.pack(fill="both", expand=True)

        metric_frame = ttk.Frame(top)
        metric_frame.pack(fill="x")

        self.metric(metric_frame, "当前占空比", self.duty_var, 0)
        self.metric(metric_frame, "采样电流", self.current_var, 1)
        self.metric(metric_frame, "设定电流", self.set_var, 2)
        self.metric(metric_frame, "占空比上限", self.limit_var, 3)

        adc_frame = ttk.Frame(top)
        adc_frame.pack(fill="x", pady=(10, 0))
        self.small_metric(adc_frame, "ADC1 采样电压", self.adc1_var, 0)
        self.small_metric(adc_frame, "ADC2 电压", self.adc2_var, 1)
        ttk.Label(adc_frame, textvariable=self.status_var).grid(row=0, column=2, sticky="e", padx=12)
        adc_frame.columnconfigure(2, weight=1)

        self.canvas = tk.Canvas(top, height=250, bg="#111827", highlightthickness=0)
        self.canvas.pack(fill="both", expand=True, pady=14)

        bottom = ttk.Frame(top)
        bottom.pack(fill="x")
        ttk.Entry(bottom, textvariable=self.command_var).pack(side="left", fill="x", expand=True)
        ttk.Button(bottom, text="发送命令", command=self.send_command).pack(side="left", padx=(8, 0))
        ttk.Button(bottom, text="DUTY=0", command=lambda: self.send_text("DUTY=0.0")).pack(side="left", padx=(8, 0))
        ttk.Button(bottom, text="STATUS", command=lambda: self.send_text("STATUS")).pack(side="left", padx=(8, 0))

        self.log = tk.Text(top, height=7, wrap="none")
        self.log.pack(fill="x", pady=(12, 0))

    def metric(self, parent, title, value_var, column):
        frame = ttk.LabelFrame(parent, text=title, padding=10)
        frame.grid(row=0, column=column, sticky="nsew", padx=5)
        ttk.Label(frame, textvariable=value_var, font=("Segoe UI", 24, "bold")).pack(anchor="center")
        parent.columnconfigure(column, weight=1)

    def small_metric(self, parent, title, value_var, column):
        frame = ttk.LabelFrame(parent, text=title, padding=8)
        frame.grid(row=0, column=column, sticky="nsew", padx=5)
        ttk.Label(frame, textvariable=value_var, font=("Segoe UI", 14, "bold")).pack(anchor="center")
        parent.columnconfigure(column, weight=1)

    def poll_queue(self):
        while True:
            try:
                kind, payload = self.queue.get_nowait()
            except queue.Empty:
                break

            if kind == "status":
                self.status_var.set(payload)
                self.append_log(payload)
            elif kind == "line":
                self.handle_line(payload)

        self.draw_plot()
        self.root.after(80, self.poll_queue)

    def handle_line(self, line):
        self.append_log(line)
        match = LINE_RE.search(line)
        if not match:
            return

        set_ma = float(match.group("set"))
        v1 = float(match.group("v1"))
        v2 = float(match.group("v2"))
        duty = float(match.group("duty"))
        limit = float(match.group("limit"))
        current_ma = v1 / self.args.rsense * 1000.0

        self.set_var.set(f"{set_ma:.0f} mA")
        self.current_var.set(f"{current_ma:.1f} mA")
        self.duty_var.set(f"{duty:.1f} %")
        self.limit_var.set(f"{limit:.1f} %")
        self.adc1_var.set(f"{v1:.3f} V")
        self.adc2_var.set(f"{v2:.3f} V")

        self.history.append((time.time(), duty, current_ma))
        if len(self.history) > self.max_points:
            self.history = self.history[-self.max_points :]

    def draw_plot(self):
        self.canvas.delete("all")
        width = max(self.canvas.winfo_width(), 10)
        height = max(self.canvas.winfo_height(), 10)
        margin = 34

        self.canvas.create_text(12, 12, anchor="nw", fill="#e5e7eb", text="白色: 占空比(%)    青色: 电流(mA)")
        self.canvas.create_line(margin, height - margin, width - margin, height - margin, fill="#374151")
        self.canvas.create_line(margin, margin, margin, height - margin, fill="#374151")

        if len(self.history) < 2:
            return

        duties = [item[1] for item in self.history]
        currents = [item[2] for item in self.history]
        duty_max = max(30.0, max(duties) * 1.2)
        current_max = max(100.0, max(currents) * 1.2)

        self.plot_series(duties, duty_max, "#f9fafb", width, height, margin)
        self.plot_series(currents, current_max, "#22d3ee", width, height, margin)

        self.canvas.create_text(width - margin, height - margin + 16, anchor="e", fill="#9ca3af", text="最近数据")
        self.canvas.create_text(margin - 6, margin, anchor="e", fill="#9ca3af", text=f"{current_max:.0f}mA")

    def plot_series(self, values, vmax, color, width, height, margin):
        points = []
        plot_w = width - margin * 2
        plot_h = height - margin * 2
        count = max(len(values) - 1, 1)
        for index, value in enumerate(values):
            x = margin + plot_w * index / count
            y = height - margin - plot_h * min(value / vmax, 1.0)
            points.extend((x, y))
        if len(points) >= 4:
            self.canvas.create_line(*points, fill=color, width=2, smooth=True)

    def append_log(self, text):
        self.log.insert("end", text + "\n")
        self.log.see("end")

    def send_text(self, command):
        self.reader.send(command)

    def send_command(self):
        command = self.command_var.get().strip()
        if command:
            self.send_text(command)

    def close(self):
        self.stop_event.set()
        self.root.after(150, self.root.destroy)


def main():
    parser = argparse.ArgumentParser(description="Realtime GUI dashboard for PWMADC serial output")
    parser.add_argument("--port", default="COM11")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--rsense", type=float, default=5.17)
    args = parser.parse_args()

    root = tk.Tk()
    Dashboard(root, args)
    root.mainloop()


if __name__ == "__main__":
    main()

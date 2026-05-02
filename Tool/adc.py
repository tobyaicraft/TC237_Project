"""
TC237 ADC Monitor v1 — Single channel ADC oscilloscope
- Serial: PL2303 USB-TTL (115200, 8N1)
- Protocol: "<integer>\r\n"  (raw ADC value 0-4095)
"""

import serial
import serial.tools.list_ports
import threading
import time
from collections import deque

import tkinter as tk
from tkinter import ttk, messagebox
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.animation as animation

# --- Configuration ---
BAUD_RATE    = 115200
ADC_MAX      = 4095
VAREF        = 5.0
HISTORY_SIZE = 200

# --- Color Palette ---
BG_COLOR    = "#111827"   # dark navy
PANEL_COLOR = "#1a2035"   # slightly lighter navy
CHART_BG    = "#161d30"
GRID_COLOR  = "#2a3450"
FG_COLOR    = "#c8d6f0"
ACCENT      = "#5b9bd5"   # blue line / highlight
DIM_COLOR   = "#4a5a7a"
WARN_COLOR  = "#f5a623"


class AdcMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("TC237 ADC Monitor")
        self.root.configure(bg=BG_COLOR)
        self.root.geometry("1200x780")
        self.root.minsize(900, 600)

        self.serial_port = None
        self.running     = False

        self.adc_value   = 0
        self.adc_min     = 0
        self.adc_max     = 0
        self.history     = deque([0] * HISTORY_SIZE, maxlen=HISTORY_SIZE)

        self._build_ui()
        self._start_animation()

    # ================================================================
    # UI construction
    # ================================================================
    def _build_ui(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Dark.TFrame",     background=BG_COLOR)
        style.configure("Panel.TFrame",    background=PANEL_COLOR)
        style.configure("Dark.TLabel",     background=BG_COLOR,    foreground=FG_COLOR,
                        font=("Consolas", 10))
        style.configure("Panel.TLabel",    background=PANEL_COLOR, foreground=FG_COLOR,
                        font=("Consolas", 10))
        style.configure("Title.TLabel",    background=BG_COLOR,    foreground=ACCENT,
                        font=("Consolas", 13, "bold"))
        style.configure("PanelHead.TLabel",background=PANEL_COLOR, foreground=ACCENT,
                        font=("Consolas", 11, "bold"))
        style.configure("BigAdc.TLabel",   background=PANEL_COLOR, foreground="#ffffff",
                        font=("Consolas", 52, "bold"))
        style.configure("Sub.TLabel",      background=PANEL_COLOR, foreground=DIM_COLOR,
                        font=("Consolas", 11))
        style.configure("Stat.TLabel",     background=PANEL_COLOR, foreground=FG_COLOR,
                        font=("Consolas", 18, "bold"))
        style.configure("StatSub.TLabel",  background=PANEL_COLOR, foreground=DIM_COLOR,
                        font=("Consolas", 10))
        style.configure("Min.TLabel",      background=PANEL_COLOR, foreground=DIM_COLOR,
                        font=("Consolas", 11))
        style.configure("Max.TLabel",      background=PANEL_COLOR, foreground=WARN_COLOR,
                        font=("Consolas", 11, "bold"))

        # ---- Top bar ----
        top = ttk.Frame(self.root, style="Dark.TFrame")
        top.pack(fill=tk.X, padx=0, pady=0)
        top.configure(style="Dark.TFrame")

        ttk.Label(top, text="TC237 ADC Monitor", style="Title.TLabel").pack(
            side=tk.LEFT, padx=15, pady=8)

        # Connection controls (right side)
        self.btn_connect = ttk.Button(top, text="Connect",
                                      command=self._toggle_connect)
        self.btn_connect.pack(side=tk.RIGHT, padx=(5, 15), pady=6)

        ttk.Button(top, text="Refresh",
                   command=self._refresh_ports).pack(side=tk.RIGHT, padx=2, pady=6)

        self.combo_port = ttk.Combobox(top, width=28, state="readonly")
        self.combo_port.pack(side=tk.RIGHT, padx=2, pady=6)

        ttk.Label(top, text="COM Port:", style="Dark.TLabel").pack(
            side=tk.RIGHT, padx=(10, 2))

        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(top, textvariable=self.status_var, style="Dark.TLabel").pack(
            side=tk.RIGHT, padx=(10, 5))

        # ---- Main area ----
        main = ttk.Frame(self.root, style="Dark.TFrame")
        main.pack(fill=tk.BOTH, expand=True, padx=0, pady=0)

        # Chart (left, fills most of window)
        chart_frame = ttk.Frame(main, style="Dark.TFrame")
        chart_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True,
                         padx=(10, 5), pady=(5, 10))

        self.fig = Figure(figsize=(8, 5), dpi=100, facecolor=CHART_BG)
        self.ax  = self.fig.add_subplot(111)
        self._setup_chart()

        self.canvas = FigureCanvasTkAgg(self.fig, master=chart_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Right panel
        right = tk.Frame(main, bg=PANEL_COLOR, width=220)
        right.pack(side=tk.RIGHT, fill=tk.Y, padx=(0, 10), pady=(5, 10))
        right.pack_propagate(False)

        self._build_right_panel(right)
        self._refresh_ports()

    def _build_right_panel(self, parent):
        pad = {"padx": 20, "pady": 0}

        tk.Label(parent, text="ADC Value", bg=PANEL_COLOR,
                 fg=ACCENT, font=("Consolas", 11, "bold")).pack(
            pady=(20, 0))

        self.lbl_adc = tk.Label(parent, text="0", bg=PANEL_COLOR,
                                fg="#ffffff", font=("Consolas", 52, "bold"))
        self.lbl_adc.pack(pady=(2, 0))

        tk.Label(parent, text="/ 4095", bg=PANEL_COLOR,
                 fg=DIM_COLOR, font=("Consolas", 11)).pack(pady=(0, 10))

        # Progress bar (custom canvas)
        self.bar_canvas = tk.Canvas(parent, height=18, bg=PANEL_COLOR,
                                    highlightthickness=0)
        self.bar_canvas.pack(fill=tk.X, padx=18, pady=(0, 18))

        # Voltage
        tk.Label(parent, text="Voltage", bg=PANEL_COLOR,
                 fg=DIM_COLOR, font=("Consolas", 10)).pack()
        self.lbl_volt = tk.Label(parent, text="0.000 V", bg=PANEL_COLOR,
                                 fg=FG_COLOR, font=("Consolas", 18, "bold"))
        self.lbl_volt.pack(pady=(0, 10))

        # Percentage
        tk.Label(parent, text="Percentage", bg=PANEL_COLOR,
                 fg=DIM_COLOR, font=("Consolas", 10)).pack()
        self.lbl_pct = tk.Label(parent, text="0.0 %", bg=PANEL_COLOR,
                                fg=FG_COLOR, font=("Consolas", 18, "bold"))
        self.lbl_pct.pack(pady=(0, 18))

        # Separator
        tk.Frame(parent, bg=GRID_COLOR, height=1).pack(
            fill=tk.X, padx=15, pady=(0, 10))

        # Min / Max row
        minmax = tk.Frame(parent, bg=PANEL_COLOR)
        minmax.pack(fill=tk.X, padx=18)

        tk.Label(minmax, text="MIN", bg=PANEL_COLOR,
                 fg=DIM_COLOR, font=("Consolas", 10)).pack(side=tk.LEFT)

        self.lbl_min = tk.Label(minmax, text="0", bg=PANEL_COLOR,
                                fg=DIM_COLOR, font=("Consolas", 11, "bold"))
        self.lbl_min.pack(side=tk.LEFT, padx=(4, 0))

        self.lbl_max = tk.Label(minmax, text="0", bg=PANEL_COLOR,
                                fg=WARN_COLOR, font=("Consolas", 11, "bold"))
        self.lbl_max.pack(side=tk.RIGHT, padx=(0, 4))

        tk.Label(minmax, text="MAX", bg=PANEL_COLOR,
                 fg=DIM_COLOR, font=("Consolas", 10)).pack(side=tk.RIGHT)

        # Reset Stats button
        ttk.Button(parent, text="Reset Stats",
                   command=self._reset_stats).pack(pady=(12, 0))

    def _setup_chart(self):
        self.ax.set_facecolor(CHART_BG)
        self.ax.set_xlim(0, HISTORY_SIZE)
        self.ax.set_ylim(0, ADC_MAX + 50)
        self.ax.set_ylabel("ADC Value", color=FG_COLOR, fontsize=10)
        self.ax.set_xlabel("Samples",   color=FG_COLOR, fontsize=10)
        self.ax.tick_params(colors=FG_COLOR, labelsize=9)
        self.ax.grid(True, color=GRID_COLOR, alpha=0.7, linestyle="-", linewidth=0.7)
        for spine in self.ax.spines.values():
            spine.set_color(GRID_COLOR)

        self.line, = self.ax.plot([], [], color=ACCENT, linewidth=1.5, alpha=0.95)

        # Fill area under the line
        self.fill = self.ax.fill_between(
            range(HISTORY_SIZE), [0] * HISTORY_SIZE,
            color=ACCENT, alpha=0.08)

        self.fig.tight_layout(pad=1.5)

    def _start_animation(self):
        self.ani = animation.FuncAnimation(
            self.fig, self._update_chart,
            interval=50, blit=False, cache_frame_data=False)

    def _update_chart(self, frame):
        self.history.append(self.adc_value)

        data = list(self.history)
        x    = list(range(HISTORY_SIZE))

        self.line.set_data(x, data)

        # Redraw fill area
        self.fill.remove()
        self.fill = self.ax.fill_between(x, data, color=ACCENT, alpha=0.08)

        # Right panel labels
        adc = self.adc_value
        volt = adc / ADC_MAX * VAREF
        pct  = adc / ADC_MAX * 100.0

        self.lbl_adc.configure(text=str(adc))
        self.lbl_volt.configure(text=f"{volt:.3f} V")
        self.lbl_pct.configure(text=f"{pct:.1f} %")
        self.lbl_min.configure(text=str(self.adc_min))
        self.lbl_max.configure(text=str(self.adc_max))

        # Progress bar
        self._draw_bar(adc)

        self.canvas.draw_idle()
        return [self.line]

    def _draw_bar(self, adc):
        c = self.bar_canvas
        c.delete("all")
        w = c.winfo_width()
        h = c.winfo_height()
        if w <= 1:
            return
        ratio  = adc / ADC_MAX
        bar_w  = int(w * ratio)
        rest_x = bar_w

        c.create_rectangle(0, 0, w, h, fill=DIM_COLOR, outline="")
        if bar_w > 0:
            c.create_rectangle(0, 0, bar_w, h, fill=ACCENT, outline="")

    # ================================================================
    # Serial
    # ================================================================
    def _refresh_ports(self):
        ports     = serial.tools.list_ports.comports()
        port_list = [f"{p.device} - {p.description}" for p in ports]
        self.combo_port["values"] = port_list
        if port_list:
            for i, desc in enumerate(port_list):
                if "PL2303" in desc or "Prolific" in desc:
                    self.combo_port.current(i)
                    return
            self.combo_port.current(0)

    def _toggle_connect(self):
        if self.running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        sel = self.combo_port.get()
        if not sel:
            messagebox.showwarning("Warning", "Select a COM port first.")
            return
        port_name = sel.split(" - ")[0].strip()
        try:
            self.serial_port = serial.Serial(port_name, BAUD_RATE, timeout=0.1)
            self.running = True
            self.btn_connect.configure(text="Disconnect")
            self.status_var.set(f"Connected: {port_name}")
            self.read_thread = threading.Thread(target=self._read_serial, daemon=True)
            self.read_thread.start()
        except Exception as e:
            messagebox.showerror("Error", f"Cannot open {port_name}\n{e}")

    def _disconnect(self):
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.btn_connect.configure(text="Connect")
        self.status_var.set("Disconnected")

    def _read_serial(self):
        buf = ""
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting:
                    raw  = self.serial_port.read(self.serial_port.in_waiting)
                    buf += raw.decode("ascii", errors="ignore")

                    while "\n" in buf:
                        line, buf = buf.split("\n", 1)
                        line = line.strip()
                        try:
                            val = int(line)
                            val = max(0, min(ADC_MAX, val))
                            self.adc_value = val
                            if val > self.adc_max:
                                self.adc_max = val
                            if val < self.adc_min or self.adc_min == 0:
                                self.adc_min = val
                        except ValueError:
                            pass
                else:
                    time.sleep(0.005)
            except Exception:
                break

    def _reset_stats(self):
        self.adc_min = 0
        self.adc_max = 0

    def on_close(self):
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.root.destroy()


def main():
    root = tk.Tk()
    app  = AdcMonitorApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()

"""
TC237 ADC Oscilloscope  v2
──────────────────────────────────────────────────────────────────────────────
TC237 VADC AN0 (12-bit, 0~4095) → UART 115200 bps → PC

Improvements over v1
  · Phosphor-glow waveform  (3 layered lines for halo effect)
  · FFT spectrum panel       (Hanning window, 0 ~ 50 Hz)
  · Arc gauge                (speedometer-style, color-coded green/yellow/red)
  · Stats  : NOW / MIN / MAX / Vpp / RMS / Freq
  · Time window selector     (1 s / 2 s / 5 s / 10 s)
  · Pause / Resume  ·  Reset Stats
  · Animated status LED
  · Measured Rx data-rate display
  · Auto-detect USB-TTL adapters (PL2303 / CH340 / FTDI / CP210x)
  · VREF fixed to 3.3 V  (was 5.0 V in v1)
"""

import math
import time
import threading
from collections import deque

import numpy as np
import serial
import serial.tools.list_ports

import tkinter as tk
from tkinter import ttk, messagebox

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.ticker as mticker
import matplotlib.gridspec as gridspec
import matplotlib.animation as animation
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ── Hardware constants ─────────────────────────────────────────────────────────
BAUD_RATE  = 115200
ADC_MAX    = 4095
VREF       = 3.3        # TC237 VADC reference voltage [V]
SAMPLE_HZ  = 100        # TC237 transmits every 10 ms

# ── Phosphor-green dark theme ──────────────────────────────────────────────────
BG         = "#0a0e1a"
PANEL_BG   = "#0c1120"
CHART_BG   = "#07090f"
GRID_MAJ   = "#16202e"
GRID_MIN   = "#0e1520"
PHOS       = "#00ff88"   # bright phosphor green
PHOS_DIM   = "#001f0d"   # fill colour under waveform
TRIG_COL   = "#ffaa00"   # amber trigger line
TEXT_HI    = "#cce8ff"
TEXT_DIM   = "#3a5070"
WARN       = "#ff4422"
YELLOW     = "#ffcc00"

USB_KW = ("PL2303", "Prolific", "CH340", "FTDI", "Silicon", "CP210", "USB Serial")


class OscilloscopeApp:
    TIME_WINDOWS = [("1 s", 1), ("2 s", 2), ("5 s", 5), ("10 s", 10)]
    N_FFT_BARS   = 40

    def __init__(self, root):
        self.root = root
        self.root.title("TC237  ADC Oscilloscope  v2")
        self.root.configure(bg=BG)
        self.root.geometry("1300x840")
        self.root.minsize(960, 640)

        self.serial_port = None
        self.running     = False
        self.paused      = False

        self.buf_volt = deque([0.0] * (SAMPLE_HZ * 10), maxlen=SAMPLE_HZ * 10)

        self.adc_now  = 0
        self.adc_min  = ADC_MAX
        self.adc_max  = 0

        self._win_sec = 2
        self._win_pts = SAMPLE_HZ * 2

        self._rx_count = 0
        self._rx_t0    = time.time()
        self.rx_hz     = 0.0

        self._build_ui()
        self._start_animation()

    # ─────────────────────────────────────────────────────────────────────────
    # UI
    # ─────────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        self._apply_ttk_styles()
        self._build_topbar()
        main = tk.Frame(self.root, bg=BG)
        main.pack(fill=tk.BOTH, expand=True)
        self._build_chart(main)
        self._build_right_panel(main)
        self._refresh_ports()

    def _apply_ttk_styles(self):
        s = ttk.Style()
        s.theme_use("clam")
        s.configure("TCombobox",
                    fieldbackground="#0d1525", background="#0d1525",
                    foreground=TEXT_HI, selectbackground="#1a2840",
                    selectforeground=PHOS)
        s.configure("TButton",
                    background="#1a2840", foreground=TEXT_HI,
                    relief="flat", font=("Consolas", 9))
        s.map("TButton", background=[("active", "#2a3850")])

    def _build_topbar(self):
        bar = tk.Frame(self.root, bg="#060810", height=48)
        bar.pack(fill=tk.X)
        bar.pack_propagate(False)

        tk.Label(bar, text="TC237  ADC Oscilloscope  v2",
                 bg="#060810", fg=PHOS,
                 font=("Consolas", 14, "bold")).pack(side=tk.LEFT, padx=16, pady=10)

        # Status LED
        self._led_cv = tk.Canvas(bar, width=14, height=14,
                                  bg="#060810", highlightthickness=0)
        self._led_cv.pack(side=tk.LEFT, padx=(8, 3))
        self._led = self._led_cv.create_oval(2, 2, 12, 12, fill=TEXT_DIM, outline="")

        self._status_var = tk.StringVar(value="Disconnected")
        tk.Label(bar, textvariable=self._status_var,
                 bg="#060810", fg=TEXT_DIM,
                 font=("Consolas", 10)).pack(side=tk.LEFT, padx=(2, 12))

        # Right: connect controls
        self._btn_conn = tk.Button(
            bar, text="Connect", width=11,
            bg="#162035", fg=TEXT_HI, relief=tk.FLAT, cursor="hand2",
            activebackground="#1e3050", activeforeground=PHOS,
            font=("Consolas", 10, "bold"),
            command=self._toggle_connect)
        self._btn_conn.pack(side=tk.RIGHT, padx=(4, 14), pady=8)

        ttk.Button(bar, text="Refresh",
                   command=self._refresh_ports).pack(side=tk.RIGHT, padx=3, pady=8)

        self._combo = ttk.Combobox(bar, width=34, state="readonly",
                                    font=("Consolas", 9))
        self._combo.pack(side=tk.RIGHT, padx=4, pady=8)

        tk.Label(bar, text="Port:", bg="#060810", fg=TEXT_DIM,
                 font=("Consolas", 10)).pack(side=tk.RIGHT, padx=(10, 3))

    def _build_chart(self, parent):
        frame = tk.Frame(parent, bg=BG)
        frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(8, 4), pady=(6, 8))

        self.fig = Figure(figsize=(9.5, 6.2), dpi=100, facecolor=BG)
        gs = gridspec.GridSpec(
            2, 1, figure=self.fig,
            height_ratios=[3.2, 1],
            hspace=0.06, top=0.96, bottom=0.09,
            left=0.07, right=0.995)

        self.ax_osc = self.fig.add_subplot(gs[0])
        self.ax_fft = self.fig.add_subplot(gs[1])
        self._setup_osc_axes()
        self._setup_fft_axes()

        self.canvas = FigureCanvasTkAgg(self.fig, master=frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def _setup_osc_axes(self):
        ax = self.ax_osc
        ax.set_facecolor(CHART_BG)
        ax.set_ylim(-0.05, VREF + 0.15)
        ax.set_xlim(0, self._win_sec * 1000)
        ax.set_ylabel("Voltage (V)", color=TEXT_DIM, fontsize=9, labelpad=4)
        ax.tick_params(axis="both", colors=TEXT_DIM, labelsize=8, length=3)
        ax.xaxis.set_ticklabels([])

        # Oscilloscope-style grid
        ax.set_yticks([0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.3])
        ax.yaxis.set_minor_locator(mticker.MultipleLocator(0.1))
        ax.xaxis.set_minor_locator(mticker.AutoMinorLocator(5))
        ax.grid(True, which="major", color=GRID_MAJ, linewidth=0.8)
        ax.grid(True, which="minor", color=GRID_MIN, linewidth=0.4)
        for sp in ax.spines.values():
            sp.set_color(GRID_MAJ)

        # Reference lines
        ax.axhline(VREF,        color=WARN,     lw=0.6, ls="--", alpha=0.55)
        ax.axhline(VREF / 2.0,  color=GRID_MAJ, lw=0.7, ls="--", alpha=0.9)
        ax.axhline(0.0,         color=GRID_MAJ, lw=0.7, ls="--", alpha=0.9)
        ax.text(8, VREF + 0.05,       f"VREF {VREF} V",
                color=WARN, fontsize=7, alpha=0.65)
        ax.text(8, VREF / 2.0 + 0.05, f"{VREF/2:.2f} V",
                color=TEXT_DIM, fontsize=7, alpha=0.65)

        # Trigger line (amber, horizontal)
        ax.axhline(VREF / 2.0, color=TRIG_COL, lw=0.9, ls="-", alpha=0.40)

        # Waveform: 3 layers → phosphor glow effect
        x0, y0 = [0.0], [0.0]
        self._ln_g2, = ax.plot(x0, y0, color=PHOS, lw=9,   alpha=0.025)
        self._ln_g1, = ax.plot(x0, y0, color=PHOS, lw=3.5, alpha=0.10)
        self._ln_m,  = ax.plot(x0, y0, color=PHOS, lw=1.3, alpha=0.95)
        self._fill   = ax.fill_between(x0, y0, color=PHOS_DIM, alpha=0.0)

    def _setup_fft_axes(self):
        ax = self.ax_fft
        ax.set_facecolor(CHART_BG)
        ax.set_xlabel("Frequency (Hz)", color=TEXT_DIM, fontsize=9, labelpad=3)
        ax.set_ylabel("dBFS", color=TEXT_DIM, fontsize=9, labelpad=4)
        ax.tick_params(axis="both", colors=TEXT_DIM, labelsize=8, length=3)
        ax.set_xlim(0, SAMPLE_HZ / 2)
        ax.set_ylim(-80, 0)
        ax.grid(True, color=GRID_MAJ, linewidth=0.5, alpha=0.8)
        for sp in ax.spines.values():
            sp.set_color(GRID_MAJ)

        n   = self.N_FFT_BARS
        bw  = (SAMPLE_HZ / 2) / n
        bar_x  = np.arange(n) * bw + bw / 2
        colors = [self._fft_color(i / (n - 1)) for i in range(n)]
        self._fft_bars = ax.bar(bar_x, [0] * n,
                                 width=bw * 0.72, bottom=-80,
                                 color=colors, linewidth=0, alpha=0.82)

    @staticmethod
    def _fft_color(t):
        """Blue (t=0) → cyan (t=0.5) → orange (t=1)."""
        r = int(34  + (255 - 34)  * max(0.0, (t - 0.5) / 0.5))
        g = int(180 - 60 * t)
        b = int(255 * max(0.0, 1.0 - t * 2))
        return f"#{r:02x}{g:02x}{b:02x}"

    def _build_right_panel(self, parent):
        rp = tk.Frame(parent, bg=PANEL_BG, width=240)
        rp.pack(side=tk.RIGHT, fill=tk.Y, padx=(0, 8), pady=(6, 8))
        rp.pack_propagate(False)

        # Arc gauge
        self._gauge = tk.Canvas(rp, width=222, height=156,
                                 bg=PANEL_BG, highlightthickness=0)
        self._gauge.pack(pady=(14, 0))
        self._draw_gauge(0.0)

        # Digital readout
        tk.Label(rp, text="ADC  Raw", bg=PANEL_BG, fg=TEXT_DIM,
                 font=("Consolas", 9)).pack(pady=(6, 0))
        self._lbl_adc = tk.Label(rp, text="0000", bg=PANEL_BG, fg=PHOS,
                                  font=("Consolas", 38, "bold"))
        self._lbl_adc.pack()
        tk.Label(rp, text="/ 4095", bg=PANEL_BG, fg=TEXT_DIM,
                 font=("Consolas", 9)).pack(pady=(0, 5))

        # Progress bar
        self._bar_cv = tk.Canvas(rp, height=10, bg=PANEL_BG, highlightthickness=0)
        self._bar_cv.pack(fill=tk.X, padx=16, pady=(0, 10))

        # Stats
        tk.Frame(rp, bg=GRID_MAJ, height=1).pack(fill=tk.X, padx=12, pady=(2, 8))
        sf = tk.Frame(rp, bg=PANEL_BG)
        sf.pack(fill=tk.X, padx=18)

        self._stat = {}
        for label, key, fg, size, bold in [
            ("NOW",  "now",  TEXT_HI,  11, True),
            ("MIN",  "min",  TEXT_DIM, 10, False),
            ("MAX",  "max",  WARN,     10, True),
            ("Vpp",  "vpp",  TEXT_HI,  10, False),
            ("RMS",  "rms",  TEXT_DIM, 10, False),
            ("Freq", "freq", PHOS,     10, False),
        ]:
            row = tk.Frame(sf, bg=PANEL_BG)
            row.pack(fill=tk.X, pady=1)
            tk.Label(row, text=f"{label:<5}", bg=PANEL_BG, fg=TEXT_DIM,
                     font=("Consolas", 9)).pack(side=tk.LEFT)
            font = ("Consolas", size, "bold") if bold else ("Consolas", size)
            lv = tk.Label(row, text="---", bg=PANEL_BG, fg=fg, font=font)
            lv.pack(side=tk.RIGHT)
            self._stat[key] = lv

        # Time window
        tk.Frame(rp, bg=GRID_MAJ, height=1).pack(fill=tk.X, padx=12, pady=(10, 8))
        tk.Label(rp, text="Time Window", bg=PANEL_BG, fg=TEXT_DIM,
                 font=("Consolas", 9)).pack()
        win_row = tk.Frame(rp, bg=PANEL_BG)
        win_row.pack(pady=(4, 6))
        self._win_btns = {}
        for label, secs in self.TIME_WINDOWS:
            active = (secs == self._win_sec)
            btn = tk.Button(
                win_row, text=label, width=4,
                bg="#162035" if active else "#0d1525",
                fg=PHOS if active else TEXT_HI,
                relief=tk.FLAT, font=("Consolas", 9), cursor="hand2",
                command=lambda s=secs, l=label: self._set_window(s, l))
            btn.pack(side=tk.LEFT, padx=2)
            self._win_btns[label] = btn

        # Control buttons
        tk.Frame(rp, bg=GRID_MAJ, height=1).pack(fill=tk.X, padx=12, pady=(4, 8))
        ctrl = tk.Frame(rp, bg=PANEL_BG)
        ctrl.pack()
        self._btn_pause = tk.Button(
            ctrl, text="Pause", width=9,
            bg="#1a2840", fg=TEXT_HI, relief=tk.FLAT,
            font=("Consolas", 9), cursor="hand2",
            command=self._toggle_pause)
        self._btn_pause.pack(side=tk.LEFT, padx=3)
        tk.Button(ctrl, text="Reset", width=9,
                  bg="#1a2840", fg=TEXT_HI, relief=tk.FLAT,
                  font=("Consolas", 9), cursor="hand2",
                  command=self._reset_stats).pack(side=tk.LEFT, padx=3)

        # Footer: Rx rate
        tk.Frame(rp, bg=GRID_MAJ, height=1).pack(fill=tk.X, padx=12, pady=(14, 6))
        self._lbl_rate = tk.Label(rp, text="Rx: ---  Hz",
                                   bg=PANEL_BG, fg=TEXT_DIM,
                                   font=("Consolas", 8))
        self._lbl_rate.pack()

    # ─────────────────────────────────────────────────────────────────────────
    # Arc gauge
    # ─────────────────────────────────────────────────────────────────────────
    def _draw_gauge(self, ratio):
        c = self._gauge
        c.delete("all")
        W, H       = 222, 156
        cx, cy     = W // 2, H - 18
        R_OUT, R_IN = 76, 55

        def a2xy(deg, r):
            rad = math.radians(deg)
            return cx + r * math.cos(rad), cy - r * math.sin(rad)

        # Background track
        c.create_arc(cx - R_OUT, cy - R_OUT, cx + R_OUT, cy + R_OUT,
                     start=210, extent=-240,
                     style=tk.ARC, outline=GRID_MAJ, width=R_OUT - R_IN)

        # Value arc (color-coded)
        if ratio > 0.002:
            col = PHOS if ratio < 0.60 else YELLOW if ratio < 0.85 else WARN
            c.create_arc(cx - R_OUT, cy - R_OUT, cx + R_OUT, cy + R_OUT,
                         start=210, extent=-240 * ratio,
                         style=tk.ARC, outline=col, width=R_OUT - R_IN)

        # Tick marks (9 major ticks)
        for i in range(9):
            x1, y1 = a2xy(210 - 30 * i, R_OUT - 2)
            x2, y2 = a2xy(210 - 30 * i, R_IN  + 4)
            c.create_line(x1, y1, x2, y2, fill=TEXT_DIM, width=1)

        # Needle
        nx, ny = a2xy(210 - 240 * ratio, R_IN - 6)
        c.create_line(cx, cy, nx, ny, fill=TEXT_HI, width=2, capstyle=tk.ROUND)
        c.create_oval(cx - 5, cy - 5, cx + 5, cy + 5,
                      fill="#1a2840", outline=TEXT_DIM, width=1)

        # Voltage & percent text
        c.create_text(cx, cy - 28, text=f"{ratio * VREF:.3f} V",
                      fill=TEXT_HI, font=("Consolas", 13, "bold"), anchor="s")
        c.create_text(cx, cy - 13, text=f"{ratio * 100:.1f} %",
                      fill=TEXT_DIM, font=("Consolas", 9), anchor="s")

        # Corner labels
        x0, y0 = a2xy(210,  R_OUT + 10)
        xv, yv = a2xy(-30,  R_OUT + 10)
        c.create_text(x0, y0, text="0 V",       fill=TEXT_DIM, font=("Consolas", 7))
        c.create_text(xv, yv, text=f"{VREF} V", fill=TEXT_DIM, font=("Consolas", 7))

    # ─────────────────────────────────────────────────────────────────────────
    # Animation
    # ─────────────────────────────────────────────────────────────────────────
    def _start_animation(self):
        self._ani = animation.FuncAnimation(
            self.fig, self._update_frame,
            interval=40, blit=False, cache_frame_data=False)

    def _update_frame(self, _frame):
        if self.paused:
            return

        pts  = min(self._win_pts, len(self.buf_volt))
        data = list(self.buf_volt)[-pts:]
        t_ms = [i * (1000.0 / SAMPLE_HZ) for i in range(pts)]

        # ── Waveform (3-layer glow) ──────────────────────────────────────────
        if len(t_ms) > 1:
            self.ax_osc.set_xlim(0, max(t_ms[-1], self._win_sec * 1000 * 0.98))
            for ln in (self._ln_m, self._ln_g1, self._ln_g2):
                ln.set_data(t_ms, data)
            try:
                self._fill.remove()
            except Exception:
                pass
            self._fill = self.ax_osc.fill_between(
                t_ms, data, color=PHOS_DIM, alpha=0.55)

        # ── FFT ─────────────────────────────────────────────────────────────
        n_use = min(256, len(data))
        if n_use >= 16:
            arr     = np.array(data[-n_use:], dtype=np.float64)
            arr     = (arr - arr.mean()) * np.hanning(n_use)
            fft_amp = np.abs(np.fft.rfft(arr, n=256))
            fft_db  = 20.0 * np.log10(fft_amp / (n_use / 2 + 1e-12) + 1e-12)
            freqs   = np.fft.rfftfreq(256, d=1.0 / SAMPLE_HZ)
            bw      = (SAMPLE_HZ / 2) / self.N_FFT_BARS
            for i, bar in enumerate(self._fft_bars):
                f_c = i * bw + bw / 2
                j   = int(np.argmin(np.abs(freqs - f_c)))
                db  = float(np.clip(fft_db[j], -80, 0))
                bar.set_height(db + 80)

        # ── Right panel ──────────────────────────────────────────────────────
        adc   = self.adc_now
        ratio = adc / ADC_MAX
        volt  = ratio * VREF
        col   = PHOS if ratio < 0.60 else YELLOW if ratio < 0.85 else WARN

        self._draw_gauge(ratio)
        self._lbl_adc.configure(text=f"{adc:04d}", fg=col)
        self._draw_bar(ratio, col)

        if data:
            v_min = min(data)
            v_max = max(data)
            v_pp  = v_max - v_min
            rms   = math.sqrt(sum(v * v for v in data) / len(data))
            freq  = self._estimate_freq(data)

            self._stat["now"].configure(text=f"{volt:.4f} V")
            self._stat["min"].configure(
                text=f"{self.adc_min / ADC_MAX * VREF:.4f} V")
            self._stat["max"].configure(
                text=f"{self.adc_max / ADC_MAX * VREF:.4f} V")
            self._stat["vpp"].configure(text=f"{v_pp:.4f} V")
            self._stat["rms"].configure(text=f"{rms:.4f} V")
            self._stat["freq"].configure(
                text=f"{freq:.2f} Hz" if freq > 0.1 else "    ---")

        self._lbl_rate.configure(text=f"Rx: {self.rx_hz:.0f}  Hz")
        self.canvas.draw_idle()

    @staticmethod
    def _estimate_freq(data):
        """Count VREF/2 rising-edge crossings → Hz."""
        mid   = VREF / 2.0
        count = sum(1 for i in range(1, len(data))
                    if data[i - 1] < mid <= data[i])
        dur   = len(data) / SAMPLE_HZ
        return count / dur if dur > 0 else 0.0

    def _draw_bar(self, ratio, color):
        c = self._bar_cv
        c.delete("all")
        w = c.winfo_width()
        h = c.winfo_height()
        if w <= 1:
            return
        c.create_rectangle(0, 0, w, h, fill="#0d1525", outline="")
        bw = int(w * ratio)
        if bw > 0:
            c.create_rectangle(0, 0, bw, h, fill=color, outline="")

    # ─────────────────────────────────────────────────────────────────────────
    # Controls
    # ─────────────────────────────────────────────────────────────────────────
    def _set_window(self, secs, label):
        self._win_sec = secs
        self._win_pts = SAMPLE_HZ * secs
        self.ax_osc.set_xlim(0, secs * 1000)
        for lbl, btn in self._win_btns.items():
            active = (lbl == label)
            btn.configure(bg="#162035" if active else "#0d1525",
                          fg=PHOS if active else TEXT_HI)

    def _toggle_pause(self):
        self.paused = not self.paused
        self._btn_pause.configure(
            text="Resume" if self.paused else "Pause",
            fg=TRIG_COL if self.paused else TEXT_HI)

    def _reset_stats(self):
        self.adc_min = ADC_MAX
        self.adc_max = 0

    # ─────────────────────────────────────────────────────────────────────────
    # Status LED blink
    # ─────────────────────────────────────────────────────────────────────────
    def _blink_led(self):
        if not self.running:
            return
        self._led_cv.itemconfigure(self._led, fill=PHOS)
        self.root.after(150, lambda:
            self._led_cv.itemconfigure(self._led, fill="#005533")
            if self.running else None)
        self.root.after(600, self._blink_led)

    # ─────────────────────────────────────────────────────────────────────────
    # Serial
    # ─────────────────────────────────────────────────────────────────────────
    def _refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        descs = [f"{p.device}   {p.description}" for p in ports]
        self._combo["values"] = descs
        if descs:
            for i, d in enumerate(descs):
                if any(k in d for k in USB_KW):
                    self._combo.current(i)
                    return
            self._combo.current(0)

    def _toggle_connect(self):
        if self.running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        sel = self._combo.get()
        if not sel:
            messagebox.showwarning("Warning", "Select a COM port first.")
            return
        port = sel.split()[0].strip()
        try:
            self.serial_port = serial.Serial(port, BAUD_RATE, timeout=0.1)
            self.running     = True
            self._rx_count   = 0
            self._rx_t0      = time.time()
            self._btn_conn.configure(text="Disconnect", fg=WARN)
            self._status_var.set(f"  {port}  @  {BAUD_RATE} bps")
            self._blink_led()
            threading.Thread(target=self._read_serial, daemon=True).start()
        except Exception as e:
            messagebox.showerror("Connect Error", f"Cannot open  {port}\n\n{e}")

    def _disconnect(self):
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self._btn_conn.configure(text="Connect", fg=TEXT_HI)
        self._status_var.set("Disconnected")
        self._led_cv.itemconfigure(self._led, fill=TEXT_DIM)

    def _read_serial(self):
        buf = ""
        while self.running:
            try:
                if self.serial_port.in_waiting:
                    buf += self.serial_port.read(
                        self.serial_port.in_waiting).decode("ascii", errors="ignore")
                    while "\n" in buf:
                        line, buf = buf.split("\n", 1)
                        line = line.strip()
                        try:
                            val = max(0, min(ADC_MAX, int(line)))
                            self.adc_now = val
                            if val > self.adc_max:
                                self.adc_max = val
                            if val < self.adc_min:
                                self.adc_min = val
                            self.buf_volt.append(val / ADC_MAX * VREF)
                            self._rx_count += 1
                            t = time.time()
                            if t - self._rx_t0 >= 1.0:
                                self.rx_hz     = self._rx_count / (t - self._rx_t0)
                                self._rx_count = 0
                                self._rx_t0    = t
                        except ValueError:
                            pass
                else:
                    time.sleep(0.004)
            except Exception:
                break

    # ─────────────────────────────────────────────────────────────────────────
    def on_close(self):
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        try:
            self._ani.event_source.stop()
        except Exception:
            pass
        self.root.destroy()


def main():
    root = tk.Tk()
    app  = OscilloscopeApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()

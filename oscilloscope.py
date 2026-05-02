"""
TC237 AN0 Real-time Oscilloscope
---------------------------------
TC237 VADC AN0 (12-bit, 0~4095) → UART 115200bps → PC

사용법:
  1. PORT 변수를 실제 COM 포트로 수정
  2. pip install pyserial matplotlib
  3. python oscilloscope.py
"""

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# ── 설정 ──────────────────────────────────────────────
PORT      = 'COM5'       # USB-TTL 변환기 COM 포트 (장치관리자 확인)
BAUDRATE  = 115200
VREF      = 3.3          # TC237 VADC 기준전압 (V)
ADC_MAX   = 4095         # 12-bit
SAMPLE_MS = 10           # TC237 전송 주기 (AppTask_10ms)
MAX_PTS   = 300          # 화면에 표시할 샘플 수 (300 × 10ms = 3초)
# ──────────────────────────────────────────────────────

# 데이터 버퍼
voltage_buf = deque([0.0] * MAX_PTS, maxlen=MAX_PTS)
time_axis   = [i * SAMPLE_MS for i in range(MAX_PTS)]   # ms 단위

# 시리얼 포트
try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=0.01)
    print(f"[OK] {PORT} @ {BAUDRATE}bps 연결 성공")
except Exception as e:
    print(f"[ERROR] 시리얼 포트 열기 실패: {e}")
    print(f"  PORT = '{PORT}' 를 확인하세요.")
    exit(1)

# ── 그래프 설정 ───────────────────────────────────────
fig, (ax_main, ax_info) = plt.subplots(
    2, 1, figsize=(12, 7),
    gridspec_kw={'height_ratios': [5, 1]}
)
fig.patch.set_facecolor('#111111')
fig.suptitle('TC237 AN0 Oscilloscope', color='#00FF88', fontsize=14, fontweight='bold')

# 메인 파형
ax_main.set_facecolor('#1a1a2e')
ax_main.set_xlim(0, MAX_PTS * SAMPLE_MS)
ax_main.set_ylim(-0.1, VREF + 0.1)
ax_main.set_xlabel('Time (ms)', color='#aaaaaa')
ax_main.set_ylabel('Voltage (V)', color='#aaaaaa')
ax_main.tick_params(colors='#aaaaaa')
ax_main.grid(True, color='#2a2a4a', linewidth=0.8)
for spine in ax_main.spines.values():
    spine.set_color('#444466')

# 기준선
ax_main.axhline(y=VREF,       color='#FF4444', linewidth=0.7, linestyle='--', alpha=0.7, label=f'VREF {VREF}V')
ax_main.axhline(y=VREF / 2.0, color='#444466', linewidth=0.7, linestyle='--', alpha=0.7, label=f'{VREF/2:.2f}V')
ax_main.axhline(y=0,          color='#444466', linewidth=0.7, linestyle='--', alpha=0.7, label='GND')
ax_main.legend(loc='upper right', fontsize=8, facecolor='#1a1a2e', labelcolor='#aaaaaa')

line_wave, = ax_main.plot(time_axis, list(voltage_buf),
                           color='#00FF88', linewidth=1.2, antialiased=True)

# 정보 바
ax_info.set_facecolor('#0d0d1a')
ax_info.axis('off')
info_text = ax_info.text(
    0.01, 0.5, '',
    transform=ax_info.transAxes,
    color='#00FF88', fontsize=10,
    fontfamily='monospace', verticalalignment='center'
)

plt.tight_layout()

# ── 애니메이션 업데이트 ───────────────────────────────
raw_prev = 0

def update(frame):
    global raw_prev

    # 수신된 모든 데이터 처리
    while ser.in_waiting > 0:
        try:
            raw_line = ser.readline().decode('utf-8').strip()
            raw = int(raw_line)
            if 0 <= raw <= ADC_MAX:
                voltage = raw * VREF / ADC_MAX
                voltage_buf.append(voltage)
                raw_prev = raw
        except (ValueError, UnicodeDecodeError):
            pass

    # 파형 갱신
    line_wave.set_ydata(list(voltage_buf))

    # 정보 갱신
    if len(voltage_buf) > 0:
        v_now  = voltage_buf[-1]
        v_max  = max(voltage_buf)
        v_min  = min(voltage_buf)
        v_pp   = v_max - v_min
        info_text.set_text(
            f"  NOW: {v_now:.3f} V  ({raw_prev:4d})  |"
            f"  MAX: {v_max:.3f} V  |"
            f"  MIN: {v_min:.3f} V  |"
            f"  Vpp: {v_pp:.3f} V  |"
            f"  Fs: {1000//SAMPLE_MS} Hz  |"
            f"  Port: {PORT}"
        )

    return line_wave, info_text

ani = animation.FuncAnimation(
    fig, update,
    interval=SAMPLE_MS,
    blit=True,
    cache_frame_data=False
)

plt.show()
ser.close()
print("[종료] 시리얼 포트 닫힘")

"""
MPU-9250 IMU 3D Viewer (matplotlib)
TC237 UART로 수신한 Roll/Pitch/Yaw를 실시간 3D 큐브로 시각화

사용법:
    python imu_3d_viewer.py

필요 패키지:
    pip install pyserial matplotlib numpy
"""

import sys
import serial
import serial.tools.list_ports
import threading
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import matplotlib.animation as animation

# ─────────────────────────────────────────────
# 설정
# ─────────────────────────────────────────────
BAUDRATE = 115200

# ─────────────────────────────────────────────
# 전역 자세 데이터
# ─────────────────────────────────────────────
roll = 0.0
pitch = 0.0
yaw = 0.0
lock = threading.Lock()
connected = False


def parse_line(line: str):
    """UART 수신 라인 파싱: 'R:+012.3,P:-005.7,Y:+045.2'"""
    global roll, pitch, yaw
    try:
        parts = line.strip().split(",")
        if len(parts) != 3:
            return
        r = float(parts[0].split(":")[1])
        p = float(parts[1].split(":")[1])
        y = float(parts[2].split(":")[1])
        with lock:
            roll, pitch, yaw = r, p, y
    except (ValueError, IndexError):
        pass


def serial_thread(port: str):
    """시리얼 수신 스레드"""
    global connected
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=1)
        connected = True
        print(f"[Serial] Connected to {port} @ {BAUDRATE}")
        while True:
            line = ser.readline().decode("ascii", errors="ignore").strip()
            if line:
                print(f"[RX] {line}")   # 디버그: 수신 데이터 출력
            if line.startswith("R:"):
                parse_line(line)
    except serial.SerialException as e:
        print(f"[Serial] Error: {e}")
        connected = False


# ─────────────────────────────────────────────
# 회전 행렬
# ─────────────────────────────────────────────
def rotation_matrix(roll_deg, pitch_deg, yaw_deg):
    """Euler 각도 → 3x3 회전 행렬
       Y축 = 비행기 앞(코), X축 = 날개, Z축 = 위
       Roll  = Y축 회전 (날개 기울임)
       Pitch = X축 회전 (기수 상하)
       Yaw   = Z축 회전 (좌우 방향 전환)"""
    r = np.radians(roll_deg)
    p = np.radians(pitch_deg)
    y = np.radians(yaw_deg)

    # Roll (Y축 — 전진 방향 축, 날개 기울임)
    Ry = np.array([
        [ np.cos(r), 0, np.sin(r)],
        [ 0,         1, 0],
        [-np.sin(r), 0, np.cos(r)],
    ])
    # Pitch (X축 — 날개 방향 축, 기수 상하)
    Rx = np.array([
        [1,  0,          0],
        [0,  np.cos(p), -np.sin(p)],
        [0,  np.sin(p),  np.cos(p)],
    ])
    # Yaw (Z축 — 수직 축, 좌우 회전)
    Rz = np.array([
        [np.cos(y), -np.sin(y), 0],
        [np.sin(y),  np.cos(y), 0],
        [0,          0,         1],
    ])

    return Rz @ Ry @ Rx


# ─────────────────────────────────────────────
# 3D 큐브 정의 (보드 형태: 넓고 얇게)
# ─────────────────────────────────────────────
CUBE_VERTICES = np.array([
    [-1.2, -2.5, -0.15],   # X=날개(짧게), Y=앞뒤(길게), Z=얇게
    [ 1.2, -2.5, -0.15],
    [ 1.2,  2.5, -0.15],
    [-1.2,  2.5, -0.15],
    [-1.2, -2.5,  0.15],
    [ 1.2, -2.5,  0.15],
    [ 1.2,  2.5,  0.15],
    [-1.2,  2.5,  0.15],
])

CUBE_FACES = [
    [0, 1, 2, 3],  # 하면
    [4, 5, 6, 7],  # 상면
    [0, 1, 5, 4],  # 앞면
    [2, 3, 7, 6],  # 뒷면
    [0, 3, 7, 4],  # 좌측
    [1, 2, 6, 5],  # 우측
]

FACE_COLORS = [
    (0.6, 0.2, 0.8, 0.7),  # 하: 보라
    (0.8, 0.4, 0.0, 0.7),  # 상: 주황
    (0.8, 0.2, 0.2, 0.7),  # 앞: 빨강
    (0.2, 0.2, 0.8, 0.7),  # 뒤: 파랑
    (0.8, 0.8, 0.2, 0.7),  # 좌: 노랑
    (0.2, 0.8, 0.2, 0.7),  # 우: 초록
]


def select_port():
    """사용 가능한 COM 포트 목록을 보여주고 선택"""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("사용 가능한 COM 포트가 없습니다.")
        sys.exit(1)

    print("\n=== 사용 가능한 COM 포트 ===")
    for i, p in enumerate(ports):
        print(f"  [{i + 1}] {p.device:8s} - {p.description}")
    print()

    if len(ports) == 1:
        print(f"→ {ports[0].device} 자동 선택")
        return ports[0].device

    while True:
        try:
            sel = input(f"포트 번호 선택 (1~{len(ports)}): ").strip()
            idx = int(sel) - 1
            if 0 <= idx < len(ports):
                return ports[idx].device
        except (ValueError, KeyboardInterrupt):
            pass
        print("잘못된 입력입니다.")


def main():
    # 커맨드라인 인자가 있으면 그대로 사용, 없으면 선택 메뉴
    if len(sys.argv) >= 2:
        port = sys.argv[1]
    else:
        port = select_port()

    # 시리얼 스레드 시작
    t = threading.Thread(target=serial_thread, args=(port,), daemon=True)
    t.start()

    # matplotlib 3D 설정
    fig = plt.figure(figsize=(10, 7))
    fig.canvas.manager.set_window_title("MPU-9250 IMU 3D Viewer")
    ax = fig.add_subplot(111, projection="3d")

    def update(frame):
        ax.cla()

        with lock:
            r, p, y = roll, pitch, yaw

        R = rotation_matrix(r, p, y)

        # 큐브 회전
        rotated = (R @ CUBE_VERTICES.T).T

        # 면 그리기
        faces = [[rotated[v] for v in face] for face in CUBE_FACES]
        poly = Poly3DCollection(faces, facecolors=FACE_COLORS, edgecolors="white", linewidths=1.5)
        ax.add_collection3d(poly)

        # 축 그리기 (월드 좌표)
        axis_len = 3.5
        ax.quiver(0, 0, 0, axis_len, 0, 0, color="r", arrow_length_ratio=0.1, linewidth=2)
        ax.quiver(0, 0, 0, 0, axis_len, 0, color="g", arrow_length_ratio=0.1, linewidth=2)
        ax.quiver(0, 0, 0, 0, 0, axis_len, color="b", arrow_length_ratio=0.1, linewidth=2)
        ax.text(axis_len + 0.3, 0, 0, "X", color="r", fontsize=12, fontweight="bold")
        ax.text(0, axis_len + 0.3, 0, "Y", color="g", fontsize=12, fontweight="bold")
        ax.text(0, 0, axis_len + 0.3, "Z", color="b", fontsize=12, fontweight="bold")

        # 보드 방향 화살표 (앞면 = +Y 방향)
        nose = R @ np.array([0, 3.0, 0])
        ax.quiver(0, 0, 0, nose[0], nose[1], nose[2], color="green", arrow_length_ratio=0.15, linewidth=3)

        # 뷰 설정
        lim = 3.5
        ax.set_xlim([-lim, lim])
        ax.set_ylim([-lim, lim])
        ax.set_zlim([-lim, lim])
        ax.set_xlabel("X")
        ax.set_ylabel("Y")
        ax.set_zlabel("Z")
        ax.set_box_aspect([1, 1, 1])

        # 상태 텍스트
        status = "Connected" if connected else "Waiting..."
        color = "green" if connected else "red"
        ax.set_title(
            f"Roll: {r:+7.1f}°   Pitch: {p:+7.1f}°   Yaw: {y:+7.1f}°\n"
            f"[{port} - {status}]",
            fontsize=13,
            color=color if not connected else "black",
            fontfamily="monospace",
        )

    ani = animation.FuncAnimation(fig, update, interval=50, cache_frame_data=False)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()

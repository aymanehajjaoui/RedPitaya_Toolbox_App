# pip install PyQt5 matplotlib
import socket
import threading
import re
from collections import deque

from PyQt5 import QtCore, QtWidgets
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

# ---- Config ----
IP = "0.0.0.0"
PORT = 5000
HISTORY = 200  # samples kept in the rolling window

# ---- Regex (unchanged) ----
pattern = re.compile(
    r"CPU:(\d+\.?\d*)%, CPU0:(\d+\.?\d*)%, CPU1:(\d+\.?\d*)%, RAM:(\d+\.?\d*)%.*?FREQ0:(\d+)\w+, FREQ1:(\d+)\w+, Temp:(\d+\.?\d*)°C"
)

# ---- Reader thread (TCP server) ----
class TcpReader(QtCore.QThread):
    data = QtCore.pyqtSignal(dict)     # emits parsed values
    interval = QtCore.pyqtSignal(float)  # emits seconds

    def run(self):
        while True:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
                    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    srv.bind((IP, PORT))
                    srv.listen(1)
                    print(f"Listening on {IP}:{PORT}")
                    conn, addr = srv.accept()
                    print(f"Client {addr} connected")
                    with conn:
                        buf = bytearray()
                        while True:
                            chunk = conn.recv(4096)
                            if not chunk:
                                print("Client disconnected")
                                break
                            buf.extend(chunk)
                            # process complete lines
                            while True:
                                nl = buf.find(b'\n')
                                if nl < 0:
                                    break
                                line = buf[:nl]
                                del buf[:nl+1]
                                s = line.decode('utf-8', 'ignore').strip()
                                if not s:
                                    continue
                                if s.startswith("INTERVAL_US:"):
                                    try:
                                        us = int(s.split(':', 1)[1])
                                        self.interval.emit(max(us/1e6, 0.001))
                                    except Exception:
                                        pass
                                    continue

                                m = pattern.search(s)
                                if m:
                                    vals = {
                                        'CPU'  : float(m.group(1)),
                                        'CPU0' : float(m.group(2)),
                                        'CPU1' : float(m.group(3)),
                                        'RAM'  : float(m.group(4)),
                                        'FREQ0': float(m.group(5)),
                                        'FREQ1': float(m.group(6)),
                                        'TEMP' : float(m.group(7)),
                                    }
                                    self.data.emit(vals)
                                # Optionally print raw line:
                                # print(s)
            except Exception as e:
                print(f"[TcpReader] {e} — restarting")

# ---- Matplotlib canvas ----
class MplCanvas(FigureCanvas):
    def __init__(self):
        fig = Figure(figsize=(9, 10), dpi=100)
        self.ax1 = fig.add_subplot(411)  # CPU
        self.ax2 = fig.add_subplot(412)  # RAM
        self.ax3 = fig.add_subplot(413)  # Temp
        self.ax4 = fig.add_subplot(414)  # Freq
        super().__init__(fig)

# ---- Main Window ----
class Main(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("System Monitor (PyQt)")

        # Rolling buffers
        self.t = deque(maxlen=HISTORY)
        self.cpu  = deque(maxlen=HISTORY)
        self.cpu0 = deque(maxlen=HISTORY)
        self.cpu1 = deque(maxlen=HISTORY)
        self.ram  = deque(maxlen=HISTORY)
        self.temp = deque(maxlen=HISTORY)
        self.f0   = deque(maxlen=HISTORY)
        self.f1   = deque(maxlen=HISTORY)

        self.dt = 0.5  # updated by INTERVAL_US
        self.elapsed = 0.0

        # UI
        central = QtWidgets.QWidget()
        v = QtWidgets.QVBoxLayout(central)

        top = QtWidgets.QHBoxLayout()
        self.reset_btn = QtWidgets.QPushButton("Reset")
        self.reset_btn.clicked.connect(self.reset_buffers)
        self.int_label = QtWidgets.QLabel("Interval: 0.5 s")
        top.addWidget(self.reset_btn)
        top.addStretch(1)
        top.addWidget(self.int_label)
        v.addLayout(top)

        self.canvas = MplCanvas()
        v.addWidget(self.canvas)
        self.setCentralWidget(central)

        # Configure axes & persistent lines (no clears)
        ax1, ax2, ax3, ax4 = self.canvas.ax1, self.canvas.ax2, self.canvas.ax3, self.canvas.ax4

        (self.l_cpu,)  = ax1.plot([], [], label="CPU")
        (self.l_cpu0,) = ax1.plot([], [], label="CPU0")
        (self.l_cpu1,) = ax1.plot([], [], label="CPU1")
        ax1.set_ylim(0, 110)
        ax1.set_ylabel("CPU (%)")
        ax1.set_xlabel("Time (s)")
        ax1.grid(True)
        ax1.legend()

        (self.l_ram,) = ax2.plot([], [], color="green", label="RAM")
        ax2.set_ylim(0, 110)
        ax2.set_ylabel("RAM (%)")
        ax2.set_xlabel("Time (s)")
        ax2.grid(True)
        ax2.legend()

        (self.l_temp,) = ax3.plot([], [], color="red", label="Temp")
        ax3.set_ylabel("Temp (°C)")
        ax3.set_xlabel("Time (s)")
        ax3.grid(True)
        ax3.legend()

        (self.l_f0,) = ax4.plot([], [], label="Freq0 (MHz)")
        (self.l_f1,) = ax4.plot([], [], label="Freq1 (MHz)")
        ax4.set_ylabel("CPU Freq (MHz)")
        ax4.set_xlabel("Time (s)")
        ax4.grid(True)
        ax4.legend()

        # Reader thread
        self.reader = TcpReader()
        self.reader.data.connect(self.on_data)
        self.reader.interval.connect(self.on_interval)
        self.reader.start()

        # UI timer for redraw
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.redraw)
        self.timer.start(int(self.dt*1000))

    # Slots
    @QtCore.pyqtSlot(float)
    def on_interval(self, dt):
        self.dt = dt
        self.int_label.setText(f"Interval: {dt:.3f} s")
        self.timer.setInterval(max(int(self.dt*1000), 50))

    @QtCore.pyqtSlot(dict)
    def on_data(self, vals):
        # Advance time
        self.t.append(self.elapsed)
        self.elapsed += self.dt

        # Append with last value fallback
        self.cpu.append(vals.get('CPU',  self.cpu[-1]  if self.cpu  else 0.0))
        self.cpu0.append(vals.get('CPU0', self.cpu0[-1] if self.cpu0 else 0.0))
        self.cpu1.append(vals.get('CPU1', self.cpu1[-1] if self.cpu1 else 0.0))
        self.ram.append(vals.get('RAM',  self.ram[-1]  if self.ram  else 0.0))
        self.temp.append(vals.get('TEMP', self.temp[-1] if self.temp else 0.0))
        self.f0.append(vals.get('FREQ0', self.f0[-1]   if self.f0   else 0.0))
        self.f1.append(vals.get('FREQ1', self.f1[-1]   if self.f1   else 0.0))

    def reset_buffers(self):
        self.t.clear()
        self.cpu.clear(); self.cpu0.clear(); self.cpu1.clear()
        self.ram.clear(); self.temp.clear(); self.f0.clear(); self.f1.clear()
        self.elapsed = 0.0
        for ln in (self.l_cpu, self.l_cpu0, self.l_cpu1, self.l_ram, self.l_temp, self.l_f0, self.l_f1):
            ln.set_data([], [])
        self.canvas.draw_idle()

    def redraw(self):
        if not self.t:
            return

        x = list(self.t)

        # update lines
        self.l_cpu.set_data(x,  list(self.cpu))
        self.l_cpu0.set_data(x, list(self.cpu0))
        self.l_cpu1.set_data(x, list(self.cpu1))
        self.l_ram.set_data(x,  list(self.ram))
        self.l_temp.set_data(x, list(self.temp))
        self.l_f0.set_data(x,   list(self.f0))
        self.l_f1.set_data(x,   list(self.f1))

        # keep x limits to visible history
        xmax = x[-1]
        xmin = max(0.0, xmax - (HISTORY-1)*self.dt)
        for ax in (self.canvas.ax1, self.canvas.ax2, self.canvas.ax3, self.canvas.ax4):
            ax.set_xlim(xmin, xmax if xmax > 0 else 1.0)

        # dynamic y for Temp
        tvals = list(self.temp)
        if tvals:
            tmin, tmax = min(tvals), max(tvals)
            pad = max(0.5, 0.05 * max(1.0, tmax - tmin))
            if tmax == tmin:
                self.canvas.ax3.set_ylim(tmin - 1, tmax + 1)
            else:
                self.canvas.ax3.set_ylim(tmin - pad, tmax + pad)

        # dynamic y for Freq
        f0v, f1v = list(self.f0), list(self.f1)
        if f0v and f1v:
            fmin = min(min(f0v), min(f1v))
            fmax = max(max(f0v), max(f1v))
            pad = max(10.0, 0.05 * max(1.0, fmax - fmin))
            if fmax == fmin:
                self.canvas.ax4.set_ylim(max(0, fmin - 20), fmax + 20)
            else:
                self.canvas.ax4.set_ylim(max(0, fmin - pad), fmax + pad)

        self.canvas.draw_idle()

# ---- Run ----
if __name__ == "__main__":
    app = QtWidgets.QApplication([])
    w = Main()
    w.resize(1000, 900)
    w.show()
    app.exec_()
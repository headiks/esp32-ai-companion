"""Graphical emulator of the companion. All behaviour, drawing and audio mixing come from the same C++ core
as the firmware; this file only provides the window, timing and test controls."""
import argparse
import sys
import time
from pathlib import Path

import numpy as np
from PySide6.QtCore import QTimer, Signal, QObject
from PySide6.QtWidgets import QApplication, QHBoxLayout, QMainWindow, QWidget

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from core.companion import Companion, emotion_names  # noqa: E402
from emulator.audio.output import AudioOutput  # noqa: E402
from emulator.communication.serial_link import SerialLink, list_serial_ports  # noqa: E402
from emulator.rendering.screen_widget import ScreenWidget  # noqa: E402
from emulator.simulation.demo import steps as demo_steps  # noqa: E402
from emulator.simulation.sensors import ImuSimulator, MicSimulator  # noqa: E402
from emulator.ui.panel import Panel  # noqa: E402

STYLE = """
QWidget { background: #17171b; color: #e8e8ea; font-size: 12px; }
QPushButton { background: #2a2a30; border: 1px solid #3a3a42; border-radius: 6px; padding: 5px 6px; }
QPushButton:hover { border-color: #ff8c00; }
QPushButton:pressed { background: #ff8c00; color: #000; }
QPushButton#demo { background: #ff8c00; color: #000; font-weight: bold; padding: 9px; font-size: 14px; }
QTabBar::tab { background: #222; padding: 6px 9px; }
QTabBar::tab:selected { background: #ff8c00; color: #000; }
QLineEdit, QComboBox, QDateTimeEdit, QPlainTextEdit { background: #101014; border: 1px solid #3a3a42; border-radius: 4px; padding: 3px; }
QGroupBox { border: 1px solid #333; border-radius: 6px; margin-top: 12px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; color: #ffcd28; }
QProgressBar { background: #101014; border: 1px solid #333; border-radius: 3px; max-height: 8px; }
QProgressBar::chunk { background: #ff8c00; }
QSlider::handle:horizontal { background: #ff8c00; width: 12px; border-radius: 6px; margin: -4px 0; }
QSlider::groove:horizontal { background: #333; height: 4px; }
"""


class Bridge(QObject):
    device_message = Signal(dict)
    log = Signal(str)


class Emulator(QMainWindow):
    def __init__(self, storage=None, silent=False):
        super().__init__()
        self.setWindowTitle("AI-компаньон — эмулятор ESP32-S3-Touch-AMOLED-1.75C")
        storage = Path(storage) if storage else ROOT / "emulator" / ".state"
        storage.mkdir(parents=True, exist_ok=True)
        self.companion = Companion(storage_dir=storage, seed=int(time.time()) & 0xFFFFFF or 1)
        self.emotions = emotion_names()
        self.imu = ImuSimulator()
        self.micsim = MicSimulator()
        self.bridge = Bridge()
        self.bridge.log.connect(self._log)
        self.bridge.device_message.connect(self._on_device_message)
        self.link = None
        self.out_level = 0.0
        self.demo_queue = []
        self.demo_wait = 0.0
        self.cycle_queue = []
        self.cycle_wait = 0.0
        self.last_state = {}
        self.silent = silent

        self.screen = ScreenWidget(self.companion.touch)
        self.panel = Panel(self)
        self.panel.setFixedWidth(430)
        root = QWidget()
        lay = QHBoxLayout(root)
        lay.addWidget(self.screen)
        lay.addWidget(self.panel, 1)
        self.setCentralWidget(root)
        self.setStyleSheet(STYLE)

        self.audio = AudioOutput(self.companion, self._tap)
        if not silent and not self.audio.start():
            self._log(f"Звук недоступен: {self.audio.error}")
        self.companion.set_battery(80, False, True, 4020)
        self.companion.handle_command({"type": "set_time", "epoch": int(time.time()), "tz_minutes": -time.timezone // 60})

        self.last = time.perf_counter()
        self.frames = 0
        self.fps_t = self.last
        self.state_t = 0.0
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._frame)
        self.timer.start(16)

    def _tap(self, level):
        self.out_level = max(level, self.out_level * 0.8)

    def _frame(self):
        now = time.perf_counter()
        dt = (now - self.last) * 1000
        self.last = now
        ax, ay, az, gx, gy, gz = self.imu.sample()
        self.companion.imu(ax, ay, az, gx, gy, gz, dt)
        self.companion.mic(self.micsim.samples(max(1, int(dt * 16))))
        self.companion.tick(dt)
        self.screen.show_frame(self.companion.render())
        self._run_scripts(dt)
        self.frames += 1
        if now - self.fps_t >= 1:
            self.companion.set_fps(self.frames / (now - self.fps_t))
            self.frames = 0
            self.fps_t = now
        self.state_t -= dt
        if self.state_t <= 0:
            self.state_t = 120
            st = self.companion.state()
            self.last_state = st
            d = self.companion.diag()
            self.panel.update_state(st, d.get("mic_db", -90), self.out_level)
            self.out_level *= 0.7

    def _run_scripts(self, dt):
        if self.demo_queue:
            self.demo_wait -= dt
            while self.demo_queue and self.demo_wait <= 0:
                _, step = self.demo_queue.pop(0)
                self.local(step)
                self._log(f"DEMO: {step}")
                if self.demo_queue:
                    self.demo_wait += self.demo_queue[0][0]
            if not self.demo_queue:
                self.panel.demo_btn.setText("▶ DEMO MODE")
        if self.cycle_queue:
            self.cycle_wait -= dt
            if self.cycle_wait <= 0:
                cmd = self.cycle_queue.pop(0)
                self.send(cmd)
                self.cycle_wait = 2400 if cmd["type"] == "emotion" else 1500

    # ---- control API used by the panel ----------------------------------------------------------
    def local(self, cmd):
        resp = self.companion.handle_command(cmd)
        if resp.get("status") != "success":
            self._log(f"← {resp}")
        return resp

    def send(self, cmd):
        if self.link and self.link.connected:
            try:
                self.link.send(cmd)
                self._log(f"→ {cmd}")
            except OSError as e:
                self._log(f"Ошибка связи: {e}")
            return
        self._log(f"← {self.local(cmd)}" if cmd["type"] in ("ping", "diag", "get_state") else f"→ {cmd}")

    def toggle_demo(self):
        if self.demo_queue:
            self.demo_queue = []
            self.panel.demo_btn.setText("▶ DEMO MODE")
        else:
            self.demo_queue = demo_steps(self)
            self.demo_wait = 0
            self.panel.demo_btn.setText("■ Остановить демо")

    def cycle_emotions(self):
        self.cycle_queue = [{"type": "emotion", "value": e, "hold_ms": 2200} for e in self.emotions]
        self.cycle_wait = 0

    def play_all_sounds(self):
        self.cycle_queue = [{"type": "play_sound", "value": s} for s in self.companion.sounds()]
        self.cycle_wait = 0

    def set_time_scale(self, s):
        self.companion.set_time_scale(s)

    def serial_ports(self):
        return list_serial_ports()

    # ---- Hardware-in-the-Loop --------------------------------------------------------------------
    def hil_serial(self, port):
        self.hil_close()
        try:
            link = SerialLink(self.bridge.device_message.emit, self.bridge.log.emit)
            link.open(port)
            self.link = link
            self.panel.hil_label.setText(f"Режим: устройство {port}")
            self.link.send({"type": "hello"})
            self.link.send({"type": "get_state"})
        except Exception as e:  # pyserial raises several types for a bad port or URL
            self._log(f"Не удалось открыть {port}: {e}")

    def hil_ble(self):
        self.hil_close()
        try:
            from emulator.communication.ble_link import BleLink
            link = BleLink(self.bridge.device_message.emit, self.bridge.log.emit)
            found = link.scan()
            if not found:
                self._log("BLE: устройство не найдено")
                return
            name, addr = found[0]
            link.open(addr)
            self.link = link
            self.panel.hil_label.setText(f"Режим: BLE {name} {addr}")
            link.send({"type": "auth", "pin": self.panel.pin.text()})
            link.send({"type": "get_state"})
        except Exception as e:  # bleak raises backend specific errors
            self._log(f"BLE: {e}")

    def hil_close(self):
        if self.link:
            self.link.close()
        self.link = None
        self.panel.hil_label.setText("Режим: эмуляция")

    def _on_device_message(self, msg):
        # The emulated face mirrors the real device: same emotion, same stats.
        emo = msg.get("emotion")
        if emo and emo != self.last_state.get("emotion"):
            self.companion.handle_command({"type": "emotion", "value": emo, "hold_ms": 60000})
        for k in ("happiness", "energy", "curiosity", "attention", "affection", "stress", "sleepiness", "hunger", "fun", "health"):
            if k in msg:
                self.companion.handle_command({"type": "set_stat", "name": k, "value": msg[k]})
        if "battery" in msg:
            self.companion.set_battery(msg["battery"], msg.get("charging", False))
        if msg.get("event") != "state":
            self._log(f"← {msg}")

    def _log(self, s):
        self.panel.log_line.emit(str(s)) if hasattr(self, "panel") else print(s)

    def closeEvent(self, e):
        self.timer.stop()
        self.audio.stop()
        self.hil_close()
        self.companion.close()
        super().closeEvent(e)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--storage", help="folder for persistent state")
    ap.add_argument("--silent", action="store_true", help="no sound output")
    ap.add_argument("--screenshot", help="save a window screenshot after --seconds and exit")
    ap.add_argument("--seconds", type=float, default=3)
    ap.add_argument("--demo", action="store_true", help="start DEMO MODE right away")
    args = ap.parse_args()
    app = QApplication(sys.argv)
    win = Emulator(args.storage, args.silent)
    win.show()
    if args.demo:
        win.toggle_demo()
    if args.screenshot:
        QTimer.singleShot(int(args.seconds * 1000), lambda: (win.grab().save(args.screenshot), print("fps", win.companion.diag()["fps"]), app.quit()))
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

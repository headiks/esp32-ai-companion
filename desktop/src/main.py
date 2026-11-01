"""Desktop app: connects to the device over USB or BLE, controls it, previews sounds, shows logs and diagnostics."""
import argparse
import json
import sys
import time
import wave
from pathlib import Path

import numpy as np
from PySide6.QtCore import QObject, QTimer, Qt, Signal
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit,
                               QListWidget, QMainWindow, QPlainTextEdit, QProgressBar, QPushButton, QScrollArea, QSlider, QTabWidget,
                               QVBoxLayout, QWidget)

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT))
from emulator.communication.serial_link import SerialLink, list_serial_ports  # noqa: E402

EMOTIONS = json.loads((ROOT / "protocol" / "commands.json").read_text(encoding="utf-8"))["commands"]["emotion"]["value"]
SCREENS = json.loads((ROOT / "protocol" / "commands.json").read_text(encoding="utf-8"))["commands"]["screen"]["value"]
STATS = ["happiness", "energy", "curiosity", "attention", "affection", "stress", "sleepiness", "hunger", "fun", "health"]
SOUNDS = ROOT / "assets" / "sounds"
STYLE = """
QWidget { background: #15151a; color: #ececee; font-size: 13px; }
QPushButton { background: #2a2a31; border: 1px solid #3c3c44; border-radius: 7px; padding: 6px 9px; }
QPushButton:hover { border-color: #ff8c00; } QPushButton:pressed { background: #ff8c00; color: #000; }
QTabBar::tab { background: #222; padding: 8px 12px; } QTabBar::tab:selected { background: #ff8c00; color: #000; }
QLineEdit, QComboBox, QPlainTextEdit, QListWidget { background: #0f0f13; border: 1px solid #3c3c44; border-radius: 5px; padding: 4px; }
QProgressBar { background: #0f0f13; border: 1px solid #333; border-radius: 4px; height: 14px; text-align: center; }
QProgressBar::chunk { background: #ff8c00; border-radius: 3px; }
QGroupBox { border: 1px solid #333; border-radius: 7px; margin-top: 12px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; color: #ffcd28; }
QSlider::handle:horizontal { background: #ff8c00; width: 14px; border-radius: 7px; margin: -5px 0; }
QSlider::groove:horizontal { background: #333; height: 4px; }
"""


def grid(items, cols=3):
    w = QWidget()
    g = QGridLayout(w)
    g.setContentsMargins(0, 0, 0, 0)
    for i, (title, cb) in enumerate(items):
        b = QPushButton(title)
        b.clicked.connect(lambda _=False, c=cb: c())
        g.addWidget(b, i // cols, i % cols)
    return w


class Bridge(QObject):
    message = Signal(dict)
    log = Signal(str)


def play_wav(path):
    """Local preview; independent from the device."""
    import sounddevice as sd
    with wave.open(str(path)) as w:
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
        sd.play(data, w.getframerate())


class Desktop(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AI-компаньон — управление")
        self.resize(900, 680)
        self.bridge = Bridge()
        self.bridge.message.connect(self.on_message)
        self.bridge.log.connect(self.log)
        self.link = None
        self.bars = {}
        self.pending = {}
        tabs = QTabWidget()
        tabs.addTab(self._connection(), "Подключение")
        tabs.addTab(self._control(), "Управление")
        tabs.addTab(self._sounds(), "Звуки")
        tabs.addTab(self._settings(), "Настройки")
        tabs.addTab(self._notify(), "Уведомления")
        tabs.addTab(self._diag(), "Диагностика")
        root = QWidget()
        lay = QVBoxLayout(root)
        self.top = QLabel("Нет подключения")
        lay.addWidget(self.top)
        lay.addWidget(tabs, 1)
        self.logs = QPlainTextEdit(readOnly=True)
        self.logs.setMaximumBlockCount(2000)
        self.logs.setFixedHeight(130)
        lay.addWidget(self.logs)
        self.setCentralWidget(root)
        self.setStyleSheet(STYLE)

    # ---- pages -----------------------------------------------------------------------------------
    def _connection(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        form = QFormLayout()
        self.port = QComboBox(editable=True)
        self.refresh_ports()
        form.addRow("USB-порт", self.port)
        lay.addLayout(form)
        row = QHBoxLayout()
        for t, cb in (("Обновить порты", self.refresh_ports), ("Подключить USB", self.connect_serial), ("Отключить", self.disconnect)):
            b = QPushButton(t)
            b.clicked.connect(cb)
            row.addWidget(b)
        lay.addLayout(row)
        box = QGroupBox("Bluetooth")
        v = QVBoxLayout(box)
        self.ble_list = QListWidget()
        v.addWidget(self.ble_list)
        self.pin = QLineEdit(placeholderText="PIN с экрана «Об устройстве»")
        v.addWidget(self.pin)
        row = QHBoxLayout()
        b = QPushButton("Поиск устройств")
        b.clicked.connect(self.ble_scan)
        row.addWidget(b)
        b = QPushButton("Подключить BLE")
        b.clicked.connect(self.ble_connect)
        row.addWidget(b)
        v.addLayout(row)
        lay.addWidget(box)
        raw = QHBoxLayout()
        self.raw = QLineEdit(placeholderText='Команда JSON: {"type":"ping"}')
        self.raw.returnPressed.connect(self.send_raw)
        raw.addWidget(self.raw, 1)
        b = QPushButton("Отправить")
        b.clicked.connect(self.send_raw)
        raw.addWidget(b)
        lay.addLayout(raw)
        lay.addStretch(1)
        return w

    def _control(self):
        w = QWidget()
        lay = QHBoxLayout(w)
        left = QVBoxLayout()
        left.addWidget(QLabel("Эмоции"))
        left.addWidget(grid([(e, lambda e=e: self.send({"type": "emotion", "value": e})) for e in EMOTIONS]))
        left.addWidget(QLabel("Питомец"))
        left.addWidget(grid([(t, lambda v=v: self.send({"type": "action", "value": v})) for t, v in [("Покормить", "feed"), ("Погладить", "pet"), ("Поиграть", "play"), ("Уложить", "sleep"), ("Разбудить", "wake")]]))
        left.addWidget(QLabel("Экран"))
        left.addWidget(grid([(s, lambda s=s: self.send({"type": "screen", "value": s})) for s in SCREENS], 4))
        row = QHBoxLayout()
        self.chat = QLineEdit(placeholderText="Сказать питомцу")
        self.chat.returnPressed.connect(self.send_chat)
        row.addWidget(self.chat, 1)
        b = QPushButton("Отправить")
        b.clicked.connect(self.send_chat)
        row.addWidget(b)
        left.addLayout(row)
        left.addStretch(1)
        lay.addLayout(left, 3)
        right = QFormLayout()
        for s in STATS:
            bar = QProgressBar()
            bar.setRange(0, 100)
            self.bars[s] = bar
            right.addRow(s, bar)
        lay.addLayout(right, 1)
        return w

    def _sounds(self):
        area = QScrollArea()
        area.setWidgetResizable(True)
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.addWidget(QLabel("«▶» играет файл на компьютере, «⇒» на устройстве."))
        for cat in sorted(p.name for p in SOUNDS.iterdir() if p.is_dir()):
            box = QGroupBox(cat)
            g = QGridLayout(box)
            for i, f in enumerate(sorted((SOUNDS / cat).glob("*.wav"))):
                cell = QWidget()
                h = QHBoxLayout(cell)
                h.setContentsMargins(0, 0, 0, 0)
                h.addWidget(QLabel(f.stem), 1)
                b = QPushButton("▶")
                b.clicked.connect(lambda _=False, p=f: self.preview(p))
                h.addWidget(b)
                b = QPushButton("⇒")
                b.clicked.connect(lambda _=False, n=f"{cat}/{f.stem}": self.send({"type": "play_sound", "value": n}))
                h.addWidget(b)
                g.addWidget(cell, i // 3, i % 3)
            lay.addWidget(box)
        area.setWidget(w)
        return area

    def _settings(self):
        w = QWidget()
        form = QFormLayout(w)

        def slider(name, lo, hi, val):
            s = QSlider(Qt.Horizontal)
            s.setRange(lo, hi)
            s.setValue(val)
            s.sliderReleased.connect(lambda: self.send({"type": "setting", "name": name, "value": s.value()}))
            return s

        for title, name, lo, hi, val in [("Яркость", "brightness", 5, 100, 70), ("Громкость", "volume", 0, 100, 60), ("Чувств. микрофонов", "mic_sensitivity", 10, 100, 60),
                                         ("Чувств. касаний", "touch_sensitivity", 10, 100, 60), ("Скорость анимации", "anim_speed", 25, 250, 100), ("Частота моргания", "blink_rate", 0, 250, 100),
                                         ("Сон через, с", "sleep_after", 15, 3600, 300)]:
            form.addRow(title, slider(name, lo, hi, val))
        for title, name, val in [("Звук", "sound", True), ("Микрофоны", "mic", True), ("Энергосбережение", "power_save", False), ("Bluetooth", "ble", True),
                                 ("Голос на AI-сервер", "ai_voice_consent", False)]:
            c = QCheckBox()
            c.setChecked(val)
            c.toggled.connect(lambda v, n=name: self.send({"type": "setting", "name": n, "value": v}))
            form.addRow(title, c)
        p = QComboBox()
        p.addItems(["playful", "calm", "grumpy", "shy"])
        p.currentTextChanged.connect(lambda v: self.send({"type": "setting", "name": "personality", "value": v}))
        form.addRow("Характер", p)
        self.ssid = QLineEdit()
        self.wpass = QLineEdit(echoMode=QLineEdit.Password)
        form.addRow("Wi-Fi SSID", self.ssid)
        form.addRow("Wi-Fi пароль", self.wpass)
        b = QPushButton("Сохранить Wi-Fi")
        b.clicked.connect(lambda: self.send({"type": "wifi_config", "ssid": self.ssid.text(), "password": self.wpass.text()}))
        form.addRow(b)
        self.ai = QLineEdit(placeholderText="http://192.168.1.10:8787")
        form.addRow("AI-сервер", self.ai)
        b = QPushButton("Сохранить AI-сервер")
        b.clicked.connect(lambda: self.send({"type": "setting", "name": "ai_url", "value": self.ai.text()}))
        form.addRow(b)
        row = QHBoxLayout()
        b = QPushButton("Синхронизировать время")
        b.clicked.connect(self.sync_time)
        row.addWidget(b)
        b = QPushButton("Сбросить всё")
        b.clicked.connect(lambda: self.send({"type": "factory_reset", "value": "confirm"}))
        row.addWidget(b)
        form.addRow(row)
        return w

    def _notify(self):
        w = QWidget()
        form = QFormLayout(w)
        self.n_title = QLineEdit("Сообщение")
        self.n_text = QLineEdit("Привет с компьютера!")
        self.n_icon = QComboBox()
        self.n_icon.addItems(["message", "bell", "mail", "phone", "calendar", "warning"])
        form.addRow("Заголовок", self.n_title)
        form.addRow("Текст", self.n_text)
        form.addRow("Иконка", self.n_icon)
        b = QPushButton("Отправить")
        b.clicked.connect(lambda: self.send({"type": "notify", "title": self.n_title.text(), "text": self.n_text.text(), "icon": self.n_icon.currentText(), "source": "pc", "time": int(time.time())}))
        form.addRow(b)
        b = QPushButton("Отметить всё прочитанным")
        b.clicked.connect(lambda: self.send({"type": "notifications", "value": "read_all"}))
        form.addRow(b)
        b = QPushButton("Очистить историю")
        b.clicked.connect(lambda: self.send({"type": "notifications", "value": "clear"}))
        form.addRow(b)
        return w

    def _diag(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        self.diag_text = QPlainTextEdit(readOnly=True)
        lay.addWidget(self.diag_text)
        b = QPushButton("Обновить")
        b.clicked.connect(lambda: self.send({"type": "diag"}))
        lay.addWidget(b)
        lay.addWidget(grid([(t, lambda v=v: self.send({"type": "sim_event", "value": v})) for t, v in
                            [("Встряхнуть", "shake"), ("Рывок", "jerk"), ("Экран вниз", "face_down"), ("Поднять", "lift"), ("Громкий звук", "loud"), ("Хлопок", "clap")]]))
        return w

    # ---- actions ---------------------------------------------------------------------------------
    def log(self, s):
        self.logs.appendPlainText(str(s))

    def refresh_ports(self):
        self.port.clear()
        self.port.addItems(list_serial_ports() + ["socket://127.0.0.1:7777"])

    def connect_serial(self):
        self.disconnect()
        try:
            link = SerialLink(self.bridge.message.emit, self.bridge.log.emit)
            link.open(self.port.currentText())
            self.link = link
            self.send({"type": "hello"})
            self.send({"type": "get_state"})
            self.sync_time()
        except Exception as e:  # pyserial raises several types for bad ports and URLs
            self.log(f"USB: {e}")

    def ble_link(self):
        from emulator.communication.ble_link import BleLink
        if not getattr(self, "_ble", None):
            self._ble = BleLink(self.bridge.message.emit, self.bridge.log.emit)
        return self._ble

    def ble_scan(self):
        self.ble_list.clear()
        self.log("Поиск BLE, 5 с…")
        QApplication.processEvents()
        try:
            for name, addr in self.ble_link().scan():
                self.ble_list.addItem(f"{name}  {addr}")
        except Exception as e:  # bleak errors differ per OS, e.g. Bluetooth switched off
            self.log(f"BLE: {e}")

    def ble_connect(self):
        item = self.ble_list.currentItem()
        if not item:
            self.log("Выберите устройство в списке")
            return
        self.disconnect()
        try:
            link = self.ble_link()
            link.open(item.text().split()[-1])
            self.link = link
            self.send({"type": "hello"})
            self.send({"type": "auth", "pin": self.pin.text()})
            self.send({"type": "get_state"})
        except Exception as e:  # bleak errors differ per OS
            self.log(f"BLE: {e}")

    def disconnect(self):
        if self.link:
            self.link.close()
        self.link = None
        self.top.setText("Нет подключения")

    def send(self, cmd):
        if not (self.link and self.link.connected):
            self.log("Нет подключения")
            return
        try:
            rid = self.link.send(cmd)
            self.pending[rid] = cmd.get("type")
            self.log(f"→ {json.dumps(cmd, ensure_ascii=False)}")
        except OSError as e:
            self.log(f"Ошибка: {e}")

    def send_raw(self):
        try:
            self.send(json.loads(self.raw.text()))
        except ValueError as e:
            self.log(f"JSON: {e}")

    def send_chat(self):
        self.send({"type": "chat", "text": self.chat.text()})
        self.chat.clear()

    def sync_time(self):
        self.send({"type": "set_time", "epoch": int(time.time()), "tz_minutes": -time.timezone // 60})

    def preview(self, path):
        try:
            play_wav(path)
        except Exception as e:  # no audio device on this PC
            self.log(f"Звук: {e}")

    def on_message(self, m):
        if m.get("event") != "state":
            self.log(f"← {json.dumps(m, ensure_ascii=False)}")
        for s, bar in self.bars.items():
            if s in m:
                bar.setValue(int(m[s]))
        if "emotion" in m:
            self.top.setText(f"Эмоция: {m['emotion']}   Состояние: {m.get('pet_status', '-')}   Батарея: {m.get('battery', '-')}%   Экран: {m.get('screen', '-')}")
        if "fps" in m:
            self.diag_text.setPlainText("\n".join(f"{k}: {v}" for k, v in m.items()))
        if m.get("status") == "error" and m.get("error") == "auth_required":
            self.log("Устройство просит PIN: см. экран «Об устройстве»")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--smoke", help="connect to this port, exercise commands, print results and exit")
    ap.add_argument("--screenshot")
    args = ap.parse_args()
    app = QApplication(sys.argv)
    win = Desktop()
    win.show()
    if args.smoke:
        win.port.setCurrentText(args.smoke)
        win.connect_serial()
        QTimer.singleShot(500, lambda: win.send({"type": "emotion", "value": "happy"}))
        QTimer.singleShot(800, lambda: win.send({"type": "diag"}))

        def finish():
            print(win.logs.toPlainText())
            if args.screenshot:
                win.grab().save(args.screenshot)
            win.disconnect()
            app.quit()
        QTimer.singleShot(1500, finish)
    elif args.screenshot:
        QTimer.singleShot(800, lambda: (win.grab().save(args.screenshot), app.quit()))
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

"""Control panel with every test action required: emotions, pet state, sensors, time, audio, sounds, HIL."""
import datetime
import json

from PySide6.QtCore import QTimer, Qt, Signal
from PySide6.QtWidgets import (QCheckBox, QComboBox, QDateTimeEdit, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QLabel,
                               QLineEdit, QPlainTextEdit, QProgressBar, QPushButton, QScrollArea, QSlider, QSpinBox, QTabWidget,
                               QVBoxLayout, QWidget)

STATS = ["happiness", "energy", "curiosity", "attention", "affection", "stress", "sleepiness", "hunger", "fun", "health"]
STAT_RU = {"happiness": "Счастье", "energy": "Энергия", "curiosity": "Любопытство", "attention": "Внимание", "affection": "Привязанность",
           "stress": "Стресс", "sleepiness": "Сонливость", "hunger": "Голод", "fun": "Веселье", "health": "Здоровье"}
SCREENS = [("face", "Лицо"), ("menu", "Меню"), ("pet", "Питомец"), ("analog_clock", "Аналог. часы"), ("digital_clock", "Цифр. часы"), ("timer", "Таймер"),
           ("stopwatch", "Секундомер"), ("notifications", "Уведомления"), ("settings", "Настройки"), ("diagnostics", "Диагностика"), ("games", "Игры"), ("info", "Инфо"),
           ("tictactoe", "Крестики-нолики"), ("reaction", "Реакция"), ("guess", "Угадай число"), ("pet_game", "Светлячок"),
           ("maze", "Лабиринт"), ("orbit", "Орбита"), ("snake", "Змейка"), ("simon", "Память"), ("weather", "Погода")]


def grid(buttons, cols=3):
    """buttons: list of (title, callback)."""
    w = QWidget()
    g = QGridLayout(w)
    g.setContentsMargins(0, 0, 0, 0)
    g.setSpacing(4)
    for i, (title, cb) in enumerate(buttons):
        b = QPushButton(title)
        b.clicked.connect(lambda _=False, c=cb: c())
        g.addWidget(b, i // cols, i % cols)
    return w


def on_slider_set(slider, callback):
    """Fires on drag end and on a click beside the handle; sliderReleased alone misses the click."""
    slider.sliderReleased.connect(lambda: callback(slider.value()))
    slider.actionTriggered.connect(lambda action: None if action == slider.SliderAction.SliderMove else QTimer.singleShot(0, lambda: callback(slider.value())))


class Panel(QWidget):
    log_line = Signal(str)

    def __init__(self, ctl):
        super().__init__()
        self.ctl = ctl
        self.sliders = {}
        self._updating = False
        tabs = self.tabs = QTabWidget()
        tabs.addTab(self._scroll(self._emotions_tab()), "Эмоции")
        tabs.addTab(self._scroll(self._pet_tab()), "Питомец")
        tabs.addTab(self._scroll(self._sensors_tab()), "Датчики")
        tabs.addTab(self._scroll(self._screens_tab()), "Экраны")
        tabs.addTab(self._scroll(self._sounds_tab()), "Звуки")
        tabs.addTab(self._scroll(self._hil_tab()), "Устройство")
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        self.demo_btn = QPushButton("▶ DEMO MODE")
        self.demo_btn.setObjectName("demo")
        self.demo_btn.clicked.connect(ctl.toggle_demo)
        lay.addWidget(self.demo_btn)
        self.status = QLabel()
        self.status.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.status.setWordWrap(True)
        lay.addWidget(self.status)
        lay.addWidget(tabs, 1)
        self.log = QPlainTextEdit(readOnly=True)
        self.log.setMaximumBlockCount(400)
        self.log.setFixedHeight(90)
        lay.addWidget(self.log)
        self.log_line.connect(self.log.appendPlainText)

    @staticmethod
    def _scroll(w):
        s = QScrollArea()
        s.setWidget(w)
        s.setWidgetResizable(True)
        return s

    def _emotions_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        names = self.ctl.emotions
        lay.addWidget(QLabel("Принудительная эмоция (с анимацией и звуком):"))
        lay.addWidget(grid([(n, lambda n=n: self.ctl.send({"type": "emotion", "value": n, "hold_ms": 4000})) for n in names]))
        lay.addWidget(QLabel("Анимации:"))
        lay.addWidget(grid([(t, c) for t, c in [
            ("Обойти все эмоции", self.ctl.cycle_emotions),
            ("Касание", lambda: self.ctl.send({"type": "gesture", "value": "tap"})),
            ("Двойное", lambda: self.ctl.send({"type": "gesture", "value": "double_tap"})),
            ("Удержание", lambda: self.ctl.send({"type": "gesture", "value": "long_press"})),
            ("Свайп ←", lambda: self.ctl.send({"type": "gesture", "value": "swipe_left"})),
            ("Свайп →", lambda: self.ctl.send({"type": "gesture", "value": "swipe_right"})),
            ("Свайп ↑", lambda: self.ctl.send({"type": "gesture", "value": "swipe_up"})),
            ("Свайп ↓", lambda: self.ctl.send({"type": "gesture", "value": "swipe_down"})),
        ]]))
        chat = QHBoxLayout()
        self.chat_edit = QLineEdit(placeholderText="Текст питомцу (автономный AI)")
        send = QPushButton("Сказать")
        send.clicked.connect(lambda: self.ctl.send({"type": "chat", "text": self.chat_edit.text()}))
        chat.addWidget(self.chat_edit, 1)
        chat.addWidget(send)
        lay.addLayout(chat)
        lay.addStretch(1)
        return w

    def _pet_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.addWidget(grid([(t, lambda v=v: self.ctl.send({"type": "action", "value": v})) for t, v in
                            [("Покормить", "feed"), ("Погладить", "pet"), ("Поиграть", "play"), ("Уложить спать", "sleep"), ("Разбудить", "wake")]]))
        form = QFormLayout()
        for s in STATS:
            sl = QSlider(Qt.Horizontal)
            sl.setRange(0, 100)
            on_slider_set(sl, lambda v, s=s: self.ctl.send({"type": "set_stat", "name": s, "value": v}))
            self.sliders[s] = sl
            form.addRow(STAT_RU[s], sl)
        lay.addLayout(form)
        self.persona = QComboBox()
        self.persona.addItems(["playful", "calm", "grumpy", "shy"])
        self.persona.currentTextChanged.connect(lambda v: self.ctl.send({"type": "setting", "name": "personality", "value": v}))
        form.addRow("Характер", self.persona)
        lay.addStretch(1)
        return w

    def _sensors_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.addWidget(QLabel("Через настоящие детекторы ядра (GestureManager, MicrophoneManager):"))
        lay.addWidget(grid([("Встряхнуть", self.ctl.imu.shake), ("Резкое движение", self.ctl.imu.jerk), ("Перевернуть экран", self.ctl.imu.flip),
                            ("Громкий звук", self.ctl.micsim.loud), ("Хлопок", self.ctl.micsim.clap), ("Голос", self.ctl.micsim.voice)]))
        lay.addWidget(QLabel("Наклон для лабиринта и орбиты: стрелки клавиатуры или правая кнопка мыши на экране."))
        form = QFormLayout()
        self.weather = QComboBox()
        self.weather.addItems(["clear", "clouds", "rain", "snow", "storm", "fog", "wind"])
        self.temp = QSpinBox()
        self.temp.setRange(-40, 50)
        self.temp.setValue(18)
        wb = QPushButton("Применить погоду")
        wb.clicked.connect(lambda: self.ctl.send({"type": "weather", "value": self.weather.currentText(), "temp": self.temp.value()}))
        form.addRow("Погода", self.weather)
        form.addRow("Температура", self.temp)
        form.addRow(wb)
        self.batt = QSlider(Qt.Horizontal)
        self.batt.setRange(0, 100)
        self.batt.setValue(80)
        self.charging = QCheckBox("Заряжается")
        self.batt_present = QCheckBox("Батарея подключена")
        self.batt_present.setChecked(True)
        for x in (self.charging, self.batt_present):
            x.toggled.connect(lambda _=False: self._apply_battery())
        self.batt.valueChanged.connect(lambda _=0: self._apply_battery())
        form.addRow("Заряд", self.batt)
        form.addRow(self.charging)
        form.addRow(self.batt_present)
        self.dt = QDateTimeEdit(datetime.datetime.now())
        self.dt.setDisplayFormat("dd.MM.yyyy HH:mm:ss")
        setb = QPushButton("Установить время")
        setb.clicked.connect(lambda: self.ctl.send({"type": "set_time", "epoch": int(self.dt.dateTime().toSecsSinceEpoch()), "tz_minutes": self.dt.dateTime().offsetFromUtc() // 60}))
        form.addRow(self.dt)
        form.addRow(setb)
        self.scale = QComboBox()
        self.scale.addItems(["×1", "×10", "×60", "×600", "×3600"])
        self.scale.currentTextChanged.connect(lambda t: self.ctl.set_time_scale(float(t[1:])))
        form.addRow("Ускорение времени", self.scale)
        self.mic_on = QCheckBox("Микрофоны")
        self.mic_on.setChecked(True)
        self.mic_on.toggled.connect(lambda v: self.ctl.send({"type": "setting", "name": "mic", "value": v}))
        self.snd_on = QCheckBox("Звук")
        self.snd_on.setChecked(True)
        self.snd_on.toggled.connect(lambda v: self.ctl.send({"type": "setting", "name": "sound", "value": v}))
        form.addRow(self.mic_on)
        form.addRow(self.snd_on)
        self.vol = QSlider(Qt.Horizontal)
        self.vol.setRange(0, 100)
        on_slider_set(self.vol, lambda v: self.ctl.send({"type": "setting", "name": "volume", "value": v}))
        form.addRow("Громкость", self.vol)
        self.bright = QSlider(Qt.Horizontal)
        self.bright.setRange(5, 100)
        on_slider_set(self.bright, lambda v: self.ctl.send({"type": "setting", "name": "brightness", "value": v}))
        form.addRow("Яркость", self.bright)
        self.mic_meter = QProgressBar(textVisible=False)
        self.mic_meter.setRange(0, 100)
        form.addRow("Уровень микрофона", self.mic_meter)
        self.out_meter = QProgressBar(textVisible=False)
        self.out_meter.setRange(0, 100)
        form.addRow("Выход звука", self.out_meter)
        lay.addLayout(form)
        lay.addStretch(1)
        return w

    def _apply_battery(self):
        self.ctl.companion.set_battery(self.batt.value(), self.charging.isChecked(), self.batt_present.isChecked(), 3300 + self.batt.value() * 9)

    def _screens_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.addWidget(grid([(t, lambda v=v: self.ctl.send({"type": "screen", "value": v})) for v, t in SCREENS]))
        form = QFormLayout()
        self.n_title = QLineEdit("Сообщение")
        self.n_text = QLineEdit("Привет! Как дела?")
        self.n_icon = QComboBox()
        self.n_icon.addItems(["message", "bell", "mail", "phone", "calendar", "warning"])
        form.addRow("Заголовок", self.n_title)
        form.addRow("Текст", self.n_text)
        form.addRow("Иконка", self.n_icon)
        b = QPushButton("Отправить уведомление")
        b.clicked.connect(lambda: self.ctl.send({"type": "notify", "title": self.n_title.text(), "text": self.n_text.text(), "icon": self.n_icon.currentText(), "source": "emulator"}))
        form.addRow(b)
        lay.addLayout(form)
        lay.addStretch(1)
        return w

    def _sounds_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.addWidget(QLabel("Предпрослушивание: каждая кнопка играет файл через микшер ядра."))
        cats = {}
        for name in self.ctl.companion.sounds():
            cats.setdefault(name.split("/")[0], []).append(name)
        for cat, names in cats.items():
            box = QGroupBox(cat)
            v = QVBoxLayout(box)
            mute = QCheckBox("Категория включена")
            mute.setChecked(True)
            key = {"emotions": "cat_emotions", "interactions": "cat_interactions", "system": "cat_system", "games": "cat_games"}[cat]
            mute.toggled.connect(lambda val, k=key: self.ctl.send({"type": "setting", "name": k, "value": val}))
            v.addWidget(mute)
            v.addWidget(grid([(n.split("/")[1], lambda n=n: self.ctl.send({"type": "play_sound", "value": n})) for n in names]))
            lay.addWidget(box)
        lay.addWidget(grid([("Прослушать все по очереди", self.ctl.play_all_sounds)], 1))
        lay.addStretch(1)
        return w

    def _hil_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.addWidget(QLabel("Hardware-in-the-Loop: команды панели идут на устройство, а его эмоция\n"
                             "и статистика повторяются здесь. Без платы подойдёт socket://127.0.0.1:7777\n(python tools/sim_device.py)."))
        form = QFormLayout()
        self.port = QComboBox(editable=True)
        self.port.addItems(self.ctl.serial_ports() + ["socket://127.0.0.1:7777"])
        form.addRow("USB-порт", self.port)
        lay.addLayout(form)
        row = QHBoxLayout()
        b = QPushButton("Подключить USB")
        b.clicked.connect(lambda: self.ctl.hil_serial(self.port.currentText()))
        row.addWidget(b)
        b = QPushButton("Найти и подключить BLE")
        b.clicked.connect(self.ctl.hil_ble)
        row.addWidget(b)
        b = QPushButton("Отключить")
        b.clicked.connect(self.ctl.hil_close)
        row.addWidget(b)
        lay.addLayout(row)
        self.pin = QLineEdit(placeholderText="PIN для BLE (экран «Об устройстве»)")
        lay.addWidget(self.pin)
        b = QPushButton("Запросить состояние устройства")
        b.clicked.connect(lambda: self.ctl.send({"type": "get_state"}))
        lay.addWidget(b)
        self.hil_label = QLabel("Режим: эмуляция")
        lay.addWidget(self.hil_label)
        self.raw = QLineEdit(placeholderText='Сырая команда: {"type":"ping"}')
        self.raw.returnPressed.connect(self._raw)
        lay.addWidget(self.raw)
        lay.addStretch(1)
        return w

    def _raw(self):
        try:
            self.ctl.send(json.loads(self.raw.text()))
        except ValueError as e:
            self.log_line.emit(f"JSON: {e}")

    def update_state(self, st, mic_db, out_level):
        txt = f"<b>{st.get('emotion')}</b> · {st.get('pet_status')} · экран {st.get('screen')} · 🔋{st.get('battery')}% · непрочитано {st.get('unread')}"
        self.status.setText(txt)
        for s, sl in self.sliders.items():
            if s in st and not sl.isSliderDown():
                sl.setValue(int(st[s]))
        for sl, key in ((self.vol, "volume"), (self.bright, "brightness")):
            if key in st and not sl.isSliderDown():
                sl.setValue(int(st[key]))
        self.mic_meter.setValue(int(max(0, min(100, (mic_db + 70) / 70 * 100))))
        self.out_meter.setValue(int(out_level * 100))

"""Drives the real emulator window: clicks the panel buttons and the screen with the mouse, as a user does."""
import os
import sys
import time
from pathlib import Path

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from PySide6.QtCore import QPoint, Qt  # noqa: E402
from PySide6.QtTest import QTest  # noqa: E402
from PySide6.QtWidgets import QApplication, QPushButton  # noqa: E402
from emulator.main import Emulator  # noqa: E402
from emulator.rendering.screen_widget import BEZEL  # noqa: E402
from emulator.ui.panel import SCREENS  # noqa: E402

app = QApplication.instance() or QApplication([])
HIL_BUTTONS = {"Подключить USB", "Найти и подключить BLE", "Отключить"}


@pytest.fixture
def emu(tmp_path):
    errors = []
    old = sys.excepthook
    sys.excepthook = lambda t, v, tb: errors.append(repr(v))  # exceptions in Qt slots do not reach pytest otherwise
    win = Emulator(storage=tmp_path, silent=True)
    win.show()
    win.errors = errors
    pump(300)
    yield win
    win.close()
    sys.excepthook = old
    assert not errors, errors


def pump(ms):
    end = time.perf_counter() + ms / 1000
    while time.perf_counter() < end:
        app.processEvents()
        time.sleep(0.002)


def button(win, title, tab=None):
    # Emotion and sound buttons share names, so those lookups name their tab.
    root = win.panel.tabs.widget(tab) if tab is not None else win.panel
    found = [b for b in root.findChildren(QPushButton) if b.text() == title]
    assert found, title
    return found[0]


def click(win, title, wait=120, tab=None):
    button(win, title, tab).click()
    pump(wait)


def state(win):
    return win.companion.state()


def test_feed_button_feeds(emu):
    emu.companion.handle_command({"type": "set_stat", "name": "hunger", "value": 80})
    click(emu, "Покормить")
    assert state(emu)["hunger"] <= 46


def test_every_action_changes_the_pet(emu):
    a0 = state(emu)["affection"]
    click(emu, "Погладить")
    assert state(emu)["affection"] > a0
    f0 = state(emu)["fun"]
    click(emu, "Поиграть")
    assert state(emu)["fun"] > f0
    click(emu, "Уложить спать")
    assert state(emu)["pet_status"] == "asleep"
    click(emu, "Разбудить")
    assert state(emu)["pet_status"] != "asleep"


@pytest.mark.parametrize("name,title", SCREENS)
def test_every_screen_button_opens_screen_and_draws(emu, name, title):
    if name == "face":
        click(emu, "Меню")
    click(emu, title, 350)
    assert state(emu)["screen"] == name
    assert (emu.screen.fb != 0).sum() > 200


def test_every_emotion_button(emu):
    for name in emu.emotions:
        click(emu, name, 60, tab=0)
        seen = set()
        for _ in range(60):  # illegal jumps are routed through other emotions, which takes a moment
            seen.add(state(emu)["emotion"])
            if name in seen:
                break
            pump(100)
        assert name in seen, (name, seen)


def test_every_sound_button_produces_audio(emu):
    # The window is silent here, so the test takes the place of the sound card and pulls from the mixer.
    for name in emu.companion.sounds():
        emu.companion.mix(32000)
        click(emu, name.split("/")[1], 10, tab=4)
        assert abs(emu.companion.mix(4000)).max() > 300, name


def test_sensor_buttons_reach_the_detectors(emu):
    seen = set()
    click(emu, "Встряхнуть", 10)
    for _ in range(40):
        pump(30)
        seen.add(state(emu)["emotion"])
    assert seen & {"surprised", "confused"}, seen
    click(emu, "Перевернуть экран", 2200)
    assert state(emu)["pet_status"] == "asleep"
    click(emu, "Перевернуть экран", 500)
    assert state(emu)["pet_status"] != "asleep"


def test_shake_interrupts_a_forced_emotion(emu):
    click(emu, "curious", 900, tab=0)
    click(emu, "Встряхнуть", 10)
    seen = set()
    for _ in range(40):
        pump(30)
        seen.add(state(emu)["emotion"])
    assert seen & {"surprised", "confused"}, seen


def test_notification_button(emu):
    click(emu, "Отправить уведомление")
    assert state(emu)["unread"] == 1


def test_all_buttons_can_be_clicked_without_errors(emu):
    for b in emu.panel.findChildren(QPushButton):
        if b.text() in HIL_BUTTONS or b is emu.panel.demo_btn:
            continue
        b.click()
        app.processEvents()
    pump(200)


def test_demo_button_runs(emu):
    emu.panel.demo_btn.click()
    assert emu.demo_queue
    pump(3000)
    emu.panel.demo_btn.click()
    assert not emu.demo_queue


def mouse_point(x, y):
    return QPoint(x + BEZEL, y + BEZEL)


def test_mouse_click_opens_menu_and_click_selects(emu):
    """Design 1a: a tap on the face opens the menu, a tap brings a row to the centre, a tap there opens it."""
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 233))
    pump(700)  # the face waits for a possible double tap first
    assert state(emu)["screen"] == "menu"
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 233 + 128))
    pump(400)
    # An instant click, shorter than one frame, must still count.
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 233))
    pump(400)
    assert state(emu)["screen"] == "pet"


def test_mouse_clicks_pet_screen_buttons(emu):
    click(emu, "Питомец", 300)
    emu.companion.handle_command({"type": "set_stat", "name": "hunger", "value": 80})
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233 - 136, 346 - 12 + 22))
    pump(200)
    assert state(emu)["hunger"] <= 46


def test_mouse_swipe_changes_screen(emu):
    QTest.mousePress(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(360, 233))
    for x in range(340, 100, -40):
        QTest.mouseMove(emu.screen, mouse_point(x, 233))
        pump(20)
    QTest.mouseRelease(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(100, 233))
    pump(300)
    assert state(emu)["screen"] == "pet"


def test_real_sound_card_receives_samples(tmp_path):
    win = Emulator(storage=tmp_path, silent=False)
    try:
        if not win.audio.stream:
            pytest.skip(f"no sound device: {win.audio.error}")
        win.show()
        pump(200)
        win.out_level = 0
        peak = 0
        button(win, "excited", 0).click()
        for _ in range(100):
            pump(10)
            peak = max(peak, win.out_level)
        assert peak > 0.02
    finally:
        win.close()


def test_arrow_keys_tilt_and_weather_button(emu):
    QTest.keyPress(emu, Qt.Key_Right)
    pump(100)
    assert emu.imu.tilt_x > 0.3
    assert emu.companion.diag()["ax"] > 0.3
    QTest.keyRelease(emu, Qt.Key_Right)
    pump(60)
    assert emu.imu.tilt_x == 0
    emu.panel.weather.setCurrentText("snow")
    click(emu, "Применить погоду")
    assert state(emu)["weather"] == "snow"


def test_right_mouse_tilts(emu):
    QTest.mousePress(emu.screen, Qt.RightButton, Qt.NoModifier, mouse_point(430, 233))
    pump(40)
    assert emu.imu.tilt_x > 0.5
    QTest.mouseRelease(emu.screen, Qt.RightButton, Qt.NoModifier, mouse_point(430, 233))
    pump(40)
    assert emu.imu.tilt_x == 0


class FakePhone:
    """Speaks to the emulator exactly as the Android app's TcpLine does."""
    def __init__(self, port):
        import socket
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=2)
        self.sock.settimeout(0.02)
        self.buf = b""
        self.got = []

    def send(self, **cmd):
        import json
        self.sock.sendall((json.dumps(cmd, ensure_ascii=False) + "\n").encode())

    def read(self, ms=300):
        import json
        end = time.perf_counter() + ms / 1000
        while time.perf_counter() < end:
            app.processEvents()
            try:
                self.buf += self.sock.recv(4096)
            except OSError:
                pass
            while b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                self.got.append(json.loads(line))
        out, self.got = self.got, []
        return out

    def close(self):
        self.sock.close()


@pytest.fixture
def phone(emu):
    emu.set_android_server(True, 7893)
    p = FakePhone(7893)
    yield p
    p.close()


def test_network_requires_pin(emu, phone):
    phone.send(type="get_state")
    assert phone.read()[0]["error"] == "auth_required"
    phone.send(type="hello")
    assert phone.read()[0]["auth_required"] is True
    phone.send(type="auth", pin="000000x")
    assert phone.read()[0]["error"] == "auth_failed"


def test_stranger_gets_no_stream(emu, phone):
    emu.companion.handle_command({"type": "controller", "value": "start", "mode": "pad"})
    pump(200)
    assert not [m for m in phone.read(300) if m.get("event")]


def test_phone_controls_emulator_and_receives_controller_input(emu, phone):
    phone.send(type="auth", pin=emu.companion.pin(), request_id=1)
    assert phone.read()[0]["status"] == "success"
    phone.send(type="action", value="feed", request_id=2)
    assert phone.read()[0]["request_id"] == 2
    phone.send(type="controller", value="start", mode="cards", labels="As,Kh,7d", title="Дурак", hint="наклон выбор")
    phone.read(200)
    assert state(emu)["screen"] == "controller"
    QTest.keyPress(emu, Qt.Key_Right)
    msgs = phone.read(400)
    assert any(m.get("event") == "input" and m.get("ax", 0) > 0.3 for m in msgs), "tilt reaches the phone"
    assert any(m.get("action") == "select" for m in msgs), "card choice reaches the phone"
    QTest.keyRelease(emu, Qt.Key_Right)
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 233))
    msgs = phone.read(300)
    assert any(m.get("action") == "tap" for m in msgs), "the click on the emulated screen reaches the phone"
    phone.send(type="controller", value="stop")
    phone.read(100)
    assert state(emu)["screen"] == "face"


def test_bad_lines_from_network_do_not_hurt(emu, phone):
    phone.sock.sendall(b"not json\n{\n" + b"x" * 5000 + b"\n")
    replies = phone.read(300)
    assert replies and all(r["status"] == "error" for r in replies)
    pump(100)
    assert emu.companion.state()["status"] == "success"


def test_phone_disconnect_is_harmless(emu):
    emu.set_android_server(True, 7894)
    p = FakePhone(7894)
    p.send(type="auth", pin=emu.companion.pin())
    p.read(100)
    p.close()
    emu.companion.handle_command({"type": "controller", "value": "start", "mode": "pad"})
    pump(400)
    assert emu.server is not None


def test_mouse_mode_feeds_test_pad(emu):
    click(emu, "Мышь", 200, tab=3)
    assert state(emu)["screen"] == "mouse"
    x0 = emu.panel.mouse_pad.x
    QTest.keyPress(emu, Qt.Key_Right)
    pump(300)
    QTest.keyRelease(emu, Qt.Key_Right)
    assert emu.panel.mouse_pad.x > x0
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(180, 180))
    pump(500)
    assert emu.panel.mouse_pad.lefts == 1


def test_idle_phone_stays_connected(emu, phone):
    """The socket timeout must not end a session: a phone is silent most of the time."""
    phone.send(type="auth", pin=emu.companion.pin())
    phone.read(100)
    pump(2500)
    phone.send(type="ping", request_id=9)
    assert any(m.get("request_id") == 9 for m in phone.read(300))


def test_oversized_line_without_newline_is_answered_once(emu, phone):
    phone.sock.sendall(b"x" * 3000)
    replies = phone.read(300)
    assert replies and all(r["status"] == "error" for r in replies)


def test_home_network_address_is_offered_before_vpn():
    from emulator.communication.tcp_server import rank_addresses
    assert rank_addresses(["169.254.254.214", "172.18.0.1", "192.168.3.72", "127.0.0.1"]) == ["192.168.3.72", "172.18.0.1"]
    assert rank_addresses(["172.18.0.1", "10.0.0.5"]) == ["10.0.0.5", "172.18.0.1"]
    assert rank_addresses(["127.0.0.1"]) == []


def test_panel_shows_a_reachable_looking_address(emu):
    emu.set_android_server(True, 7895)
    assert "PIN" in emu.panel.net_label.text() and "Адрес для телефона" in emu.panel.net_label.text()


def test_second_server_on_a_busy_port_refuses_instead_of_sharing():
    from emulator.communication.tcp_server import TcpServer
    first = TcpServer(7897)
    try:
        with pytest.raises(OSError):
            TcpServer(7897)
    finally:
        first.close()

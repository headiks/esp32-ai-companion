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


def test_mouse_hold_opens_menu_and_click_selects(emu):
    QTest.mousePress(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 233))
    pump(800)
    QTest.mouseRelease(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 233))
    pump(350)
    assert state(emu)["screen"] == "menu"
    # An instant click, shorter than one frame, must still count.
    QTest.mouseClick(emu.screen, Qt.LeftButton, Qt.NoModifier, mouse_point(233, 72 + 66 * 2 + 30))
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

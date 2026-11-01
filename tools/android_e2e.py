"""End-to-end check: the real Android app on a running Android emulator talks to the real device emulator.
Start an Android emulator first (or attach a phone with adb), install app-debug.apk, then:
    python tools/android_e2e.py [folder for screenshots]
Taps go through adb and uiautomator, so this exercises the actual interface and the actual network link."""
import os
import re
import subprocess
import sys
import tempfile
import threading
import time
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication  # noqa: E402
from emulator.main import Emulator  # noqa: E402

ADB = str(Path(os.environ.get("ANDROID_HOME") or Path(os.environ.get("LOCALAPPDATA", "")) / "Android" / "Sdk") / "platform-tools" / ("adb.exe" if os.name == "nt" else "adb"))
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp())
OUT.mkdir(parents=True, exist_ok=True)
PORT = 7796
app = QApplication([])
emu = Emulator(storage=tempfile.mkdtemp(), silent=True)
emu.show()
results = []


def pump(seconds):
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        app.processEvents()
        time.sleep(0.003)


def adb(*args, binary=False):
    """Runs adb while the emulator window keeps living: it serves the phone's commands in its frames."""
    box = {}

    def work():
        box["r"] = subprocess.run([ADB, *args], capture_output=True)
    t = threading.Thread(target=work)
    t.start()
    while t.is_alive():
        pump(0.02)
    return box["r"].stdout if binary else box["r"].stdout.decode("utf-8", "replace")


def nodes():
    adb("shell", "uiautomator", "dump", "/sdcard/ui.xml")
    xml = adb("shell", "cat", "/sdcard/ui.xml")
    return list(ET.fromstring(xml[xml.index("<?xml"):]).iter("node")) if "<?xml" in xml else []


def texts():
    return [n.get("text") for n in nodes() if n.get("text")]


def find(pattern):
    for n in nodes():
        if n.get("text") and re.search(pattern, n.get("text")):
            return n
    return None


def visible(*patterns):
    # "is not None": an ElementTree element without children counts as false.
    return any(find(p) is not None for p in patterns)


def tap_node(n):
    a, b, c, d = map(int, re.findall(r"\d+", n.get("bounds")))
    adb("shell", "input", "tap", str((a + c) // 2), str((b + d) // 2))


def tap(pattern, wait=0.8, required=True):
    n = find(pattern)
    if n is None:
        if required:
            raise AssertionError(f"not on screen: {pattern}; visible: {texts()[:30]}")
        return False
    tap_node(n)
    pump(wait)
    return True


def shot(name):
    (OUT / f"{name}.png").write_bytes(adb("exec-out", "screencap", "-p", binary=True))
    emu.grab().save(str(OUT / f"{name}_emulator.png"))


def frames_differ(seconds=1.5):
    """Compares two screenshots: a frozen canvas once passed every check that only read text."""
    from io import BytesIO
    from PIL import Image, ImageChops
    a = Image.open(BytesIO(adb("exec-out", "screencap", "-p", binary=True))).convert("L")
    pump(seconds)
    b = Image.open(BytesIO(adb("exec-out", "screencap", "-p", binary=True))).convert("L")
    w, h = a.size
    box = (0, h // 3, w, h - 150)  # the game area, without the clock in the status bar
    return ImageChops.difference(a.crop(box), b.crop(box)).getbbox() is not None


def rank_cards_on_screen():
    return [t for t in texts() if re.fullmatch(r"(10|[2-9JQKA])", t)]


def check(name, ok, detail=""):
    results.append((name, bool(ok)))
    print(("OK  " if ok else "FAIL"), name, detail)


def state():
    return emu.companion.state()


def device_tap():
    emu.touch_queue += [(True, 233, 233), (True, 233, 233), (True, 233, 233), (False, 233, 233)]
    pump(0.25)


def device_swipe_up():
    emu.touch_queue += [(True, 233, 350)] + [(True, 233, 350 - i * 30) for i in range(1, 9)] + [(False, 233, 110)]
    pump(0.5)


def main():
    # Its own port, so an emulator window left open on 7777 is not mistaken for this one.
    emu.set_android_server(True, PORT)
    if not emu.server:
        raise SystemExit(f"Port {PORT} is busy")
    adb("shell", "pm", "clear", "com.companion.app")  # a saved PIN from an earlier run would get the new one appended
    adb("shell", "am", "start", "-W", "-n", "com.companion.app/.MainActivity")
    fields = []
    deadline = time.time() + 40  # a cold start on a software-rendered emulator is slow
    while time.time() < deadline and len(fields) < 2:
        tap(r"^(Allow|Разрешить)$", required=False)
        fields = [n for n in nodes() if n.get("class") == "android.widget.EditText"]
        pump(0.5)

    pin = emu.companion.pin()
    check("connection tab has PIN and address fields", len(fields) >= 2)
    # A slow virtual phone can drop typed characters, so read the field back before trusting it.
    for _ in range(4):
        tap_node(fields[0])
        pump(0.6)
        adb("shell", "input", "keyevent", "KEYCODE_MOVE_END")
        for _ in range(8):
            adb("shell", "input", "keyevent", "KEYCODE_DEL")
        adb("shell", "input", "text", pin)
        pump(1.2)
        fields = [n for n in nodes() if n.get("class") == "android.widget.EditText"]
        typed = fields[0].get("text")
        if typed == pin:
            break
    check("PIN typed correctly", typed == pin, f"field holds {typed!r}")
    tap_node(fields[1])
    pump(0.6)
    adb("shell", "input", "keyevent", "KEYCODE_MOVE_END")
    adb("shell", "input", "text", f":{PORT}")
    pump(1)
    adb("shell", "input", "keyevent", "111")  # the keyboard would hide buttons
    pump(0.5)
    tap(r"^Эмулятор$", 2.5)
    shot("1_connected")
    check("app shows it is connected", find("подключено") is not None, str(texts()[:6]))
    check("wrong PIN error absent", find("Неверный PIN") is None)
    check("time came from the phone", abs(state()["time"] - time.time()) < 120)

    tap(r"^Питомец$")
    emu.companion.handle_command({"type": "set_stat", "name": "hunger", "value": 80})
    tap(r"^Покормить$", 1.0)
    check("feeding from the phone changes the emulator", state()["hunger"] <= 46, f"hunger={state()['hunger']}")
    emu.companion.handle_command({"type": "screen", "value": "face"})

    tap(r"^Игры$")
    shot("2_games")
    tap(r"^Дурак$")
    tap(r"^Подкидной$", 2)
    shot("3_durak_start")
    check("durak turns the emulator into a card controller", state()["screen"] == "controller")
    pump(1)
    check("the phone does not show the player's hand in durak", find("на устройстве") is not None and len(rank_cards_on_screen()) <= 12, str(texts()[:16]))

    # Wait for the human's move, then choose by tilt and throw with a swipe, as with the real device.
    deadline = time.time() + 25
    while time.time() < deadline and not visible("Ваш ход", "Отбивайтесь"):
        pump(0.5)
    check("durak reaches the human's move", visible("Ваш ход", "Отбивайтесь"), str(texts()[:12]))
    emu.set_tilt(0.5, 0)
    pump(1.0)
    emu.set_tilt(0, 0)
    pump(0.3)
    shot("4_durak_chosen")
    device_tap()
    pump(0.8)
    shot("5_durak_marked")
    device_swipe_up()
    pump(1.5)
    shot("6_durak_played")
    check("a swipe on the device reaches the game", visible("Ходит игрок", "Подкидывайте", "Этими картами", "сюда не подходят"), str(texts()[:14]))

    tap(r"^Назад$", 1.5)
    check("leaving the game releases the controller", state()["screen"] == "face", state()["screen"])
    tap(r"^Гонки$", 1.5)
    check("racing starts the pad", state()["screen"] == "controller")
    emu.set_tilt(0.4, 0)
    pump(1.5)
    emu.set_tilt(0, 0)
    shot("7_race")
    check("the race score counts on screen", any(re.fullmatch(r"[1-9]\d*", t) for t in texts()), str(texts()[:14]))
    check("the race picture moves", frames_differ())
    emu.set_tilt(-0.4, 0)
    pump(0.6)
    check("the race picture follows the tilt", frames_differ(0.6))
    emu.set_tilt(0, 0)
    tap(r"^Назад$", 1)

    tap(r"^Арканоид$", 1.5)
    check("breakout starts the pad", state()["screen"] == "controller")
    device_tap()
    pump(0.4)
    check("the breakout ball flies after a tap on the device", frames_differ(0.6))
    shot("8_breakout")
    tap(r"^Назад$", 1)

    tap(r"^Покер$", 1.5)
    tap(r"^Трое$", 2.5)
    shot("9_poker_table")
    check("poker turns the emulator into a card controller", state()["screen"] == "controller")
    check("the phone hides the two hole cards", visible("Ваши карты на устройстве") and len(rank_cards_on_screen()) == 0, str(texts()[:16]))
    shot("9b_poker_device")
    # Check or call from the device until the hand is over; every street must add shared cards to the table.
    board_seen = set()
    for _ in range(40):
        if visible("Вскрытие", "банк ваш", "забирает банк"):
            break
        board_seen.add(len(rank_cards_on_screen()))
        device_tap()
        pump(1.2)
    shot("10_poker_showdown")
    finished = visible("Вскрытие", "банк ваш", "забирает банк")
    check("the hand played from the device finishes", finished, str(texts()[:24]))
    check("shared cards appear on the table", max(board_seen | {0}) >= 3 or visible("банк ваш", "забирает банк"), str(sorted(board_seen)))
    device_tap()
    pump(2.5)
    shot("11_poker_next_hand")
    check("a tap on the device deals the next hand", visible("Префлоп", "Флоп", "Тёрн", "Ривер"), str(texts()[:24]))
    tap(r"^Назад$", 1)

    crash = adb("logcat", "-d", "-s", "AndroidRuntime:E")
    check("no crash in the app log", "FATAL" not in crash, crash[-400:] if "FATAL" in crash else "")
    emu.close()
    failed = [n for n, ok in results if not ok]
    print(f"\n{len(results) - len(failed)}/{len(results)} passed. Screenshots: {OUT}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    try:
        main()
    finally:
        emu.close()

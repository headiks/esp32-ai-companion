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
sys.stdout.reconfigure(errors="replace")  # the app shows emoji; a cp1251 Windows console cannot print them

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
        label = n.get("text") or n.get("content-desc")  # icon buttons carry their name in content-desc
        if label and re.search(pattern, label):
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
    deadline = time.time() + 8  # a slow virtual phone can take seconds to draw the next screen
    while n is None and required and time.time() < deadline:
        pump(0.5)
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


def white_center(top, bottom):
    """Horizontal centre of the near-white pixels in a band of the screen, as a fraction of its width."""
    from io import BytesIO
    from PIL import Image
    img = Image.open(BytesIO(adb("exec-out", "screencap", "-p", binary=True))).convert("L")
    w, h = img.size
    band = img.crop((0, int(h * top), w, int(h * bottom))).point(lambda v: 255 if v > 225 else 0)
    box = band.getbbox()
    return ((box[0] + box[2]) / 2 / w) if box else None


def color_center(rgb, tolerance=8):
    """Centre of the pixels of exactly this colour on the game table, as fractions of the screen; (None, None) if absent."""
    from io import BytesIO
    from PIL import Image
    img = Image.open(BytesIO(adb("exec-out", "screencap", "-p", binary=True))).convert("RGB")
    w, h = img.size
    # The table only: the app's header and the standings have white and rose text of their own.
    top = int(h * 0.27)
    px = img.crop((0, top, w, h - 100)).load()
    found = [(x, y) for y in range(0, h - 100 - top, 3) for x in range(0, w, 3)
             if all(abs(px[x, y][i] - rgb[i]) <= tolerance for i in range(3))]
    if not found:
        return None, None
    return sum(p[0] for p in found) / len(found) / w, (sum(p[1] for p in found) / len(found) + top) / h


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
    deadline = time.time() + 120  # a cold start on a software-rendered emulator is slow
    while time.time() < deadline and len(fields) < 2:
        tap(r"^(Allow|Разрешить|Wait|Подождать)$", required=False)  # Wait: "not responding" on a phone still booting
        fields = [n for n in nodes() if n.get("class") == "android.widget.EditText"]
        if len(fields) == 1:
            tap(r"^Эмулятор на ПК$", required=False)  # the address field is folded away below the PIN
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
        now = [n for n in nodes() if n.get("class") == "android.widget.EditText"]
        fields = now or fields  # a dump taken mid-redraw can come back empty
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
    tap(r"^Подключить$", 2.5)
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
    tap(r"^Играть$", 2)  # the transfer-free mode is the default
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
    table_before = texts()[10:]
    device_swipe_up()
    pump(1.5)
    shot("6_durak_played")
    # Any change will do: a beaten attack can end the round, and the computer attacks again with the same words on screen.
    check("a swipe on the device reaches the game", texts()[10:] != table_before or visible("Этими картами", "сюда не подходят"), str(texts()[:14]))

    tap(r"^Назад$", 1.5)
    check("leaving the game releases the controller", state()["screen"] == "face", state()["screen"])
    tap(r"^Гонки$", 1.5)
    check("racing starts the pad", state()["screen"] == "controller")
    check("three tracks to choose from", all(find(n) is not None for n in ("Овал", "Восьмёрка", "Серпантин")), str(texts()[:16]))
    shot("7_race_setup")
    WHITE = (255, 255, 255)

    def race_with_lean(lean):
        """Average height of the first player's car while it drives holding this lean: a held lean makes it circle."""
        emu.set_tilt(lean, 0)
        tap(r"^Играть$", 0.1)
        pump(3.3)
        at = [color_center(WHITE) for _ in range(5)]
        emu.set_tilt(0, 0)
        return at, sum(p[1] for p in at) / len(at)

    at, right = race_with_lean(0.4)
    spread = max(p[0] for p in at) - min(p[0] for p in at) + max(p[1] for p in at) - min(p[1] for p in at)
    check("the car drives on its own after the countdown", spread > 0.03, str(at))
    tap(r"^Назад$", 1)
    tap(r"^Гонки$", 1.5)
    _, left = race_with_lean(-0.4)
    # The road starts going right, and right of that is down the screen.
    check("leaning the device steers the car to that side", right > left + 0.03, f"lean right y={right} lean left y={left}")
    check("standings show the lap", find(r"круг 1/3") is not None, str(texts()[:14]))
    shot("8_race")
    check("the race picture moves", frames_differ())
    tap(r"^Назад$", 1)

    tap(r"^Гонки$", 1.5)
    tap(r"^2$", 0.5)
    tap(r"^Восьмёрка$", 0.5)
    shot("9_race_two_setup")
    tap(r"^Играть$", 4.5)
    pads = [n for n in nodes() if n.get("text") == "▶"]
    check("the second player has buttons, the first has the device", len(pads) == 1, str(texts()[:16]))
    ROSE = (218, 176, 184)
    before = color_center(ROSE)
    a, b, c, d = map(int, re.findall(r"\d+", pads[0].get("bounds")))
    adb("shell", "input", "swipe", str((a + c) // 2), str((b + d) // 2), str((a + c) // 2), str((b + d) // 2), "1200")  # held, not tapped
    after = color_center(ROSE)
    check("the second player's car is on the track and moves", before[0] is not None and after[0] is not None and before != after, f"{before} -> {after}")
    shot("10_race_two")
    tap(r"^Назад$", 1)

    tap(r"^Арканоид$", 1.5)
    check("breakout starts the pad", state()["screen"] == "controller")
    check("breakout says how to launch", find("старт") is not None)
    PADDLE = (0.82, 0.86)  # the paddle's row of the screen, above the bottom navigation and its white labels
    emu.set_tilt(0.25, 0)
    pump(0.8)
    right = white_center(*PADDLE)
    emu.set_tilt(-0.25, 0)
    pump(0.8)
    left = white_center(*PADDLE)
    emu.set_tilt(0, 0)
    pump(0.8)
    middle = white_center(*PADDLE)
    check("the paddle goes where the device leans", right and left and middle and left < 0.4 < middle < 0.6 < right, f"left={left} middle={middle} right={right}")
    device_tap()
    pump(0.4)
    check("the breakout ball flies after a tap on the device", frames_differ(0.6))
    shot("8_breakout")
    tap(r"^Назад$", 1)

    tap(r"^Покер$", 1.5)
    tap(r"^3$", 0.5)
    tap(r"^Играть$", 2.5)
    shot("9_poker_table")
    check("poker turns the emulator into a card controller", state()["screen"] == "controller")
    check("the phone hides the two hole cards", visible("Ваши карты на устройстве") and len(rank_cards_on_screen()) == 0, str(texts()[:16]))
    shot("9b_poker_device")
    # Whose move it is decides what the first button says, so wait for the human's turn.
    deadline = time.time() + 15
    while time.time() < deadline and not visible(r"^Чек$", r"^Поддержать \d+$"):
        pump(0.5)
    check("the moves are check or call, raise and fold, and nothing else", visible(r"^Чек$", r"^Поддержать \d+$") and visible(r"^Поднять$") and visible(r"^Сбросить$"), str(texts()[:24]))
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

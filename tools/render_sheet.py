"""Renders emotions, screens and weather through the real core, for visual review or for design tools.
Usage:
    python tools/render_sheet.py out.png [emotions|screens|weather]   contact sheet of one group
    python tools/render_sheet.py OUT_DIR --each                        every frame as its own full-size PNG, all groups"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from core.companion import Companion, SCREEN, emotion_names, rgb565_to_rgb888  # noqa: E402

WEATHER = ["clear", "clouds", "rain", "snow", "storm", "fog", "wind"]
MODES = ["screens", "emotions", "weather"]


def snapshot(c, scale=0.5, transparent=False):
    """The round display as seen through the bezel: outside the circle is grey, or transparent for design tools."""
    img = Image.fromarray(rgb565_to_rgb888(c.render()))
    mask = Image.new("L", (SCREEN, SCREEN), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, SCREEN, SCREEN), fill=255)
    bg = Image.new("RGBA", (SCREEN, SCREEN), (0, 0, 0, 0)) if transparent else Image.new("RGB", (SCREEN, SCREEN), (40, 40, 44))
    bg.paste(img, (0, 0), mask)
    return bg if scale == 1 else bg.resize((int(SCREEN * scale), int(SCREEN * scale)), Image.LANCZOS)


def sheet(items, cols=6):
    w, h = items[0][1].size
    rows = (len(items) + cols - 1) // cols
    out = Image.new("RGB", (cols * (w + 6), rows * (h + 22)), (24, 24, 28))
    d = ImageDraw.Draw(out)
    for i, (name, im) in enumerate(items):
        x, y = (i % cols) * (w + 6), (i // cols) * (h + 22)
        out.paste(im, (x + 3, y + 18), im if im.mode == "RGBA" else None)
        d.text((x + 6, y + 3), name, fill=(255, 200, 80))
    return out


def collect(mode, scale=0.5, transparent=False):
    """(name, image) pairs for one group. Names are file names: lowercase, no spaces."""
    c = Companion(seed=7)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    c.handle_command({"type": "setting", "name": "blink_rate", "value": 0})  # a frame caught mid-blink says nothing
    c.handle_command({"type": "set_time", "epoch": 1767225600 + 12 * 3600 + 34 * 60 + 56, "tz_minutes": 0})
    c.run(3000)
    items = []

    def shot(name):
        items.append((name, snapshot(c, scale, transparent)))

    if mode == "emotions":
        for e in emotion_names():
            c.handle_command({"type": "action", "value": "wake"})
            c.run(1500)
            c.handle_command({"type": "emotion", "value": e, "hold_ms": 8000})
            c.run(1200 if e == "yawning" else 2600)
            assert c.state()["emotion"] == e, (e, c.state()["emotion"])
            shot(e)
    elif mode == "weather":
        for w in WEATHER:
            c.handle_command({"type": "weather", "value": w, "temp": 12})
            c.handle_command({"type": "screen", "value": "face"})
            c.run(1500)
            shot(f"face_{w}")
            c.handle_command({"type": "screen", "value": "weather"})
            c.run(700)
            shot(f"screen_{w}")
    else:
        for name, setup in SCENES:
            setup(c)
            shot(name)
    c.close()
    if mode not in ("emotions", "weather"):
        # Time not set: only a device that was never synchronised shows it.
        fresh = Companion(seed=7)
        fresh.handle_command({"type": "screen", "value": "digital_clock"})
        fresh.run(1000)
        items.insert(12, ("13_time_not_set", snapshot(fresh, scale, transparent)))
        fresh.close()
    return items


def _screen(name, ms=1000):
    def go(c):
        c.handle_command({"type": "screen", "value": name})
        c.run(ms)
    return go


def _steps(*fns):
    def go(c):
        for f in fns:
            f(c)
    return go


def _cmd(**cmd):
    return lambda c: c.handle_command(cmd)


def _tap(x, y):
    def go(c):
        c.touch(True, x, y); c.run(60); c.touch(False, x, y); c.run(60)
    return go


def _tilt(ax, ay, ms=600):
    def go(c):
        for _ in range(ms // 20):
            c.imu(ax, ay, 0.9, dt=20)
            c.tick(20)
        c.imu(0, 0, 1, dt=20)
        c.run(100)
    return go


# The screens of the 1a design, under its numbers, so that tools/compare_design.py can pair them with the reference.
SCENES = [
    ("01_face", _screen("face", 1500)),
    ("02_menu", _screen("menu")),
    # Without an AI server (always so in the emulator) the talk screen says why the voice is unavailable.
    ("03_talk", _steps(_screen("face", 300), _cmd(type="setting", name="ai_voice_consent", value=True), _cmd(type="ai_listen"),
                       lambda c: c.run(800))),
    ("04_notifications", _steps(*[_cmd(type="notify", title="Сообщение", text=t, icon="message") for t in
                                   ("Встреча в 15:00", "Посылка в пути", "Привет! Ты уже дома? Купи хлеба по дороге")],
                                lambda c: c.run(12000), _screen("notifications"))),  # arrival pills have gone by
    ("05_settings", _steps(_screen("settings"), _tap(233, 233 + 78), lambda c: c.run(600))),  # volume to the centre
    ("06_setting_brightness", _screen("setting_value")),
    ("07_diagnostics", _screen("diagnostics")),
    ("08_info", _screen("info")),
    ("09_digital_clock", _steps(_cmd(type="weather", value="rain", temp=12), _screen("digital_clock"))),
    ("10_analog_clock", _screen("analog_clock")),
    # The design's moment: 42.7 s, the second lap 19.3 s old.
    ("11_stopwatch", _steps(_screen("stopwatch", 300), _tap(284, 358), lambda c: c.run(23400), _tap(180, 358), lambda c: c.run(19300))),
    ("12_timer", _steps(_screen("timer", 300), _tap(233, 340), lambda c: c.run(48000))),  # 5 min, 4:12 left
    ("14_pet", _screen("pet")),
    ("15_pet_game", _screen("pet_game", 1500)),
    ("16_games", _screen("games")),
    ("17_game_start", _screen("reaction")),
    ("18_game_result", _screen("reaction")),
    ("19_maze", _screen("maze")),
    ("20_orbit", _screen("orbit")),
    ("21_snake", _screen("snake")),
    ("22_simon", _screen("simon")),
    ("23_tictactoe", _screen("tictactoe")),
    ("24_reaction", _screen("reaction")),
    ("25_guess", _screen("guess")),
    ("26_coin", _screen("coin")),
    ("27_dice", _screen("dice")),
    ("28_magic_ball", _screen("magic_ball")),
    ("29_balance", _screen("balance")),
    ("30_duel", _screen("duel")),
    ("31_shaker", _screen("shaker")),
    ("32_safe", _screen("safe")),
    ("33_workout", _screen("workout")),
    ("34_mole", _screen("mole")),
    ("35_blackjack", _screen("blackjack")),
    ("36_mouse", _screen("mouse")),
    # The device as a gamepad, with what the Android app sends it in each game.
    ("37_controller_durak", _steps(_cmd(type="controller", value="start", mode="cards", title="Козырь: пики", info="Отбивайтесь", target="Ks",
                                        labels="6s,6h,7d,8c,9s,10h,Jd,Qc,Ks,Ah,6d,7c,9h,10s", marks="01000000010000",
                                        hint="наклон выбор · тап отметить\n↑ бить · ↓ беру"), _tilt(0.35, 0))),
    ("38_controller_poker", _steps(_cmd(type="controller", value="start", mode="cards", labels="As,10h", marks="", title="Фишки 190",
                                        info="Префлоп · банк 30", hint="тап чек\n↑ поднять · ↓ сбросить"), lambda c: c.run(600))),
    ("39_controller_race", _steps(_cmd(type="controller", value="start", mode="pad", title="Гонки", hint="наклон руль · держи экран тормоз"),
                                  _tilt(0.4, 0.2))),
    ("40_ble_pairing", _steps(_cmd(type="controller", value="stop"), _screen("ble_pairing"))),
]


def main():
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "sheet.png")
    mode = sys.argv[2] if len(sys.argv) > 2 else "emotions"
    if mode != "--each":
        sheet(collect(mode)).save(path)
        return
    for m in MODES:
        items = collect(m, scale=1, transparent=True)
        (path / m).mkdir(parents=True, exist_ok=True)
        for name, im in items:
            im.save(path / m / f"{name}.png", optimize=True)
        sheet([(n, im.resize((233, 233), Image.LANCZOS)) for n, im in items]).save(path / f"{m}_overview.png")
        print(f"{m}: {len(items)} -> {path / m}")


if __name__ == "__main__":
    main()

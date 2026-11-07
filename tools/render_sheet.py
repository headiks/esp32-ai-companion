"""Renders emotions, screens and weather through the real core, for visual review or for design tools.
Usage:
    python tools/render_sheet.py out.png [emotions|screens|weather]   contact sheet of one group
    python tools/render_sheet.py OUT_DIR --each                        every frame as its own full-size PNG, all groups"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from core.companion import Companion, SCREEN, emotion_names, rgb565_to_rgb888  # noqa: E402

SCREENS = ["face", "menu", "pet", "weather", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications", "settings", "diagnostics", "games", "info",
           "maze", "orbit", "snake", "simon", "tictactoe", "reaction", "guess", "pet_game",
           "coin", "dice", "magic_ball", "balance", "duel", "shaker", "safe", "workout", "mole", "blackjack", "mouse"]
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
        c.handle_command({"type": "weather", "value": "rain", "temp": 12})
        c.handle_command({"type": "notify", "title": "Сообщение", "text": "Привет! Как дела?", "icon": "message"})
        c.run(5000)
        for s in SCREENS:
            c.handle_command({"type": "screen", "value": s})
            c.run(1000)
            shot(s)
        # The device as a gamepad, with what the Android app sends it in each game.
        c.handle_command({"type": "controller", "value": "start", "mode": "cards", "title": "Козырь: пики", "info": "Отбивайтесь", "target": "Ks",
                          "labels": "6s,6h,7d,8c,9s,10h,Jd,Qc,Ks,Ah,6d,7c,9h,10s", "marks": "01000000010000",
                          "hint": "наклон выбор · тап отметить\n↑ бить · ↓ беру"})
        for _ in range(30):
            c.imu(0.35, 0, 0.9, dt=20)
            c.tick(20)
        c.imu(0, 0, 1, dt=20)
        c.run(100)
        shot("controller_durak")
        c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,10h", "marks": "", "title": "Фишки 190",
                          "info": "Префлоп · банк 30", "hint": "тап чек\n↑ поднять · ↓ сбросить"})
        c.run(600)
        shot("controller_poker")
        c.handle_command({"type": "controller", "value": "start", "mode": "pad", "title": "Гонки", "hint": "наклон руль · держи экран тормоз"})
        c.imu(0.4, 0.2, 0.9, dt=20)
        c.run(600)
        shot("controller_race")
    c.close()
    return items


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

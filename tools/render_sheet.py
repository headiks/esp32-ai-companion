"""Renders emotions, screens and weather through the real core into a contact sheet PNG, for visual review.
Usage: python tools/render_sheet.py out.png [emotions|screens|weather]"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from core.companion import Companion, SCREEN, emotion_names, rgb565_to_rgb888  # noqa: E402

SCREENS = ["face", "menu", "pet", "weather", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications", "settings", "diagnostics", "games", "info",
           "maze", "orbit", "snake", "simon", "tictactoe", "reaction", "guess", "pet_game"]
WEATHER = ["clear", "clouds", "rain", "snow", "storm", "fog", "wind"]


def snapshot(c, scale=0.5):
    img = Image.fromarray(rgb565_to_rgb888(c.render()))
    mask = Image.new("L", (SCREEN, SCREEN), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, SCREEN, SCREEN), fill=255)
    bg = Image.new("RGB", (SCREEN, SCREEN), (40, 40, 44))
    bg.paste(img, (0, 0), mask)
    return bg.resize((int(SCREEN * scale), int(SCREEN * scale)), Image.LANCZOS)


def sheet(items, cols=6):
    w, h = items[0][1].size
    rows = (len(items) + cols - 1) // cols
    out = Image.new("RGB", (cols * (w + 6), rows * (h + 22)), (24, 24, 28))
    d = ImageDraw.Draw(out)
    for i, (name, im) in enumerate(items):
        x, y = (i % cols) * (w + 6), (i // cols) * (h + 22)
        out.paste(im, (x + 3, y + 18))
        d.text((x + 6, y + 3), name, fill=(255, 200, 80))
    return out


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "sheet.png"
    mode = sys.argv[2] if len(sys.argv) > 2 else "emotions"
    c = Companion(seed=7)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    c.handle_command({"type": "set_time", "epoch": 1767225600 + 12 * 3600 + 34 * 60 + 56, "tz_minutes": 0})
    c.run(3000)
    items = []
    if mode == "emotions":
        for e in emotion_names():
            c.handle_command({"type": "action", "value": "wake"})
            c.run(1500)
            c.handle_command({"type": "emotion", "value": e, "hold_ms": 8000})
            c.run(1200 if e == "yawning" else 2600)
            items.append((f"{e}:{c.state()['emotion']}", snapshot(c)))
    elif mode == "weather":
        for w in WEATHER:
            c.handle_command({"type": "weather", "value": w, "temp": 12})
            c.handle_command({"type": "screen", "value": "face"})
            c.run(1500)
            items.append((f"face {w}", snapshot(c)))
            c.handle_command({"type": "screen", "value": "weather"})
            c.run(700)
            items.append((f"screen {w}", snapshot(c)))
    else:
        c.handle_command({"type": "weather", "value": "rain", "temp": 12})
        c.handle_command({"type": "notify", "title": "Сообщение", "text": "Привет! Как дела?", "icon": "message"})
        c.run(5000)
        for s in SCREENS:
            c.handle_command({"type": "screen", "value": s})
            c.run(1000)
            items.append((s, snapshot(c)))
    sheet(items).save(path)
    c.close()


if __name__ == "__main__":
    main()

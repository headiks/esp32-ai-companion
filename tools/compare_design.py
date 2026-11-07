"""Puts every frame the core draws next to the 1a design reference and measures what the spec limits.
Usage: python tools/compare_design.py OUT_DIR
Writes OUT_DIR/compare_<group>.png (reference left, ours right) and OUT_DIR/report.md with, per frame:
- bright share: pixels close to INK, ACCENT or ACCENT_2 (or brighter), out of the round screen; the spec allows 15 %
- distance: mean grey-level difference to the reference at quarter size, 0 = identical."""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools.render_sheet import MODES, collect  # noqa: E402

REF = ROOT / "documentation/design/1a"
BRIGHT = np.array([(0xF4, 0xF1, 0xEA), (0xA9, 0xB4, 0xD8), (0xE8, 0xA0, 0x8E)])
LIMIT = 0.15


def bright_share(img):
    a = np.asarray(img.convert("RGBA")).astype(int)
    inside = a[..., 3] > 0
    rgb = a[..., :3]
    near = (np.abs(rgb[..., None, :] - BRIGHT).sum(-1) < 60).any(-1) | (rgb.min(-1) > 200)
    return float(near[inside].mean())


def distance(a, b):
    def grey(im):
        flat = Image.new("RGB", im.size, (0, 0, 0))
        flat.paste(im.convert("RGBA"), (0, 0), im.convert("RGBA"))
        return np.asarray(flat.convert("L").resize((116, 116), Image.BILINEAR)).astype(float)
    return float(np.abs(grey(a) - grey(b)).mean())


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "design_compare")
    out.mkdir(parents=True, exist_ok=True)
    lines = ["| Кадр | Яркие, наши | Яркие, макет | Расхождение |", "|---|---|---|---|"]
    over = []
    for mode in MODES:
        pairs = []
        for name, ours in collect(mode, scale=1, transparent=True):
            ref_path = REF / mode / f"{name}.png"
            ref = Image.open(ref_path).convert("RGBA") if ref_path.exists() else None
            share = bright_share(ours)
            if share >= LIMIT:
                over.append(name)
            lines.append(f"| {mode}/{name} | {share:.1%}{' ⚠' if share >= LIMIT else ''} | "
                         f"{bright_share(ref):.1%} | {distance(ref, ours):.1f} |" if ref else f"| {mode}/{name} | {share:.1%} | — | — |")
            pairs.append((name, ref, ours))
        w, gap, label = 233, 8, 18
        cols = 4
        rows = (len(pairs) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * (2 * w + 3 * gap), rows * (w + label + gap)), (24, 24, 28))
        d = ImageDraw.Draw(sheet)
        for i, (name, ref, ours) in enumerate(pairs):
            x, y = (i % cols) * (2 * w + 3 * gap), (i // cols) * (w + label + gap)
            d.text((x + 4, y + 3), name, fill=(255, 200, 80))
            for k, im in enumerate((ref, ours)):
                if im is not None:
                    small = im.resize((w, w), Image.LANCZOS)
                    sheet.paste(small, (x + k * (w + gap), y + label), small)
        sheet.save(out / f"compare_{mode}.png")
    summary = f"Кадров с долей ярких пикселей от {LIMIT:.0%}: {len(over)}" + (f" ({', '.join(over)})" if over else "")
    (out / "report.md").write_text("# Сравнение с макетом 1a\n\n" + summary + "\n\n" + "\n".join(lines) + "\n", encoding="utf-8")
    print(summary)


if __name__ == "__main__":
    main()

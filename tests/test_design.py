"""Acceptance rules of design 1a (documentation/design/1a/claude-code-prompt.md), checked on every frame the
reference covers: render_sheet draws the same states as the design's screens, emotions and weather."""
import re
import sys
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools.compare_design import LIMIT, REF, bright_share  # noqa: E402
from tools.gen_font import FONTS  # noqa: E402
from tools.render_sheet import MODES, collect  # noqa: E402

SRC = ROOT / "core/libcompanion"


@pytest.fixture(scope="module")
def frames():
    return {m: collect(m, scale=1, transparent=True) for m in MODES}


def test_text_and_buttons_stay_inside_r210(frames):
    out = [(m, n, round(im.info["reach"], 1)) for m in MODES for n, im in frames[m] if im.info["reach"] > 210]
    assert not out, out


def test_bright_pixels_under_15_percent(frames):
    """The design itself goes over on a few frames (a wide-open stare, the timer ring): ours may too, no further."""
    out = []
    for m in MODES:
        for n, im in frames[m]:
            share = bright_share(im)
            ref = REF / m / f"{n}.png"
            allowed = max(LIMIT, bright_share(Image.open(ref)) + 0.03) if ref.exists() else LIMIT
            if share >= allowed:
                out.append((m, n, round(share, 3)))
    assert not out, out


def test_every_frame_has_its_reference(frames):
    missing = [f"{m}/{n}" for m in MODES for n, _ in frames[m] if not (REF / m / f"{n}.png").exists()]
    assert not missing, missing


def test_colours_only_in_theme():
    """No colour literal outside src/ui/theme.h: rgb565() is called nowhere else."""
    out = []
    for p in list(SRC.rglob("*.cpp")) + list(SRC.rglob("*.h")):
        if p.name == "theme.h":
            continue
        for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            if re.search(r"\brgb565\(", line) and "constexpr uint16_t rgb565(" not in line:
                out.append(f"{p.relative_to(ROOT)}:{i}")
            if re.search(r"uint16_t\s+k?[A-Z_][A-Za-z_]*\s*=\s*0x[0-9A-Fa-f]{4}\b", line):
                out.append(f"{p.relative_to(ROOT)}:{i}")
    assert not out, out


def test_no_font_under_22px():
    assert min(size for _, _, size, _ in FONTS) >= 22

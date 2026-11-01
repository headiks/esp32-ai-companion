"""Builds or flashes the firmware. Xtensa GCC on Windows cannot read non-ASCII include paths from its response
files, so when the project path is not ASCII the build runs from an ASCII mirror (timestamps kept, builds stay
incremental). Usage: python tools/build_firmware.py [upload] [any extra `pio run` arguments]"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIRROR = Path(os.environ.get("COMPANION_MIRROR", "C:/companion_build/mirror" if os.name == "nt" else "/tmp/companion_mirror"))
PARTS = ["firmware", "core/libcompanion"]


def mirror():
    for part in PARTS:
        src, dst = ROOT / part, MIRROR / part
        # Stale files in the mirror would keep compiling after being deleted here.
        for old in [p for p in dst.rglob("*") if p.is_file() and ".pio" not in p.parts] if dst.exists() else []:
            if not (src / old.relative_to(dst)).exists():
                old.unlink()
        shutil.copytree(src, dst, dirs_exist_ok=True, ignore=shutil.ignore_patterns(".pio"))
    (MIRROR / "assets").mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "assets" / "sounds.pack", MIRROR / "assets" / "sounds.pack")
    return MIRROR / "firmware"


def main():
    args = sys.argv[1:]
    cwd = ROOT / "firmware" if str(ROOT).isascii() else mirror()
    cmd = ["pio", "run"]
    if args[:1] == ["upload"]:
        cmd += ["-t", "upload"]
        args = args[1:]
    sys.exit(subprocess.call(cmd + args, cwd=cwd))


if __name__ == "__main__":
    main()

"""Builds the shared C++ core as a shared library for the emulator, simulated device and tests.
Uses the system compiler when present, otherwise `pip install ziglang`. Usage: python tools/build_core.py [--tests]"""
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / "core" / "libcompanion"
OUT_DIR = ROOT / "core" / "bin"
NAMES = {"Windows": "companion_core.dll", "Darwin": "libcompanion_core.dylib"}
LIB = OUT_DIR / NAMES.get(platform.system(), "libcompanion_core.so")


def compiler():
    for cxx in ("c++", "g++", "clang++"):
        if shutil.which(cxx):
            return [cxx]
    try:
        import ziglang  # noqa: F401
    except ImportError:
        sys.exit("No C++ compiler found. Install one or run: pip install ziglang")
    return [sys.executable, "-m", "ziglang", "c++"]


def build():
    sources = sorted(CORE.glob("src/*.cpp")) + [ROOT / "core/capi/companion_capi.cpp"]
    OUT_DIR.mkdir(exist_ok=True)
    newest = max(p.stat().st_mtime for p in list(CORE.rglob("*.cpp")) + list(CORE.rglob("*.h")) + sources)
    if LIB.exists() and LIB.stat().st_mtime > newest:
        return LIB
    print("Сборка ядра, это может занять до минуты…", flush=True)
    # Compile from a temporary ASCII mirror: some toolchains mishandle non-ASCII project paths.
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        shutil.copytree(CORE / "src", tmp / "src")
        shutil.copytree(CORE / "include", tmp / "include")
        shutil.copy(ROOT / "core/capi/companion_capi.cpp", tmp / "capi.cpp")
        out = tmp / LIB.name
        srcs = [str(p) for p in sorted((tmp / "src").glob("*.cpp"))] + [str(tmp / "capi.cpp")]
        cmd = compiler() + ["-std=c++17", "-O2", "-shared", "-fPIC", "-I", str(tmp / "include"), "-include", "memory",
                            "-w", "-o", str(out)] + srcs
        if platform.system() == "Windows" and "ziglang" in cmd:
            cmd.remove("-fPIC")
        for _ in range(4):  # zig on Windows fails now and then on its cache; only a real source error is final
            res = subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, encoding="utf-8", errors="replace")
            if res.returncode == 0 or re.search(r"\.(cpp|h):\d+:\d+: error: (?!Unexpected)", res.stderr):
                break
        if res.returncode:
            sys.stderr.write(res.stderr[-6000:])
            sys.exit(f"core build failed ({res.returncode})")
        for old in OUT_DIR.glob("*.old"):  # left by earlier builds; still locked ones are skipped
            try:
                old.unlink()
            except OSError:
                pass
        if LIB.exists():
            # Windows refuses to overwrite a DLL loaded by a running emulator, but allows renaming it.
            LIB.replace(LIB.with_name(f"{LIB.name}.{os.getpid()}.old"))
        shutil.copy(out, LIB)
    return LIB


if __name__ == "__main__":
    print("built", build())

"""Whole-chain tests without hardware: simulated device over TCP, desktop UI, emulator UI, AI gateway."""
import json
import os
import subprocess
import sys
import threading
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")


def test_gateway_offline_rules():
    from tools import ai_gateway
    threading.Thread(target=ai_gateway.serve, args=(8799,), daemon=True).start()
    time.sleep(0.3)
    req = urllib.request.Request("http://127.0.0.1:8799/v1/chat", json.dumps({"text": "Привет"}).encode())
    with urllib.request.urlopen(req, timeout=5) as r:
        out = json.loads(r.read())
    assert out["emotion"] == "happy" and out["text"]


def _run(args, timeout=60):
    env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "PYTHONIOENCODING": "utf-8"}
    return subprocess.run([sys.executable] + args, cwd=ROOT, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)


def test_emulator_window_runs_demo(tmp_path):
    shot = tmp_path / "emu.png"
    r = _run(["emulator/main.py", "--storage", str(tmp_path), "--silent", "--demo", "--screenshot", str(shot), "--seconds", "4"])
    assert r.returncode == 0, r.stderr[-2000:]
    assert shot.stat().st_size > 5000


def test_desktop_talks_to_simulated_device(tmp_path):
    from tools import sim_device
    threading.Thread(target=sim_device.serve, args=(7792,), daemon=True).start()
    time.sleep(0.5)
    r = _run(["desktop/src/main.py", "--smoke", "socket://127.0.0.1:7792"])
    assert r.returncode == 0, r.stderr[-2000:]
    assert '"status": "success"' in r.stdout
    assert '"protocol": 1' in r.stdout
    assert '"fps"' in r.stdout

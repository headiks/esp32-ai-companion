"""ctypes wrapper around the shared C++ core: emulator, simulated device and tests all use this class."""
import ctypes as C
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools.build_core import build  # noqa: E402

SCREEN = 466
SAMPLE_RATE = 16000
PACK = ROOT / "assets" / "sounds.pack"
_lib = None


def lib():
    global _lib
    if _lib is None:
        _lib = C.CDLL(str(build()))
        p, f, i = C.c_void_p, C.c_float, C.c_int
        sigs = {
            "cc_create": (p, [C.c_char_p, C.c_char_p, C.c_uint32]), "cc_destroy": (None, [p]), "cc_tick": (None, [p, f]),
            "cc_render": (None, [p, p]), "cc_render_strips": (None, [p, p, i]), "cc_touch": (None, [p, i, i, i]),
            "cc_imu": (None, [p, f, f, f, f, f, f, f]), "cc_mic": (None, [p, p, i]), "cc_mix": (None, [p, p, i]),
            "cc_command": (i, [p, C.c_char_p, C.c_char_p, i]), "cc_command_unauth": (i, [p, C.c_char_p, C.c_char_p, i, C.POINTER(i)]),
            "cc_state": (i, [p, C.c_char_p, i]), "cc_set_battery": (None, [p, i, i, i, i]), "cc_set_time_scale": (None, [p, f]),
            "cc_set_fps": (None, [p, f]), "cc_emotion_count": (i, []), "cc_emotion_name": (C.c_char_p, [i]), "cc_allowed": (i, [i, i]),
            "cc_sound_count": (i, [p]), "cc_sound_name": (C.c_char_p, [p, i]), "cc_emotion_sound": (i, [p, i, i]), "cc_last_sound": (i, [p]),
            "cc_play_count": (C.c_uint32, [p]), "cc_blink_amount": (f, [p]), "cc_pose": (f, [p, i, i]), "cc_gaze": (f, [p, i]),
            "cc_maze_selftest": (i, [i, C.c_uint32]), "cc_maze_wall_selftest": (i, [C.c_uint32]), "cc_snake_selftest": (i, [C.c_uint32]), "cc_poll_input": (i, [p, C.c_char_p, i]), "cc_poll_mouse": (i, [p, C.POINTER(i)]), "cc_pin": (C.c_char_p, [p]), "cc_parse_weather": (i, [C.c_char_p, C.POINTER(f)]),
        }
        for name, (res, args) in sigs.items():
            fn = getattr(_lib, name)
            fn.restype, fn.argtypes = res, args
    return _lib


def emotion_names():
    l = lib()
    return [l.cc_emotion_name(i).decode() for i in range(l.cc_emotion_count())]


class Companion:
    def __init__(self, storage_dir=None, seed=1, pack=PACK):
        self.l = lib()
        self.h = self.l.cc_create(str(pack).encode("utf-8") if pack else b"", str(storage_dir).encode("utf-8") if storage_dir else b"", seed)
        self.fb = np.zeros((SCREEN, SCREEN), dtype=np.uint16)
        self._out = C.create_string_buffer(2048)

    def close(self):
        if self.h:
            self.l.cc_destroy(self.h)
            self.h = None

    def tick(self, dt_ms):
        self.l.cc_tick(self.h, dt_ms)

    def run(self, ms, step=20):
        for _ in range(int(ms / step)):
            self.tick(step)

    def render(self):
        self.l.cc_render(self.h, self.fb.ctypes.data)
        return self.fb

    def render_strips(self, strip_h):
        fb = np.zeros((SCREEN, SCREEN), dtype=np.uint16)
        self.l.cc_render_strips(self.h, fb.ctypes.data, strip_h)
        return fb

    def touch(self, down, x=0, y=0):
        self.l.cc_touch(self.h, 1 if down else 0, int(x), int(y))

    def imu(self, ax, ay, az, gx=0.0, gy=0.0, gz=0.0, dt=20.0):
        self.l.cc_imu(self.h, ax, ay, az, gx, gy, gz, dt)

    def mic(self, pcm):
        a = np.asarray(pcm, dtype=np.int16)
        self.l.cc_mic(self.h, a.ctypes.data, len(a))

    def mix(self, n):
        out = np.zeros(n, dtype=np.int16)
        self.l.cc_mix(self.h, out.ctypes.data, n)
        return out

    def handle_command(self, cmd):
        n = self.l.cc_command(self.h, json.dumps(cmd, ensure_ascii=False).encode(), self._out, len(self._out))
        return json.loads(self._out.raw[:n].decode("utf-8", "replace"))

    def command_unauth(self, cmd, authed=False):
        flag = C.c_int(1 if authed else 0)
        n = self.l.cc_command_unauth(self.h, json.dumps(cmd, ensure_ascii=False).encode(), self._out, len(self._out), C.byref(flag))
        return json.loads(self._out.raw[:n].decode("utf-8", "replace")), bool(flag.value)

    def poll_input(self):
        n = self.l.cc_poll_input(self.h, self._out, len(self._out))
        return json.loads(self._out.raw[:n].decode()) if n > 0 else None

    def poll_mouse(self):
        """(dx, dy, wheel, buttons) or None; buttons: 1 left, 2 right."""
        out = (C.c_int * 4)()
        return tuple(out) if self.l.cc_poll_mouse(self.h, out) else None

    def pin(self):
        return self.l.cc_pin(self.h).decode()

    def command_raw(self, line, authed):
        """A line from an untrusted transport, returns (response dict, authed). Malformed input gets an error response."""
        flag = C.c_int(1 if authed else 0)
        n = self.l.cc_command_unauth(self.h, line, self._out, len(self._out), C.byref(flag))
        return json.loads(self._out.raw[:n].decode("utf-8", "replace")), bool(flag.value)

    def state_event(self):
        n = self.l.cc_state(self.h, self._out, len(self._out))
        return json.loads(self._out.raw[:n].decode("utf-8", "replace"))

    def state(self):
        return self.handle_command({"type": "get_state"})

    def diag(self):
        return self.handle_command({"type": "diag"})

    def set_battery(self, percent, charging=False, present=True, mv=3900):
        self.l.cc_set_battery(self.h, percent, int(charging), int(present), mv)

    def set_time_scale(self, s):
        self.l.cc_set_time_scale(self.h, s)

    def set_fps(self, fps):
        self.l.cc_set_fps(self.h, fps)

    def sounds(self):
        return [self.l.cc_sound_name(self.h, i).decode() for i in range(self.l.cc_sound_count(self.h))]

    def emotion_sound(self, emotion_index, variant=0):
        return self.l.cc_emotion_sound(self.h, emotion_index, variant)

    def play_count(self):
        return self.l.cc_play_count(self.h)

    def last_sound(self):
        return self.l.cc_last_sound(self.h)

    def blink_amount(self):
        return self.l.cc_blink_amount(self.h)

    def pose(self, eye, field):
        return self.l.cc_pose(self.h, eye, field)

    def gaze(self):
        return self.l.cc_gaze(self.h, 0), self.l.cc_gaze(self.h, 1)


def rgb565_to_rgb888(fb):
    """Used by tools and tests; the emulator displays RGB565 directly."""
    r = ((fb >> 11) & 31) * 255 // 31
    g = ((fb >> 5) & 63) * 255 // 63
    b = (fb & 31) * 255 // 31
    return np.dstack([r, g, b]).astype(np.uint8)

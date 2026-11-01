"""Synthetic sensor streams. They go through the real GestureManager and MicrophoneManager of the core,
so the panel exercises the same detection code as the device instead of bypassing it."""
import numpy as np

SR = 16000


class ImuSimulator:
    def __init__(self):
        self.script = []      # queued (ax, ay, az, gyro) samples, one per tick
        self.face_down = False
        self.tilt_x = 0.0      # in g, +x rolls a ball to the right of the screen
        self.tilt_y = 0.0
        self.rng = np.random.default_rng(5)

    def shake(self):
        self.script += [(s * 2.6, 0.2, 1.0, 400.0) for s in [1, -1] * 2 for _ in range(1)] + [(0, 0, 1, 50.0)] * 2
        self.script += [(2.5, 0, 1.0, 400.0), (0, 0, 1, 120.0)] * 3

    def jerk(self):
        self.script += [(2.8, 0.5, 1.2, 300.0), (0, 0, 1, 40.0)]

    def flip(self):
        self.face_down = not self.face_down

    def sample(self):
        if self.script:
            ax, ay, az, g = self.script.pop(0)
            return ax, ay, (-az if self.face_down else az), g, 0.0, 0.0
        n = self.rng.normal(0, 0.004, 3)
        z = np.sqrt(max(0.0, 1.0 - self.tilt_x ** 2 - self.tilt_y ** 2))
        z = -z if self.face_down else z
        return self.tilt_x + float(n[0]), self.tilt_y + float(n[1]), z + float(n[2]), 0.1, 0.1, 0.1


class MicSimulator:
    """Produces 16 kHz PCM per tick: room noise, plus queued events."""
    def __init__(self):
        self.queue = np.zeros(0, dtype=np.float64)
        self.rng = np.random.default_rng(6)

    def loud(self):
        n = int(SR * 0.6)
        self.queue = np.concatenate([self.queue, 0.5 * np.sin(2 * np.pi * 400 * np.arange(n) / SR) + 0.15 * self.rng.standard_normal(n)])

    def clap(self):
        n = int(SR * 0.05)
        burst = 0.9 * self.rng.standard_normal(n) * np.exp(-np.linspace(0, 8, n))
        self.queue = np.concatenate([self.queue, burst, np.zeros(int(SR * 0.15))])

    def voice(self):
        # Syllable-like amplitude bursts around a voice fundamental with speech-like zero crossings.
        n = int(SR * 1.2)
        t = np.arange(n) / SR
        env = 0.06 * (0.6 + 0.4 * np.sin(2 * np.pi * 4 * t))
        self.queue = np.concatenate([self.queue, env * (np.sin(2 * np.pi * 180 * t) + 0.5 * np.sin(2 * np.pi * 720 * t))])

    def samples(self, n):
        out = self.rng.normal(0, 0.0015, n)
        take = min(n, len(self.queue))
        if take:
            out[:take] += self.queue[:take]
            self.queue = self.queue[take:]
        return np.clip(out * 32767, -32768, 32767).astype(np.int16)

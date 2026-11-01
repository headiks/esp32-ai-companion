"""Pulls mono PCM from the core mixer into the sound card. Runs on the audio callback thread,
just as the firmware mixer runs in its I2S task. Without a sound device the emulator stays silent."""
import numpy as np

try:
    import sounddevice as sd
except (ImportError, OSError):
    sd = None

SR = 16000


class AudioOutput:
    def __init__(self, companion, tap=None):
        self.companion = companion
        self.tap = tap        # optional callback for the level meter
        self.stream = None
        self.error = None

    def start(self):
        if sd is None:
            self.error = "sounddevice is not available"
            return False
        try:
            self.stream = sd.OutputStream(samplerate=SR, channels=1, dtype="int16", blocksize=256, callback=self._cb)
            self.stream.start()
            return True
        except Exception as e:  # sounddevice raises PortAudioError and OSError depending on the host
            self.error = str(e)
            self.stream = None
            return False

    def _cb(self, out, frames, _time, _status):
        pcm = self.companion.mix(frames)
        out[:, 0] = pcm
        if self.tap:
            self.tap(float(np.max(np.abs(pcm))) / 32768)

    def stop(self):
        if self.stream:
            self.stream.stop()
            self.stream.close()
            self.stream = None

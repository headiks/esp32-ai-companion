"""Synthesizes all sound assets of the character. No samples or recordings: everything is original.
Output: assets/sounds/<category>/<name>.wav (16 kHz mono) and assets/sounds.pack for firmware and emulator.
The voice is a small creature: soft sine-based tones with pitch glides and vibrato, kept above 300 Hz
because the built-in speaker does not reproduce lows, and short so repeats stay pleasant."""
import struct
import wave
from pathlib import Path

import numpy as np

SR = 16000
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "sounds"


def contour(points, n):
    t = np.linspace(0, 1, n)
    xs = [p[0] for p in points]
    ys = np.log([p[1] for p in points])  # glide in musical (log) pitch
    return np.exp(np.interp(t, xs, ys))


def voice(points, dur, gain=1.0, attack=0.012, release=0.06, vib=0.0, vib_hz=7.0, harm=0.18, fm=0.0, breath=0.0, seed=1):
    """One utterance: pitch contour, soft harmonics, optional vibrato, FM growl and breath."""
    n = max(int(dur * SR), 8)
    t = np.arange(n) / SR
    f = contour(points, n)
    if vib:
        f = f * (1 + vib * np.sin(2 * np.pi * vib_hz * t))
    ph = 2 * np.pi * np.cumsum(f) / SR
    mod = fm * np.sin(ph * 0.5) if fm else 0
    s = np.sin(ph + mod) + harm * np.sin(2 * ph + mod) + harm * 0.35 * np.sin(3 * ph)
    if breath:
        rng = np.random.default_rng(seed)
        s = s + breath * np.convolve(rng.standard_normal(n), np.ones(6) / 6, "same")
    env = np.ones(n)
    a, r = min(int(attack * SR), n // 2), min(int(release * SR), n // 2)
    if a:
        env[:a] = np.sin(np.linspace(0, np.pi / 2, a)) ** 2
    if r:
        env[-r:] *= np.cos(np.linspace(0, np.pi / 2, r)) ** 2
    return s * env * gain


def rest(d):
    return np.zeros(int(d * SR))


def seq(*parts):
    return np.concatenate(parts)


def note(hz, d, gain=1.0, **k):
    return voice([(0, hz), (1, hz)], d, gain, **k)


def glide(a, b, d, gain=1.0, **k):
    return voice([(0, a), (1, b)], d, gain, **k)


def noise_burst(d, cutoff=2000, gain=1.0, seed=3):
    n = int(d * SR)
    x = np.random.default_rng(seed).standard_normal(n)
    width = max(int(SR / cutoff), 1)
    x = np.convolve(x, np.ones(width) / width, "same")
    return x * np.exp(-np.linspace(0, 6, n)) * gain


def room(x, amount=0.16):
    """Tiny echo tail: gives the voice some body without a real reverb."""
    tail = np.zeros(len(x) + int(0.12 * SR))
    tail[:len(x)] = x
    for delay, g in ((0.031, 1.0), (0.057, 0.65), (0.093, 0.4)):
        d = int(delay * SR)
        tail[d:d + len(x)] += x * g * amount
    return tail


def st(hz, semis):
    return hz * 2 ** (semis / 12)


# Each emotion takes `v` (0 or 1): variants differ in melody, not only in pitch.
def e_neutral(v):
    return glide(620, 680, .09, .7) if v == 0 else glide(660, 600, .09, .7)


def e_happy(v):
    if v == 0:
        return seq(note(784, .08), note(988, .08), glide(1175, 1320, .15))
    return seq(note(880, .07), note(1109, .07), note(1319, .07), glide(1109, 1319, .13))


def e_very_happy(v):
    n = [784, 988, 1175, 1568] if v == 0 else [880, 1109, 1319, 1760]
    return seq(*[note(f, .065, vib=.01) for f in n], glide(n[3], n[3] * 1.12, .22, vib=.025, vib_hz=11))


def e_sad(v):
    if v == 0:
        return seq(glide(740, 520, .34, .9, vib=.012, vib_hz=5), rest(.03), glide(560, 392, .42, .8, vib=.015, vib_hz=5))
    return seq(glide(660, 466, .55, .9, vib=.018, vib_hz=4.5))


def e_angry(v):
    if v == 0:
        return seq(glide(330, 300, .1, 1, fm=2.2, harm=.4), rest(.025), glide(349, 262, .17, 1, fm=2.6, harm=.4))
    return seq(glide(392, 280, .2, 1, fm=3, harm=.45))


def e_annoyed(v):
    if v == 0:
        return seq(note(494, .07, .85, fm=.7), glide(466, 415, .16, .85, fm=.8))
    return seq(glide(523, 440, .1, .85, fm=.6), rest(.02), note(440, .08, .8, fm=.8))


def e_surprised(v):
    if v == 0:
        return seq(glide(520, 1760, .17, 1, attack=.004), note(1760, .06, .8))
    return seq(glide(660, 1480, .12, 1, attack=.004), rest(.015), glide(1319, 1976, .1))


def e_curious(v):
    if v == 0:
        return seq(note(698, .08), glide(698, 1047, .19))
    return seq(glide(622, 784, .09), rest(.02), glide(784, 1175, .16))


def e_thinking(v):
    if v == 0:
        return seq(note(587, .11, .8), rest(.05), note(659, .11, .8), rest(.05), note(587, .16, .75))
    return seq(glide(523, 587, .14, .8, vib=.01), rest(.04), glide(587, 554, .24, .75, vib=.012))


def e_sleepy(v):
    if v == 0:
        return glide(600, 330, .95, .85, attack=.12, release=.3, vib=.01, vib_hz=3, breath=.04)
    return seq(glide(523, 440, .4, .8, attack=.1, release=.15), glide(440, 349, .6, .7, release=.3, vib=.01, vib_hz=3))


def e_sleeping(v):
    n = [659, 523, 392] if v == 0 else [587, 494, 370]
    return seq(*[note(f, .34, .72 - i * .1, attack=.07, release=.18, harm=.08) for i, f in enumerate(n)], note(n[2] * .75, .55, .4, attack=.08, release=.36, harm=.05))


def e_waking_up(v):
    if v == 0:
        return seq(glide(392, 523, .25, .8, attack=.06), glide(523, 784, .22, .95), note(988, .13, .8))
    return seq(glide(440, 659, .3, .85, attack=.06), glide(659, 880, .2), glide(880, 1047, .1))


def e_scared(v):
    if v == 0:
        return seq(glide(1400, 2200, .06, 1, attack=.002), glide(2200, 1500, .1, .9, vib=.05, vib_hz=26), glide(1600, 1300, .12, .8, vib=.06, vib_hz=26))
    return glide(1200, 2100, .16, 1, attack=.002, vib=.06, vib_hz=24)


def e_affection(v):
    n = [659, 784, 988, 1175] if v == 0 else [698, 880, 1047, 1319]
    return seq(note(n[0], .13, .8, attack=.03, vib=.008), note(n[1], .13, .85, attack=.03, vib=.008),
               note(n[2], .15, .9, attack=.03, vib=.01), glide(n[3], n[2], .4, .8, attack=.03, release=.2, vib=.02, vib_hz=5.5))


def e_bored(v):
    if v == 0:
        return seq(glide(494, 392, .75, .75, attack=.08, release=.3, breath=.05, harm=.1))
    return seq(note(440, .25, .7, attack=.05), glide(440, 349, .55, .7, release=.3, breath=.05))


def e_attentive(v):
    return seq(note(1047, .055, .8), rest(.03), note(1319, .075, .8)) if v == 0 else seq(glide(988, 1175, .12, .8))


def e_confused(v):
    if v == 0:
        return seq(glide(659, 880, .11), glide(880, 554, .11), glide(554, 932, .17))
    return seq(glide(784, 587, .13), rest(.02), glide(587, 784, .1), glide(784, 698, .1))


def e_excited(v):
    n = [880, 1175, 880, 1319, 1568] if v == 0 else [988, 1319, 1109, 1480, 1760]
    return seq(*[note(f, .06, vib=.015, vib_hz=14) for f in n], rest(.02), glide(n[4], n[4] * 1.2, .13))


def e_mischievous(v):
    n = [523, 622, 523, 740] if v == 0 else [587, 698, 587, 831]
    return seq(note(n[0], .08, .8), note(n[1], .06, .7), rest(.03), note(n[2], .08, .8), glide(n[3], n[3] * 1.06, .16, .85, vib=.02, vib_hz=9))


def e_shy(v):
    return seq(glide(880, 988, .08, .5), rest(.05), glide(932, 831, .18, .45, release=.1)) if v == 0 else glide(784, 932, .2, .5, release=.12, vib=.01)


def e_proud(v):
    n = [523, 659, 784, 1047] if v == 0 else [587, 740, 880, 1175]
    return seq(note(n[0], .08), note(n[1], .08), note(n[2], .1), glide(n[3], n[3], .28, release=.14, vib=.012, vib_hz=6))


def e_dizzy(v):
    base = 700 if v == 0 else 620
    return voice([(0, base), (.2, base * 1.5), (.4, base * .8), (.6, base * 1.35), (.8, base * .75), (1, base * .6)], .75, .85, vib=.02, vib_hz=8)


def e_yawning(v):
    return voice([(0, 420), (.35, 660 if v == 0 else 620), (.7, 480), (1, 340)], 1.3, .75, attack=.15, release=.4, breath=.07, harm=.1)


def e_cold(v):
    base = 1100 if v == 0 else 980
    return seq(*[note(base * (1 + .03 * (i % 2)), .05, .7, attack=.004, vib=.03, vib_hz=30) for i in range(6)])


def e_hot(v):
    return glide(560, 400, .7, .7, attack=.1, release=.3, breath=.09, harm=.08) if v == 0 else seq(glide(520, 460, .3, .65, breath=.08), glide(460, 380, .4, .6, release=.25, breath=.08))


def e_dreamy(v):
    n = [659, 880, 784, 988] if v == 0 else [622, 830, 740, 932]
    return seq(*[note(f, .24, .55, attack=.07, release=.12, harm=.06, vib=.008, vib_hz=4) for f in n])


def e_focused(v):
    return seq(note(1175, .05, .6), rest(.06), note(1175, .05, .6)) if v == 0 else seq(note(1047, .06, .6), rest(.04), note(1109, .07, .6))


def e_winking(v):
    return seq(glide(880, 1319, .07), rest(.02), note(1760, .05, .8)) if v == 0 else seq(glide(988, 1480, .08), note(1319, .06, .8))


def e_smug(v):
    return seq(note(587, .1, .7), glide(587, 784, .12, .75), note(740, .14, .7, vib=.01)) if v == 0 else seq(glide(523, 698, .16, .7), note(659, .16, .7, vib=.012))


def e_skeptical(v):
    return seq(note(622, .12, .7), rest(.04), glide(622, 830, .2, .7)) if v == 0 else seq(glide(587, 554, .1, .7), rest(.03), glide(554, 740, .18, .7))


def e_guilty(v):
    return seq(note(523, .1, .55), rest(.03), glide(494, 392, .3, .5, vib=.012)) if v == 0 else glide(494, 349, .42, .55, vib=.015, vib_hz=4)


def e_embarrassed(v):
    return seq(*[glide(740 + 40 * i, 660 + 40 * i, .06, .5) for i in range(3)]) if v == 0 else seq(note(784, .05, .5), rest(.02), note(740, .05, .5), rest(.02), note(698, .1, .45))


def e_hopeful(v):
    return seq(note(523, .1, .7), note(659, .1, .7), glide(784, 932, .24, .75, vib=.012)) if v == 0 else seq(glide(494, 659, .16, .7), glide(740, 880, .24, .75, vib=.012))


def e_lonely(v):
    return seq(glide(622, 554, .35, .5, attack=.06, vib=.012, vib_hz=4), rest(.08), glide(523, 415, .45, .45, release=.2, vib=.015, vib_hz=4)) if v == 0 else glide(587, 392, .8, .5, attack=.08, release=.3, vib=.015, vib_hz=4)


def e_grateful(v):
    n = [659, 880, 784, 1047] if v == 0 else [698, 932, 830, 1109]
    return seq(*[note(f, .13, .75, attack=.02, vib=.008) for f in n[:3]], glide(n[3], n[3], .3, .8, release=.15, vib=.018, vib_hz=5))


def e_jealous(v):
    return seq(glide(466, 494, .16, .7, fm=.5), glide(494, 440, .18, .65, fm=.6)) if v == 0 else seq(note(415, .1, .7, fm=.5), glide(466, 415, .2, .65, fm=.6))


def e_determined(v):
    n = [440, 554, 659] if v == 0 else [494, 622, 740]
    return seq(note(n[0], .07, .85, attack=.004), rest(.02), note(n[1], .07, .85, attack=.004), rest(.02), note(n[2], .16, .9, attack=.004))


def e_terrified(v):
    base = 1500 if v == 0 else 1320
    return voice([(0, base), (.15, base * 1.6), (.5, base * 1.4), (1, base * .8)], .4, 1, attack=.002, vib=.08, vib_hz=30)


def e_disgusted(v):
    return glide(392, 262, .24, .8, fm=1.6, harm=.35) if v == 0 else seq(note(349, .08, .75, fm=1.4), glide(330, 247, .2, .75, fm=1.8))


def e_hungry(v):
    return seq(glide(440, 349, .18, .6, fm=.6, vib=.03, vib_hz=12), glide(370, 311, .22, .55, fm=.7, vib=.03, vib_hz=12)) if v == 0 else voice([(0, 392), (.4, 466), (1, 311)], .45, .6, fm=.7, vib=.03, vib_hz=11)


def e_starstruck(v):
    n = [1047, 1319, 1568, 2093] if v == 0 else [1175, 1480, 1760, 2349]
    return seq(*[note(f, .05, .75, attack=.003) for f in n], rest(.02), *[note(f, .05, .45, attack=.003) for f in n[::-1][:2]])


def e_laughing(v):
    base = 880 if v == 0 else 988
    return seq(*[seq(glide(base * (1 - i * .03), base * (.86 - i * .03), .07, .85, attack=.005), rest(.035)) for i in range(5)])


def e_crying(v):
    return seq(voice([(0, 700), (.2, 800), (1, 520)], .42, .75, vib=.05, vib_hz=7), rest(.04), voice([(0, 620), (.25, 720), (1, 440)], .5, .7, vib=.055, vib_hz=7)) if v == 0 else voice([(0, 660), (.15, 760), (.5, 620), (1, 415)], .85, .75, vib=.055, vib_hz=6.5)


def e_relieved(v):
    return glide(740, 494, .6, .6, attack=.05, release=.3, breath=.06, harm=.08) if v == 0 else seq(note(659, .14, .55, attack=.03), glide(659, 466, .45, .55, release=.25, breath=.05))


def e_sneaky(v):
    n = [494, 466, 494, 554] if v == 0 else [440, 415, 440, 494]
    return seq(*[seq(note(f, .05, .5, attack=.004), rest(.07)) for f in n])


def e_nostalgic(v):
    n = [587, 740, 659, 494] if v == 0 else [554, 698, 622, 466]
    return seq(*[note(f, .26, .5, attack=.08, release=.14, harm=.05, vib=.01, vib_hz=3.5) for f in n])


def e_playful(v):
    n = [659, 988, 784, 1175, 880] if v == 0 else [698, 1047, 830, 1245, 932]
    return seq(*[glide(f, f * 1.05, .07, .8, attack=.005) for f in n])


def e_amazed(v):
    return seq(glide(392, 1047, .3, .85, attack=.02), note(1047, .22, .8, vib=.02, vib_hz=7, release=.12)) if v == 0 else seq(glide(440, 880, .22, .85), glide(880, 1319, .14), note(1319, .18, .75, release=.1, vib=.02))


def e_exhausted(v):
    return glide(440, 262, 1.0, .55, attack=.1, release=.4, breath=.1, harm=.06) if v == 0 else seq(glide(415, 349, .4, .5, breath=.09), rest(.06), glide(349, 277, .5, .45, release=.25, breath=.1))


def e_suspicious(v):
    return seq(note(523, .13, .6), rest(.1), note(523, .13, .6), rest(.1), glide(523, 587, .2, .6)) if v == 0 else seq(note(494, .16, .6), rest(.12), glide(494, 554, .24, .6))


EMOTIONS = {
    "neutral": e_neutral, "happy": e_happy, "very_happy": e_very_happy, "sad": e_sad, "angry": e_angry, "annoyed": e_annoyed,
    "surprised": e_surprised, "curious": e_curious, "thinking": e_thinking, "sleepy": e_sleepy, "sleeping": e_sleeping,
    "waking_up": e_waking_up, "scared": e_scared, "affection": e_affection, "bored": e_bored, "attentive": e_attentive,
    "confused": e_confused, "excited": e_excited, "mischievous": e_mischievous, "shy": e_shy, "proud": e_proud,
    "dizzy": e_dizzy, "yawning": e_yawning, "cold": e_cold, "hot": e_hot, "dreamy": e_dreamy, "focused": e_focused, "winking": e_winking,
    "smug": e_smug, "skeptical": e_skeptical, "guilty": e_guilty, "embarrassed": e_embarrassed, "hopeful": e_hopeful, "lonely": e_lonely,
    "grateful": e_grateful, "jealous": e_jealous, "determined": e_determined, "terrified": e_terrified, "disgusted": e_disgusted, "hungry": e_hungry,
    "starstruck": e_starstruck, "laughing": e_laughing, "crying": e_crying, "relieved": e_relieved, "sneaky": e_sneaky, "nostalgic": e_nostalgic,
    "playful": e_playful, "amazed": e_amazed, "exhausted": e_exhausted, "suspicious": e_suspicious,
}

INTERACTIONS = {
    "tap": lambda: note(1400, .03, .6, attack=.002, release=.02),
    "double_tap": lambda: seq(note(1319, .03, .6, release=.015), rest(.02), note(1661, .04, .6, release=.02)),
    "hold": lambda: glide(600, 900, .16, .5, attack=.03),
    "swipe": lambda: glide(900, 1500, .07, .35, attack=.008, release=.04, harm=.03),
    "shake": lambda: seq(*[glide(700 + 90 * i, 500, .05, .6, attack=.003, vib=.05, vib_hz=30) for i in range(3)]),
    "clap": lambda: glide(1500, 2000, .06, .55, attack=.003),
    "feed": lambda: seq(note(523, .05, .7), rest(.02), note(659, .05, .7), rest(.02), note(587, .05, .7), rest(.03), glide(784, 988, .13, .8)),
    "pet": lambda: seq(glide(784, 932, .16, .55, attack=.04, vib=.015, vib_hz=6), glide(932, 880, .2, .5, release=.12)),
    "flip": lambda: glide(880, 440, .18, .55, attack=.01),
}

SYSTEM = {
    "boot": lambda: seq(note(392, .1, .7, attack=.03), note(523, .1, .75), note(659, .1, .8), glide(784, 1047, .3, .85, release=.15, vib=.01)),
    "notify": lambda: seq(note(1047, .08, .7), rest(.02), note(1397, .14, .7, release=.09)),
    "click": lambda: note(1760, .018, .45, attack=.001, release=.012, harm=.02),
    "error": lambda: seq(note(392, .1, .8, fm=.6), rest(.03), note(349, .16, .8, fm=.6)),
    "alarm": lambda: seq(*[seq(note(1319, .1, .85), rest(.05), note(1568, .1, .85), rest(.16)) for _ in range(3)]),
    "timer_done": lambda: seq(note(1047, .12, .8), rest(.04), note(1319, .12, .8), rest(.04), note(1568, .26, .85, release=.15)),
    "volume": lambda: note(1047, .07, .8, attack=.005),
}

GAMES = {
    "win": lambda: seq(note(659, .08), note(784, .08), note(988, .08), note(1319, .1), rest(.02), glide(1319, 1760, .25, release=.12)),
    "lose": lambda: seq(note(587, .14, .8), note(523, .14, .8), glide(466, 349, .4, .75, vib=.012, vib_hz=5)),
    "draw": lambda: seq(note(659, .11, .75), note(659, .11, .75), glide(659, 622, .2, .7)),
    "correct": lambda: seq(note(1047, .05, .75), note(1568, .1, .75)),
    "wrong": lambda: glide(330, 262, .16, .75, fm=.7),
    "go": lambda: seq(note(880, .08, .75), rest(.03), glide(1175, 1397, .12, .75)),
    "move": lambda: note(1175, .04, .55, attack=.003, release=.025),
    # Simon: a tonal chord, so the four sectors can be told apart by ear.
    "tone1": lambda: note(523, .22, .8, release=.08),
    "tone2": lambda: note(659, .22, .8, release=.08),
    "tone3": lambda: note(784, .22, .8, release=.08),
    "tone4": lambda: note(1047, .22, .8, release=.08),
    "bounce": lambda: glide(900, 1200, .04, .6, attack=.002, release=.02),
}


def condition(x):
    """Removes rumble the speaker cannot play, normalizes level, guards against clicks."""
    x = np.asarray(x, dtype=np.float64)
    # One-pole high-pass at 250 Hz.
    a = np.exp(-2 * np.pi * 250 / SR)
    y = np.zeros_like(x)
    prev_x = prev_y = 0.0
    for i, v in enumerate(x):
        prev_y = a * (prev_y + v - prev_x)
        prev_x = v
        y[i] = prev_y
    peak = np.max(np.abs(y)) or 1.0
    y = y / peak * 0.80
    fade = min(48, len(y) // 4)
    y[:fade] *= np.linspace(0, 1, fade)
    y[-fade:] *= np.linspace(1, 0, fade)
    return np.round(y * 32767).astype(np.int16)


def write_wav(path, pcm):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def main():
    table = {}
    for name, fn in EMOTIONS.items():
        table[f"emotions/{name}"] = room(fn(0))
        table[f"emotions/{name}_b"] = room(fn(1))
    for cat, group, amt in (("interactions", INTERACTIONS, .08), ("system", SYSTEM, .1), ("games", GAMES, .12)):
        for name, fn in group.items():
            table[f"{cat}/{name}"] = room(fn(), amt)

    entries, blob = [], bytearray()
    for key, x in table.items():
        pcm = condition(x)
        write_wav(OUT / f"{key}.wav", pcm)
        entries.append((key, len(blob), len(pcm)))
        blob += pcm.tobytes()
    header = 8 + 40 * len(entries)
    pack = bytearray(b"CSP1" + struct.pack("<I", len(entries)))
    for key, off, samples in entries:
        k = key.encode()
        assert len(k) < 32, key
        pack += k.ljust(32, b"\0") + struct.pack("<II", header + off, samples)
    pack += blob
    (ROOT / "assets" / "sounds.pack").write_bytes(pack)
    print(f"{len(entries)} sounds, pack {len(pack) / 1024:.0f} KB, longest {max(e[2] for e in entries) / SR:.2f} s")


if __name__ == "__main__":
    main()

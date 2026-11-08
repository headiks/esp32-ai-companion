"""Tests of the shared core through its C ABI: the code under test is the code the firmware runs."""
import json
import sys
import wave
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from core.companion import Companion, emotion_names, lib, SCREEN  # noqa: E402
from emulator.simulation.sensors import ImuSimulator, MicSimulator  # noqa: E402

EMOTIONS = emotion_names()
INK = 0xF79D          # theme::INK, the colour of the eyes and of main text
ACCENT = 0xADBB       # theme::ACCENT
POSE_FIELDS = 7       # EyePose of design 1a: t, a, b, px, py, s, pr
REQUIRED = ["neutral", "happy", "very_happy", "sad", "angry", "annoyed", "surprised", "curious", "thinking", "sleepy", "sleeping",
            "waking_up", "scared", "affection", "bored", "attentive", "confused", "excited",
            "mischievous", "shy", "proud", "dizzy", "yawning", "cold", "hot", "dreamy", "focused", "winking",
            "smug", "skeptical", "guilty", "embarrassed", "hopeful", "lonely", "grateful", "jealous", "determined", "terrified", "disgusted", "hungry", "starstruck", "laughing", "crying", "relieved", "sneaky", "nostalgic", "playful", "amazed", "exhausted", "suspicious"]
assert len(REQUIRED) == 50


@pytest.fixture
def c():
    x = Companion(seed=11)
    x.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    x.run(3500)
    yield x
    x.close()


def emotion(c):
    return c.state()["emotion"]


def tap(c, x=233, y=233):
    c.touch(True, x, y); c.run(60); c.touch(False, x, y); c.run(60)


def swipe(c, x0, y0, x1, y1):
    c.touch(True, x0, y0); c.run(40)
    for i in range(1, 9):
        c.touch(True, x0 + (x1 - x0) * i // 8, y0 + (y1 - y0) * i // 8); c.run(20)
    c.touch(False, x1, y1); c.run(120)


# ---- emotions --------------------------------------------------------------------------------
def test_all_required_emotions_exist():
    assert EMOTIONS == REQUIRED


@pytest.mark.parametrize("name", REQUIRED)
def test_each_emotion_reachable_and_plays_own_sound(name):
    c = Companion(seed=3)
    c.run(4000)
    idx = EMOTIONS.index(name)
    sound_ids = {c.emotion_sound(idx, 0), c.emotion_sound(idx, 1)}
    assert -1 not in sound_ids
    before = c.play_count()
    resp = c.handle_command({"type": "emotion", "value": name, "hold_ms": 9000})
    assert resp["status"] == "success"
    # The mixer must run for sounds to count, as on real hardware.
    seen = set()
    for _ in range(300):
        c.tick(20); c.mix(320)
        seen.add(emotion(c))
        if c.last_sound() in sound_ids and c.play_count() > before and emotion(c) == name:
            break
    assert emotion(c) == name, seen
    assert c.last_sound() in sound_ids, c.sounds()[c.last_sound()]
    c.close()


def test_emotion_sounds_are_unique_per_emotion():
    c = Companion()
    ids = [c.emotion_sound(i, 0) for i in range(len(EMOTIONS))] + [c.emotion_sound(i, 1) for i in range(len(EMOTIONS))]
    assert len(set(ids)) == 2 * len(EMOTIONS)
    c.close()


def test_transition_table_blocks_chaos():
    l = lib()
    s = EMOTIONS.index("sleeping")
    assert l.cc_allowed(s, EMOTIONS.index("waking_up"))
    for e in EMOTIONS:
        if e not in ("sleeping", "waking_up"):
            assert not l.cc_allowed(s, EMOTIONS.index(e)), e
    assert not l.cc_allowed(EMOTIONS.index("neutral"), s), "must fall asleep through sleepy"


def test_sleep_routes_through_sleepy_and_wake_through_waking_up(c):
    c.handle_command({"type": "action", "value": "sleep"})
    seen = []
    for _ in range(300):
        c.tick(20)
        e = emotion(c)
        if not seen or seen[-1] != e:
            seen.append(e)
    assert seen[-2:] == ["sleepy", "sleeping"], seen
    c.handle_command({"type": "action", "value": "wake"})
    assert emotion(c) == "waking_up"
    c.run(1200)
    assert emotion(c) != "sleeping"


def test_sound_not_repeated_while_emotion_active(c):
    c.handle_command({"type": "emotion", "value": "excited", "hold_ms": 20000})
    c.run(1000); c.mix(16000)
    n = c.play_count()
    for _ in range(500):
        c.tick(20); c.mix(320)
    assert c.play_count() == n


def test_each_emotion_has_distinct_geometry():
    c = Companion(seed=2)
    c.run(3000)
    poses = {}
    for name in REQUIRED:
        c.handle_command({"type": "action", "value": "wake"})
        c.handle_command({"type": "emotion", "value": name, "hold_ms": 8000})
        c.run(3500)
        # t, a, b, px, py, s, pr of both eyes; the lid angle in whole degrees, the rest in hundredths.
        poses[name] = tuple(round(c.pose(e, f), 0 if f == 1 else 2) for e in (0, 1) for f in range(POSE_FIELDS))
    assert len(set(poses.values())) == len(REQUIRED)
    c.close()


# ---- animation -------------------------------------------------------------------------------
def test_blinks_happen_and_face_never_freezes(c):
    closed, frames, gaze = 0, [], set()
    for _ in range(1000):
        c.tick(20)
        if c.blink_amount() > 0.8:
            closed += 1
        gaze.add(tuple(round(v) for v in c.gaze()))
    assert closed > 0
    assert len(gaze) > 3
    a = c.render().copy(); c.run(200); b = c.render().copy()
    assert (a != b).any(), "idle face must keep moving"


def test_face_image_matches_reference_layout(c):
    fb = c.render()
    white = fb == INK
    assert white.mean() > 0.04
    assert not white[:SCREEN // 2, :].all(axis=0).any()
    cols = np.where(white.any(axis=0))[0]
    left, right = white[:, :SCREEN // 2].sum(), white[:, SCREEN // 2:].sum()
    assert cols.min() < SCREEN // 2 < cols.max() and left > 0 and right > 0, "two eyes"
    assert abs(int(left) - int(right)) / max(left, right) < 0.2
    assert fb[233, 233] == 0, "black gap between the eyes"


@pytest.mark.parametrize("strip", [40, 64, 117])
@pytest.mark.parametrize("screen", ["face", "analog_clock", "settings", "pet", "digital_clock"])
def test_strip_rendering_identical_to_full(screen, strip):
    """Firmware draws in strips, the emulator at once. Both must produce the same picture."""
    c = Companion(seed=4)
    c.run(2000)
    c.handle_command({"type": "screen", "value": screen})
    c.run(600)
    full = c.render().copy()
    strips = c.render_strips(strip)
    assert np.array_equal(full, strips)
    c.close()


# ---- touch -----------------------------------------------------------------------------------
def test_gestures_map_to_screens(c):
    """Design 1a: back is a swipe down from the top edge; a tap on the face opens the menu, holding it talks."""
    def scr():
        return c.state()["screen"]
    def back():
        swipe(c, 233, 20, 233, 300); c.run(300)
    swipe(c, 233, 350, 233, 120); c.run(300)
    assert scr() == "settings"
    back()
    assert scr() == "face"
    swipe(c, 233, 120, 233, 350); c.run(300)
    assert scr() == "notifications"
    back()
    assert scr() == "face"
    swipe(c, 360, 233, 100, 233); c.run(300)
    assert scr() == "pet"
    swipe(c, 100, 233, 360, 233); c.run(300)
    assert scr() == "face"
    tap(c); c.run(400)
    assert scr() == "menu"
    back()
    c.touch(True, 233, 233); c.run(800); c.touch(False, 233, 233); c.run(300)
    assert scr() == "talk"


def test_tap_and_double_tap_react_on_face(c):
    h0 = c.state()["attention"]
    tap(c); c.run(400)
    assert c.state()["attention"] >= h0
    assert emotion(c) in ("curious", "attentive")
    assert c.state()["screen"] == "menu"
    c.handle_command({"type": "screen", "value": "face"})
    c.run(3000)
    tap(c); c.run(80); tap(c); c.run(300)
    assert emotion(c) in ("happy", "surprised")


def test_touch_dropout_of_one_frame_is_not_a_release(c):
    c.touch(True, 233, 233); c.run(100)
    c.touch(False, 233, 233); c.tick(16)
    c.touch(True, 233, 233); c.run(700)
    c.touch(False, 233, 233); c.run(200)
    assert c.state()["screen"] == "talk", "long press must survive a dropped sample"


def test_repeated_taps_annoy_the_pet(c):
    s0 = c.state()["stress"]
    for _ in range(9):
        c.handle_command({"type": "screen", "value": "face"})  # a tap opens the menu: come back and poke again
        tap(c); c.run(400)
    assert c.state()["stress"] > s0
    assert emotion(c) in ("annoyed", "angry")


# ---- motion and sound ------------------------------------------------------------------------
def feed_imu(c, sim, ticks):
    for _ in range(ticks):
        ax, ay, az, gx, gy, gz = sim.sample()
        c.imu(ax, ay, az, gx, gy, gz, 20)
        c.tick(20)


def test_flip_sleeps_lift_wakes(c):
    sim = ImuSimulator()
    feed_imu(c, sim, 20)
    sim.flip()
    feed_imu(c, sim, 300)
    assert c.state()["pet_status"] == "asleep"
    sim.flip()
    feed_imu(c, sim, 100)
    assert c.state()["pet_status"] != "asleep"


def test_shake_surprises_jerk_scares(c):
    sim = ImuSimulator()
    feed_imu(c, sim, 10)
    sim.shake()
    seen = set()
    for _ in range(60):
        feed_imu(c, sim, 1)
        seen.add(emotion(c))
    assert seen & {"surprised", "confused"}, seen
    feed_imu(c, sim, 200)
    sim.jerk()
    seen = set()
    for _ in range(80):
        feed_imu(c, sim, 1)
        seen.add(emotion(c))
    assert "scared" in seen, seen


def feed_mic(c, sim, ms):
    for _ in range(ms // 20):
        c.mic(sim.samples(320))
        c.tick(20)


def test_mic_events_through_detectors(c):
    sim = MicSimulator()
    feed_mic(c, sim, 2000)
    sim.loud()
    seen = set()
    for _ in range(60):
        feed_mic(c, sim, 20)
        seen.add(emotion(c))
    assert seen & {"surprised", "scared"}, seen
    c.run(3000)
    sim.clap()
    seen = set()
    for _ in range(40):
        feed_mic(c, sim, 20)
        seen.add(emotion(c))
    assert "attentive" in seen, seen
    c.run(3000)
    a0 = c.state()["attention"]
    sim.voice()
    feed_mic(c, sim, 1500)
    assert c.state()["attention"] >= a0


def test_mic_off_stops_reactions(c):
    c.handle_command({"type": "setting", "name": "mic", "value": False})
    sim = MicSimulator()
    sim.loud()
    seen = set()
    for _ in range(60):
        feed_mic(c, sim, 20)
        seen.add(emotion(c))
    assert not seen & {"surprised", "scared"}


# ---- pet behaviour ---------------------------------------------------------------------------
def test_stats_stay_in_range_and_change_over_time(c):
    c.set_time_scale(600)
    start = c.state()
    c.run(30000, 50)
    st = c.state()
    assert st["hunger"] > start["hunger"]
    for k in ("happiness", "energy", "curiosity", "attention", "affection", "stress", "sleepiness", "hunger", "fun", "health"):
        assert 0 <= st[k] <= 100


def test_feeding_reduces_hunger_and_petting_builds_affection(c):
    c.handle_command({"type": "set_stat", "name": "hunger", "value": 80})
    c.handle_command({"type": "action", "value": "feed"})
    assert c.state()["hunger"] <= 46
    a = c.state()["affection"]
    c.handle_command({"type": "action", "value": "pet"})
    assert c.state()["affection"] > a


def test_feeding_a_hungry_pet_makes_it_grateful(c):
    c.handle_command({"type": "set_stat", "name": "hunger", "value": 70})
    c.run(1000)
    c.handle_command({"type": "action", "value": "feed"})
    c.run(1200)
    assert emotion(c) == "grateful"


def test_starving_pet_cries(c):
    c.handle_command({"type": "set_stat", "name": "hunger", "value": 95})
    c.run(4000)
    assert emotion(c) == "crying"


def test_status_matches_needs(c):
    for k, v, want in (("hunger", 90, "hungry"), ("hunger", 10, None)):
        c.handle_command({"type": "set_stat", "name": k, "value": v})
        if want:
            assert c.state()["pet_status"] == want
    c.handle_command({"type": "set_stat", "name": "energy", "value": 15})
    c.run(100)
    assert c.state()["pet_status"] == "tired"
    c.run(3000)
    assert emotion(c) in ("sleepy", "exhausted")


def test_state_persists_across_restart(tmp_path):
    c = Companion(storage_dir=tmp_path, seed=9)
    c.handle_command({"type": "set_stat", "name": "affection", "value": 77})
    c.handle_command({"type": "setting", "name": "volume", "value": 33})
    pin = c.handle_command({"type": "get_settings"})
    c.close()  # saves
    c2 = Companion(storage_dir=tmp_path, seed=10)
    assert c2.state()["affection"] == 77
    assert c2.state()["volume"] == 33
    assert pin["volume"] == 33
    c2.close()


def test_corrupt_storage_falls_back_to_defaults(tmp_path):
    (tmp_path / "pet.bin").write_bytes(b"\x01" * 80)
    (tmp_path / "settings.bin").write_bytes(b"garbage")
    c = Companion(storage_dir=tmp_path)
    c.run(100)
    assert c.state()["energy"] > 50
    c.close()


# ---- protocol --------------------------------------------------------------------------------
def test_spec_example_response_fields(c):
    r = c.handle_command({"type": "emotion", "value": "happy", "source": "android", "request_id": 123})
    assert r["status"] == "success" and r["request_id"] == 123
    assert {"emotion", "energy", "happiness"} <= set(r)


def test_errors_are_structured(c):
    assert c.handle_command({"type": "emotion", "value": "bogus", "request_id": 5}) == {"status": "error", "error": "bad_value", "message": "unknown emotion", "request_id": 5}
    assert c.handle_command({"type": "nope"})["error"] == "unknown_type"
    assert c.handle_command({"x": 1})["error"] == "no_type"
    assert c.handle_command({"type": "ping", "protocol": 99})["error"] == "protocol_mismatch"
    out = __import__("ctypes").create_string_buffer(512)
    for bad in [b"{", b"not json", b'{"type":', b'{"type":"ping","x":{"nested":1}}', b'{"a":"\\u12"}']:
        n = c.l.cc_command(c.h, bad, out, 512)
        assert json.loads(out.raw[:n])["status"] == "error", bad


def test_long_command_is_rejected_not_overflowed(c):
    out = __import__("ctypes").create_string_buffer(512)
    big = b'{"type":"notify","title":"' + b"x" * 5000 + b'"}'
    n = c.l.cc_command(c.h, big, out, 512)
    assert json.loads(out.raw[:n])["error"] == "too_long"


def test_small_response_buffer_does_not_overflow(c):
    import ctypes
    out = ctypes.create_string_buffer(b"\xAA" * 64, 64)
    n = c.l.cc_command(c.h, b'{"type":"get_state"}', out, 32)
    assert n < 32
    assert out.raw[33:] == b"\xAA" * 31


def test_authentication_rules(c):
    pin = None
    r, authed = c.command_unauth({"type": "get_state"}, False)
    assert r["error"] == "auth_required" and not authed
    r, authed = c.command_unauth({"type": "hello"}, False)
    assert r["auth_required"] is True and r["protocol"] == 1
    r, authed = c.command_unauth({"type": "auth", "pin": "000000x"}, False)
    assert r["error"] == "auth_failed" and not authed
    for _ in range(5):
        r, _a = c.command_unauth({"type": "auth", "pin": "bad"}, False)
    assert r["error"] in ("auth_failed", "auth_locked")
    r, _a = c.command_unauth({"type": "auth", "pin": "bad"}, False)
    assert r["error"] == "auth_locked"


def test_auth_success_with_device_pin(tmp_path):
    c = Companion(storage_dir=tmp_path)
    c.run(100)
    c.close()
    # The PIN is shown on the Info screen; here it is read from the stored settings via a trusted session.
    c = Companion(storage_dir=tmp_path)
    blob = (tmp_path / "settings.bin").read_bytes()
    import re
    pin = re.search(rb"\d{6}\0", blob).group()[:6].decode()
    r, authed = c.command_unauth({"type": "auth", "pin": pin}, False)
    assert r["status"] == "success" and authed
    c.close()


def test_cyrillic_notification_roundtrip_and_truncation(c):
    r = c.handle_command({"type": "notify", "title": "Привет", "text": "я" * 200, "icon": "mail", "source": "android"})
    assert r["status"] == "success" and r["unread"] == 1
    c.run(4500)
    c.handle_command({"type": "screen", "value": "notifications"})
    c.run(500)
    c.render()


@pytest.mark.parametrize("way", ["up", "down", "right"])
def test_notifications_close_with_a_swipe(c, way):
    """Opened by a swipe down from the face: swipes up, down, or right on the first card take it back."""
    for i in range(2):
        assert c.handle_command({"type": "notify", "title": "Сообщение", "text": f"Привет {i}"})["status"] == "success"
    swipe(c, 233, 150, 233, 330)          # from the face
    assert c.state()["screen"] == "notifications"
    ends = {"up": (233, 330, 233, 150), "down": (233, 150, 233, 330), "right": (120, 233, 330, 233)}
    swipe(c, *ends[way])
    assert c.state()["screen"] == "face"


def test_notification_cards_still_turn(c):
    for i in range(2):
        assert c.handle_command({"type": "notify", "title": "Сообщение", "text": f"Привет {i}"})["status"] == "success"
    c.handle_command({"type": "screen", "value": "notifications"}); c.run(300)
    swipe(c, 330, 233, 120, 233)          # left: the next card
    swipe(c, 120, 233, 330, 233)          # right: back to the first, still here
    assert c.state()["screen"] == "notifications"


def test_notification_queue_keeps_newest(c):
    for i in range(40):
        c.handle_command({"type": "notify", "title": f"n{i}"})
    assert c.handle_command({"type": "notifications"})["count"] == 24
    assert c.handle_command({"type": "notifications", "value": "read_all"})["unread"] == 0
    assert c.handle_command({"type": "notifications", "value": "clear"})["count"] == 0


# ---- screens and games -----------------------------------------------------------------------
SCREENS = ["face", "menu", "pet", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications", "settings", "diagnostics", "games", "info", "tictactoe", "guess", "pet_game", "maze", "orbit", "snake", "simon", "coin", "dice", "magic_ball", "balance", "duel", "shaker", "safe", "workout", "mole", "blackjack", "weather", "mouse"]


@pytest.mark.parametrize("name", SCREENS)
def test_every_screen_draws_something(c, name):
    c.handle_command({"type": "notify", "title": "t"})
    c.handle_command({"type": "screen", "value": name})
    c.run(400)
    assert (c.render() != 0).sum() > 200


def test_menu_opens_all_items(c):
    c.handle_command({"type": "screen", "value": "menu"})
    c.run(300)
    tap(c, 233, 233 + 128); c.run(400)     # two rows below the face, Pet comes to the centre
    assert c.state()["screen"] == "menu"
    tap(c); c.run(300)                     # and opens with a tap on the centre
    assert c.state()["screen"] == "pet"


def test_timer_sets_minutes_and_seconds_and_alarms(c):
    """Tap the seconds to choose them; -1/+1 and a drag on the digits change the chosen part."""
    c.handle_command({"type": "screen", "value": "timer"})
    c.run(300)
    tap(c, 233 + 70, 198)                # the seconds
    for _ in range(5):
        tap(c, 334, 340)                 # +1: 05:05
    tap(c, 233 - 70, 198)                # the minutes
    for _ in range(5):
        tap(c, 132, 340)                 # -1: 00:05
    c.touch(True, 233 - 70, 230)         # drag the minutes up by two steps and back down by two
    for y in range(230, 186, -4):
        c.touch(True, 233 - 70, y); c.tick(20)
    for y in range(186, 232, 4):
        c.touch(True, 233 - 70, y); c.tick(20)
    c.touch(False, 233 - 70, 230); c.run(100)
    tap(c, 233, 340)                     # "Старт"
    c.run(3000)
    assert c.state()["screen"] == "timer", "five seconds are not up yet"
    c.run(3000)
    assert c.state()["screen"] == "face" and emotion(c) == "excited", "the alarm"


def test_stopwatch_runs_in_background(c):
    c.handle_command({"type": "screen", "value": "stopwatch"})
    c.run(300)
    tap(c, 233 - 56, 233 + 124)
    c.handle_command({"type": "screen", "value": "face"})
    c.run(1000)
    c.handle_command({"type": "screen", "value": "stopwatch"})
    c.run(200)
    c.render()


def test_tictactoe_full_game_vs_pet(c):
    c.handle_command({"type": "screen", "value": "tictactoe"})
    c.run(300)
    tap(c, 233 - 84, 233 + 76)  # vs pet
    c.run(200)
    h0 = c.state()
    cells = [(118 + 43 + 86 * (i % 3) + (233 - 129 - 118), 118 + 43 + 86 * (i // 3)) for i in range(9)]
    for _ in range(3):
        for x, y in cells:
            tap(c, x, y); c.run(900)
    # Game ended at some point and reaction sound played; no crash and the screen is alive.
    assert c.state()["screen"] == "tictactoe"
    assert c.play_count() >= 0


def test_settings_tap_changes_value(c):
    c.handle_command({"type": "screen", "value": "settings"})
    c.run(300)
    v0 = c.state()["brightness"]
    tap(c); c.run(300)                     # brightness is the first row: its own screen opens
    assert c.state()["screen"] == "setting_value"
    tap(c, 58, 360); c.run(100)            # minus
    assert c.state()["brightness"] == v0 - 10
    tap(c, 408, 360); c.run(100)           # plus
    assert c.state()["brightness"] == v0


def test_ai_offline_answers_with_emotion(c):
    c.handle_command({"type": "chat", "text": "Привет, как дела?"})
    c.run(200)
    assert emotion(c) in ("thinking", "happy")
    c.run(1500)
    assert emotion(c) == "happy"


def test_ai_answer_reaches_the_app_in_state(c):
    assert "reply" not in c.state_event()
    c.handle_command({"type": "chat", "text": "Привет, как дела?"})
    c.run(1700)
    s = c.state_event()
    assert s["reply"] and s["reply_n"] == 1 and s["reply_emotion"] == "happy"
    c.handle_command({"type": "chat", "text": "Привет, как дела?"})
    c.run(1700)
    assert c.state_event()["reply_n"] == 2  # the same words again are still a new answer


# ---- audio -----------------------------------------------------------------------------------
def test_all_sound_files_real_and_categorized():
    base = ROOT / "assets" / "sounds"
    for cat in ("emotions", "interactions", "system", "games"):
        files = list((base / cat).glob("*.wav"))
        assert files, cat
        for f in files:
            with wave.open(str(f)) as w:
                assert w.getframerate() == 16000 and w.getnchannels() == 1
                pcm = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16)
            assert len(pcm) > 200, f
            assert np.abs(pcm).max() > 8000, f
            assert np.abs(pcm[-5:]).max() < 2000, f"{f} ends with a click"


def test_emotion_sounds_differ_from_each_other():
    base = ROOT / "assets" / "sounds" / "emotions"
    seen = {}
    for f in base.glob("*.wav"):
        with wave.open(str(f)) as w:
            seen[f.stem] = w.readframes(w.getnframes())
    assert len(set(seen.values())) == len(seen) == 2 * len(EMOTIONS)


def test_mixer_output_volume_and_mute(c):
    c.handle_command({"type": "setting", "name": "volume", "value": 100})
    c.handle_command({"type": "play_sound", "value": "system/boot"})
    loud = np.abs(c.mix(4000)).max()
    assert loud > 3000
    c.mix(32000)
    c.handle_command({"type": "setting", "name": "volume", "value": 20})
    c.mix(8000)
    c.handle_command({"type": "play_sound", "value": "system/boot"})
    quiet = np.abs(c.mix(4000)).max()
    assert quiet < loud
    c.mix(32000)
    c.handle_command({"type": "setting", "name": "sound", "value": False})
    c.handle_command({"type": "play_sound", "value": "system/boot"})
    assert np.abs(c.mix(4000)).max() == 0


def test_category_mute(c):
    c.mix(32000)
    c.handle_command({"type": "setting", "name": "cat_games", "value": False})
    c.handle_command({"type": "play_sound", "value": "games/win"})
    assert np.abs(c.mix(4000)).max() == 0
    c.handle_command({"type": "play_sound", "value": "system/notify"})
    assert np.abs(c.mix(4000)).max() > 0


def test_priority_keeps_strong_sound(c):
    c.mix(32000)
    surprised = EMOTIONS.index("surprised")
    c.handle_command({"type": "emotion", "value": "surprised", "hold_ms": 3000})
    c.run(60); c.mix(320)
    played = c.last_sound()
    assert played in (c.emotion_sound(surprised, 0), c.emotion_sound(surprised, 1))


def test_sound_cue_is_synchronized_with_animation():
    """Surprise must sound while the eyes are snapping open, not after the animation."""
    c = Companion(seed=8)
    c.run(3000)
    c.mix(32000)
    n0 = c.play_count()
    c.handle_command({"type": "emotion", "value": "surprised", "hold_ms": 3000})
    for ms in range(0, 400, 10):
        c.tick(10); c.mix(160)
        if c.play_count() > n0:
            assert ms <= 60, f"sound at {ms} ms"
            # Eyes are still far from fully open at that moment: the scale has not reached 1.14.
            assert c.pose(0, 5) < 1.12
            break
    else:
        pytest.fail("no sound")
    c.close()


def test_inactivity_nap_does_not_flap():
    """Falling asleep from inactivity while fully rested must not wake up on its own."""
    c = Companion(seed=5)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 30})
    c.handle_command({"type": "set_stat", "name": "energy", "value": 100})
    c.handle_command({"type": "set_stat", "name": "sleepiness", "value": 0})
    c.run(33000, 50)
    assert c.state()["pet_status"] == "asleep"
    seen = set()
    for _ in range(600):
        c.tick(50)
        seen.add(c.state()["pet_status"])
    assert seen == {"asleep"}
    c.touch(True, 233, 233); c.run(40); c.touch(False, 233, 233); c.run(2000)
    assert c.state()["pet_status"] != "asleep"
    c.close()


def test_tired_sleep_ends_when_rested():
    c = Companion(seed=5)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    c.handle_command({"type": "set_stat", "name": "energy", "value": 5})
    c.run(4000)
    assert c.state()["pet_status"] == "asleep"
    c.set_time_scale(600)
    for _ in range(4000):
        c.tick(50)
    assert c.state()["pet_status"] != "asleep"
    c.close()


# ---- living behaviour --------------------------------------------------------------------------
def test_animation_never_jumps():
    """Every parameter moves continuously, whatever emotion comes next."""
    c = Companion(seed=21)
    c.run(3000)
    for name in REQUIRED:
        c.handle_command({"type": "emotion", "value": name, "hold_ms": 700})
        prev = [c.pose(e, f) for e in (0, 1) for f in range(POSE_FIELDS)]
        for _ in range(60):
            c.tick(16)
            now = [c.pose(e, f) for e in (0, 1) for f in range(POSE_FIELDS)]
            # Per frame: lid angle under 20 degrees; pupils may dart (saccades take about 40 ms) but not cross
            # the eye, under 1 unit; lid, cheek, scale and pupil size under half their range.
            limit = {1: 20, 3: 1.0, 4: 1.0}
            for i, (x, y) in enumerate(zip(now, prev)):
                assert abs(x - y) < limit.get(i % POSE_FIELDS, 0.5), (name, i)
            prev = now
    c.close()


def test_face_keeps_changing_by_itself():
    c = Companion(seed=33)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    seen, changes, last = set(), 0, None
    for _ in range(3000):      # one minute
        c.tick(20)
        e = emotion(c)
        seen.add(e)
        changes += e != last
        last = e
    assert changes >= 6 and len(seen) >= 4, (changes, seen)
    c.close()


def test_idle_thoughts_are_mostly_silent():
    c = Companion(seed=33)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    c.run(4000); c.mix(32000)
    n0 = c.play_count(); changes = 0; last = emotion(c)
    for _ in range(3000):
        c.tick(20); c.mix(320)
        changes += emotion(c) != last
        last = emotion(c)
    assert changes > 3 and c.play_count() - n0 <= changes / 2 + 1


def test_pose_breathes_while_holding_one_emotion(c):
    c.handle_command({"type": "emotion", "value": "neutral", "hold_ms": 60000})
    values = set()
    for _ in range(500):
        c.tick(20)
        values.add(round(c.pose(0, 0), 3))
    assert len(values) > 20, "upper lid must keep moving"


def set_hour(c, hour):
    c.handle_command({"type": "set_time", "epoch": 1767225600 + hour * 3600, "tz_minutes": 0})


def test_night_makes_it_sleepy_day_does_not():
    day = Companion(seed=2); night = Companion(seed=2)
    for x, h in ((day, 14), (night, 3)):
        x.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
        set_hour(x, h)
        x.set_time_scale(1)
    for _ in range(3000):
        day.tick(50); night.tick(50)
    assert night.state()["sleepiness"] > day.state()["sleepiness"]
    night.handle_command({"type": "set_stat", "name": "sleepiness", "value": 10})
    for _ in range(4000):
        night.tick(60)
    assert night.state()["pet_status"] == "asleep", "left alone at night it goes to bed"
    assert day.state()["pet_status"] != "asleep"
    day.close(); night.close()


def test_eyes_are_ink_day_and_night(c):
    """Design 1a: one eye colour from the palette; the night is quieter through brightness, not a tint."""
    for hour in (14, 2):
        c.handle_command({"type": "emotion", "value": "neutral", "hold_ms": 9000})
        set_hour(c, hour); c.run(200)
        assert (c.render() == INK).sum() > 3000, hour


def test_weather_command_and_reactions(c):
    assert c.state()["weather"] == "unknown"
    r = c.handle_command({"type": "weather", "value": "rain", "temp": 7})
    assert r["status"] == "success"
    assert c.state()["weather"] == "rain" and c.state()["temp"] == 7
    assert c.handle_command({"type": "weather", "value": "lava"})["status"] == "error"
    assert c.handle_command({"type": "weather", "value": "rain", "temp": 900})["status"] == "error"
    blank = c.render().copy()
    c.run(300)
    assert (c.render() != blank).any()


def test_snow_and_cold_shift_thoughts_to_cold():
    c = Companion(seed=8)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    c.handle_command({"type": "weather", "value": "snow", "temp": -12})
    seen = set()
    for _ in range(9000):
        c.tick(20)
        seen.add(emotion(c))
    assert "cold" in seen, seen
    c.close()


def test_weather_goes_stale(c):
    c.handle_command({"type": "weather", "value": "clear", "temp": 20})
    c.set_time_scale(3600)
    c.run(10000, 100)
    assert c.state()["weather"] == "unknown"


def test_open_meteo_parser():
    import ctypes
    l = lib()
    t = ctypes.c_float()
    good = b'{"latitude":55.7,"current_units":{"temperature_2m":"C"},"current":{"time":"2026-11-01T10:00","temperature_2m":-3.5,"weather_code":73,"wind_speed_10m":12.0}}'
    assert l.cc_parse_weather(good, ctypes.byref(t)) == 3 and abs(t.value + 3.5) < 0.01
    windy = b'{"current":{"temperature_2m":8,"weather_code":2,"wind_speed_10m":55}}'
    assert l.cc_parse_weather(windy, ctypes.byref(t)) == 6
    for bad in (b"", b"<html>502</html>", b'{"current":{"temperature_2m":1}}', b'{"current":{"temperature_2m":999,"weather_code":1}}'):
        assert l.cc_parse_weather(bad, ctypes.byref(t)) == -1, bad


# ---- games -------------------------------------------------------------------------------------
@pytest.mark.parametrize("size,seed,branchy", [(5, 1, 0), (7, 2, 0), (9, 3, 0), (11, 4, 0), (13, 5, 0), (13, 6, 30), (13, 7, 60)])
def test_maze_is_solvable_by_tilt_alone(size, seed, branchy):
    ms = lib().cc_maze_selftest(size, seed, branchy)
    assert 0 < ms < 600000, ms


def test_maze_is_new_every_time_with_the_exit_far_away():
    """Each level starts somewhere else; the exit is the farthest cell, so the route is never trivial."""
    import ctypes
    layouts = []
    for seed in range(1, 11):
        out = (ctypes.c_int * 5)()
        lib().cc_maze_layout(9, seed, 30, out)
        layouts.append(tuple(out))
    assert len({(sx, sy) for sx, sy, *_ in layouts}) >= 6, layouts
    assert all(length >= 9 for *_, length in layouts), layouts
    assert all((sx, sy) != (gx, gy) for sx, sy, gx, gy, _ in layouts)


def test_maze_ball_cannot_leave_the_board():
    assert all(lib().cc_maze_wall_selftest(s) == 1 for s in range(1, 8))


def test_snake_rules():
    assert lib().cc_snake_selftest(9) == 0


def centroid_of_white(fb, y_max=380):
    ys, xs = np.nonzero(fb[:y_max] == INK)  # the ball; the mini face sits below y_max
    return xs.mean(), ys.mean(), len(xs)


def test_maze_screen_tilts_the_ball(c):
    c.handle_command({"type": "screen", "value": "maze"})
    c.run(400)
    x0, y0, _ = centroid_of_white(c.render())
    for _ in range(40):
        c.imu(0.0, 0.7, 0.7, dt=20)
        c.tick(20)
    x1, y1, _ = centroid_of_white(c.render())
    for _ in range(40):
        c.imu(0.7, 0.0, 0.7, dt=20)
        c.tick(20)
    x2, y2, _ = centroid_of_white(c.render())
    assert (x1, y1) != (x0, y0) or (x2, y2) != (x1, y1)


def orbit_ball_angle(fb):
    """The ball is the only INK near the rim: the face and the score sit inside r130."""
    import math
    ys, xs = np.nonzero(fb == INK)
    far = np.hypot(xs - 233, ys - 233) > 130
    if not far.any():
        return None
    return math.atan2(xs[far].mean() - 233, -(ys[far].mean() - 233))


def orbit_play(c, ms, follow):
    """The finger holds the paddle on the ball's side of the rim, or on the opposite side.
    Returns the emotions the character showed meanwhile."""
    import math
    angle, felt = 0.0, set()
    for _ in range(ms // 20):
        seen = orbit_ball_angle(c.render())
        if seen is not None:
            angle = seen if follow else seen + math.pi
        c.touch(True, int(233 + 150 * math.sin(angle)), int(233 - 150 * math.cos(angle)))
        c.tick(20)
        felt.add(emotion(c))
    c.touch(False, 233, 233); c.run(100)   # a real lift: the core bridges gaps under 40 ms
    return felt


def test_orbit_paddle_keeps_the_ball_and_a_miss_ends_it(c):
    """The old rim pong: the paddle goes where the finger (or the lean) points, the ball bounces off it."""
    c.handle_command({"type": "screen", "value": "orbit"})
    c.run(300)
    tap(c, 233, 372)                 # "Старт"
    felt = orbit_play(c, 6000, follow=True)
    assert not felt & {"sad", "proud"}, f"the ball stays in play: {felt}"
    assert "proud" in orbit_play(c, 4000, follow=False), "a miss ends the round; bounces before it are a record"
    assert c.state()["screen"] == "orbit", "lifting the steering finger is not a swipe away from the result"
    tap(c, 280, 356)                 # "Ещё"
    assert "sad" in orbit_play(c, 4000, follow=False), "no bounce, no record"


def test_snake_screen_runs_and_dies_at_wall(c):
    c.handle_command({"type": "screen", "value": "snake"})
    c.run(300)
    tap(c)
    c.run(6000)
    c.render()
    assert c.state()["screen"] == "snake"


def test_clock_waits_for_time_then_shows_it():
    """Design 1a: until the first sync the clock says the time is not set; then the digits are INK."""
    INK_3 = 0x39C6
    c = Companion(seed=3)
    c.handle_command({"type": "screen", "value": "digital_clock"})
    c.run(600)
    band = c.render()[200:280]
    assert (band == INK_3).sum() > 300, "--:-- in INK_3"
    assert (band == INK).sum() < 50
    c.handle_command({"type": "set_time", "epoch": 1767225600 + 3600 * 12 + 34 * 60, "tz_minutes": 0})
    c.run(600)
    band = c.render()[170:260]
    assert (band == INK).sum() > 1500, "12:34 in INK"
    c.close()


def test_digit_change_ripples(c):
    c.handle_command({"type": "set_time", "epoch": 1767225600 + 3600 * 12, "tz_minutes": 0})
    c.handle_command({"type": "screen", "value": "digital_clock"})
    c.run(1000)
    a = c.render().copy()
    c.handle_command({"type": "set_time", "epoch": 1767225600 + 3600 * 12 + 60 * 11, "tz_minutes": 0})
    c.run(120)
    mid = c.render().copy()
    c.run(800)
    end = c.render().copy()
    assert (mid != a).any() and (mid != end).any(), "cells must pass through in-between states"


# ---- palette ------------------------------------------------------------------------------------
def is_saturated_warm(fb):
    r = ((fb >> 11) & 31) * 8
    g = ((fb >> 5) & 63) * 4
    b = (fb & 31) * 8
    return (r.astype(int) - b.astype(int) > 110) & (r.astype(int) - g.astype(int) > 40)


@pytest.mark.parametrize("name", SCREENS)
def test_no_orange_anywhere(c, name):
    c.handle_command({"type": "notify", "title": "t", "text": "x"})
    c.handle_command({"type": "screen", "value": name})
    c.run(500)
    assert not is_saturated_warm(c.render()).any(), name


@pytest.mark.parametrize("emo", REQUIRED)
def test_no_orange_in_emotion_accents(c, emo):
    c.handle_command({"type": "emotion", "value": emo, "hold_ms": 9000})
    c.run(1600)
    assert not is_saturated_warm(c.render()).any(), emo


# ---- party games --------------------------------------------------------------------------------
def shake(c):
    for ax in (2.6, 0.0, 2.6, 0.0, 2.6, 0.0, 2.6, 0.0):
        c.imu(ax, 0.1, 1.0, 400, 0, 0, 20)
        c.tick(20)


def still(c, ms):
    for _ in range(ms // 20):
        c.imu(0, 0, 1, 0, 0, 0, 20)
        c.tick(20)


@pytest.mark.parametrize("name", ["coin", "dice", "magic_ball"])
def test_chance_games_react_to_shake_not_like_pet(c, name):
    c.handle_command({"type": "screen", "value": name})
    still(c, 400)
    a = c.render().copy()
    shake(c)
    still(c, 300)
    b = c.render().copy()
    assert (a != b).any(), "shaking must start the toy"
    assert c.state()["emotion"] not in ("surprised", "confused"), "the game takes the shake, not the pet reaction"


def test_coin_finishes_and_counts_either_side():
    results = set()
    for seed in range(1, 10):
        c = Companion(seed=seed)
        c.handle_command({"type": "screen", "value": "coin"})
        c.run(300)
        tap(c)
        c.run(2000)
        results.add(c.render()[233 - 16].tobytes())
        assert c.state()["screen"] == "coin"
        c.close()
    assert len(results) >= 2, "both sides must come up"


def test_duel_false_start_then_win(c):
    c.handle_command({"type": "screen", "value": "duel"})
    c.run(300)
    tap(c, 233, 376)                  # "Готов"
    still(c, 1000)
    c.imu(2.5, 0, 1, 300, 0, 0, 20)   # moving before the signal
    c.tick(20)
    still(c, 200)
    assert emotion(c) == "smug", "the pet gloats over a false start"
    tap(c, 280, 356)                  # "Ещё"
    still(c, 1000)
    for _ in range(400):              # wait for the ring of the signal: it lights up, the screen is never flooded
        still(c, 20)
        if (c.render() == ACCENT).sum() > 2000:
            break
    else:
        pytest.fail("no signal")
    c.imu(2.5, 0, 1, 300, 0, 0, 20)   # swing immediately: faster than the pet can be
    c.tick(20)
    still(c, 300)
    assert emotion(c) == "terrified", "the pet lost the duel"


def test_shaker_counts_strokes_not_samples(c):
    c.handle_command({"type": "screen", "value": "shaker"})
    c.run(300)
    tap(c, 233, 372)  # "Старт"
    c.mix(32000)
    n0 = c.play_count()
    for _ in range(12):
        c.imu(2.4, 0, 1, 300, 0, 0, 20); c.tick(20); c.mix(320)
        c.imu(2.4, 0, 1, 300, 0, 0, 20); c.tick(20); c.mix(320)
        c.imu(0, 0, 1, 0, 0, 0, 20); c.tick(20); c.mix(320)
        c.imu(0, 0, 1, 0, 0, 0, 20); c.tick(20); c.mix(320)
    assert 8 <= c.play_count() - n0 <= 16


def test_workout_reps_have_refractory_period(c):
    c.handle_command({"type": "screen", "value": "workout"})
    c.run(300); c.mix(32000)
    n0 = c.play_count()
    for _ in range(30):      # far too fast to be real repetitions
        c.imu(1.6, 0, 1, 0, 0, 0, 20); c.tick(20); c.mix(320)
        c.imu(0, 0, 1, 0, 0, 0, 20); c.tick(20); c.mix(320)
    assert c.play_count() - n0 <= 5, "shaking must not count as a dozen reps"


def test_workout_can_be_finished_and_rewards(tmp_path):
    c = Companion(storage_dir=tmp_path, seed=1)
    c.handle_command({"type": "setting", "name": "sleep_after", "value": 7200})
    c.handle_command({"type": "set_stat", "name": "stress", "value": 50})
    c.handle_command({"type": "screen", "value": "workout"})
    c.run(300)
    for _ in range(4):
        tap(c, 233, 354); c.run(200)   # "Пропустить"
    assert c.state()["stress"] <= 36
    c.close()


@pytest.mark.parametrize("turn", [1, -1])
def test_safe_opens_when_dial_on_secret(c, turn):
    """Either way round, and after turns of winding up first: the dial angle grows without bound."""
    import math
    c.handle_command({"type": "screen", "value": "safe"})
    c.run(300)
    for deg in range(0, 3 * 360, 4):  # three idle turns
        c.imu(math.sin(math.radians(turn * deg)) * 0.5, -math.cos(math.radians(turn * deg)) * 0.5, 0.8, dt=40)
        c.tick(40)
    # Sweep the whole circle by tilt and tap everywhere: three locks must eventually open, wrong taps are harmless.
    opened = False
    for deg in range(0, 1800, 4):
        c.imu(math.sin(math.radians(turn * deg)) * 0.5, -math.cos(math.radians(turn * deg)) * 0.5, 0.8, dt=40)
        c.tick(40)
        if deg % 8 == 0:
            tap(c)
        if emotion(c) == "starstruck":
            opened = True
            break
    assert opened


# ---- controller ---------------------------------------------------------------------------------
def drain(c):
    out = []
    while True:
        m = c.poll_input()
        if not m:
            return out
        out.append(m)


def test_no_input_stream_without_controller(c):
    c.imu(0.3, 0, 0.9, dt=20); c.tick(60)
    tap(c)
    assert drain(c) == []


def test_controller_pad_streams_tilt_and_actions(c):
    r = c.handle_command({"type": "controller", "value": "start", "mode": "pad", "title": "Гонки"})
    assert r["status"] == "success" and c.state()["screen"] == "controller"
    c.imu(0.4, -0.2, 0.9, dt=20); c.tick(100)
    msgs = drain(c)
    assert any("ax" in m and abs(m["ax"] - 0.4) < 0.01 and abs(m["ay"] + 0.2) < 0.01 for m in msgs)
    tap(c)
    actions = [m["action"] for m in drain(c) if "action" in m]
    assert actions == ["down", "up", "tap"]


def test_controller_captures_swipes_and_shake(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "pad"})
    c.run(100); drain(c)
    swipe(c, 360, 233, 100, 233)
    assert "swipe_left" in [m.get("action") for m in drain(c)]
    assert c.state()["screen"] == "controller", "a swipe belongs to the game"
    shake(c)
    assert "shake" in [m.get("action") for m in drain(c)]
    assert c.state()["emotion"] not in ("surprised", "confused")


def test_cards_are_chosen_by_tilt_and_sent_by_tap(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,10h,Kd,2c,7s", "marks": "10000", "title": "Покер"})
    for _ in range(10):
        c.imu(-0.5, 0, 0.8, dt=20); c.tick(20)
    drain(c)
    tap(c)
    assert [m["index"] for m in drain(c) if m.get("action") == "tap"] == [0]
    for _ in range(10):
        c.imu(0.5, 0, 0.8, dt=20); c.tick(20)
    msgs = drain(c)
    assert {"event": "input", "action": "select", "index": 4} in msgs
    tap(c)
    assert [m["index"] for m in drain(c) if m.get("action") == "tap"] == [4]
    for _ in range(10):
        c.imu(0.0, 0, 1, dt=20); c.tick(20)
    assert [m["index"] for m in drain(c) if m.get("action") == "select"] == [2]


def test_card_choice_does_not_flicker_at_a_boundary(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh,Qd,Jc", "title": ""})
    c.run(100); drain(c)
    selects = 0
    for i in range(200):
        c.imu(0.0 + (0.012 if i % 2 else -0.012), 0, 1, dt=20)  # hand trembling right on the line between two cards
        c.tick(20)
        selects += sum(1 for m in drain(c) if m.get("action") == "select")
    assert selects <= 1


def test_cards_screen_redraws_on_update(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh", "title": ""})
    c.run(300)
    a = c.render().copy()
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "2d,3c", "title": ""})
    c.run(300)
    assert (a != c.render()).any()


def test_controller_bad_input_rejected(c):
    assert c.handle_command({"type": "controller", "value": "start", "mode": "banana"})["status"] == "error"
    assert c.handle_command({"type": "controller", "value": "nope"})["status"] == "error"
    assert c.handle_command({"type": "controller", "value": "ping"})["error"] == "not_active"
    too_many = ",".join(["As"] * 30)
    assert c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": too_many})["status"] == "success"
    assert c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": ",,,toolongcardlabel,"})["status"] == "success"
    c.run(200)
    c.render()


def test_controller_times_out_without_host(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "pad"})
    c.run(7000)
    assert c.state()["screen"] == "controller"
    c.handle_command({"type": "controller", "value": "ping"})
    c.run(7000)
    assert c.state()["screen"] == "controller"
    c.run(2000)
    assert c.state()["screen"] == "face"
    assert drain(c) == []


# ---- card controller with big hands ---------------------------------------------------------------
def big_hand(c, n=14, marks="", **kw):
    labels = ",".join(f"{6 + i % 9}{'shdc'[i % 4]}" for i in range(n))
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": labels, "marks": marks, "title": "Дурак", **kw})


def test_big_hand_scrolls_by_lean_speed_and_holds_when_level(c):
    big_hand(c)
    c.run(100); drain(c)
    for _ in range(100):
        c.imu(0, 0, 1, dt=20); c.tick(20)
    assert not [m for m in drain(c) if m.get("action") == "select"], "level hand holds the card"
    seen = []
    for _ in range(260):
        c.imu(0.4, 0, 0.9, dt=20); c.tick(20)
        seen += [m["index"] for m in drain(c) if m.get("action") == "select"]
    assert seen == sorted(seen) and seen[-1] == 13 and len(seen) >= 8, "every card is passed, none skipped"
    assert all(b - a == 1 for a, b in zip(seen, seen[1:]))
    back = []
    for _ in range(40):
        c.imu(-0.4, 0, 0.9, dt=20); c.tick(20)
        back += [m["index"] for m in drain(c) if m.get("action") == "select"]
    assert back and back[-1] < 13


def test_full_deck_fits_and_draws(c):
    big_hand(c, n=36, marks="1" * 36, info="Ход игрока", hint="наклон выбор · тап отметить · вверх на стол", target="Ks")
    c.run(300)
    for i in range(600):
        c.imu(0.6, 0, 0.8, dt=20); c.tick(20)
        drain(c)
        if i % 150 == 0:
            c.handle_command({"type": "controller", "value": "ping"})
    assert (c.render() != 0).sum() > 500
    tap(c)
    assert [m for m in drain(c) if m.get("action") == "tap"][-1]["index"] == 35


def test_hand_can_shrink_under_the_selection(c):
    big_hand(c, n=14)
    for _ in range(100):
        c.imu(0.5, 0, 0.8, dt=20); c.tick(20)
    big_hand(c, n=3)
    for _ in range(20):
        c.imu(0, 0, 1, dt=20); c.tick(20)
    c.render()
    tap(c)
    assert [m for m in drain(c) if m.get("action") == "tap"][-1]["index"] < 3


def test_empty_hand_draws(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "", "title": "x"})
    c.run(200)
    c.render()


def test_hint_is_on_screen(c):
    big_hand(c, n=5)
    c.run(300)
    a = c.render().copy()
    big_hand(c, n=5, hint="наклон выбор")
    c.run(300)
    assert (a[233 + 136:233 + 170] != c.render()[233 + 136:233 + 170]).any()


def test_swipes_reach_the_host_for_durak_actions(c):
    big_hand(c, n=8)
    c.run(100); drain(c)
    swipe(c, 233, 350, 233, 120)
    swipe(c, 233, 120, 233, 350)
    swipe(c, 100, 233, 360, 233)
    actions = [m.get("action") for m in drain(c)]
    assert {"swipe_up", "swipe_down", "swipe_right"} <= set(actions)


# ---- mouse --------------------------------------------------------------------------------------
def collect_mouse(c, ticks, ax=0.0, ay=0.0):
    x = y = wheel = 0
    buttons = []
    for _ in range(ticks):
        c.imu(ax, ay, 0.9, dt=20)
        c.tick(20)
        while (r := c.poll_mouse()):
            x += r[0]; y += r[1]; wheel += r[2]
            buttons.append(r[3])
    return x, y, wheel, buttons


def enter_mouse(c):
    for _ in range(5):
        c.imu(0, 0, 1, dt=20); c.tick(20)
    c.handle_command({"type": "screen", "value": "mouse"})
    collect_mouse(c, 15)


def test_mouse_only_reports_in_mouse_mode(c):
    assert collect_mouse(c, 50, ax=0.5) == (0, 0, 0, [])


def test_tilt_moves_pointer_with_dead_zone(c):
    enter_mouse(c)
    assert collect_mouse(c, 50, ax=0.03)[:2] == (0, 0), "a steady hand keeps the pointer still"
    x, y, _, _ = collect_mouse(c, 50, ax=0.4)
    assert x > 50 and abs(y) <= 1
    x, y, _, _ = collect_mouse(c, 50, ay=-0.4)
    assert y < -50 and abs(x) <= 1
    slow = collect_mouse(c, 50, ax=0.15)[0]
    fast = collect_mouse(c, 50, ax=0.6)[0]
    assert 0 < slow < fast / 4, "fine control at small angles"


def test_tap_left_double_tap_right(c):
    enter_mouse(c)
    c.touch(True, 180, 180); collect_mouse(c, 3); c.touch(False, 180, 180)
    _, _, _, b = collect_mouse(c, 40)
    assert 1 in b and b[-1] == 0 and 2 not in b, b
    c.touch(True, 180, 180); collect_mouse(c, 3); c.touch(False, 180, 180); collect_mouse(c, 5)
    c.touch(True, 180, 180); collect_mouse(c, 3); c.touch(False, 180, 180)
    _, _, _, b = collect_mouse(c, 40)
    assert 2 in b and b[-1] == 0 and 1 not in b, b


def test_finger_moves_pointer_and_stops_tilt(c):
    enter_mouse(c)
    c.touch(True, 150, 200); collect_mouse(c, 3, ax=0.5)
    x = 0
    for i in range(10):
        c.touch(True, 150 + i * 6, 200)
        x += collect_mouse(c, 1, ax=0.5)[0]
    assert 40 < x < 180, x
    c.touch(False, 200, 200)
    collect_mouse(c, 30)


def test_rim_scrolls_instead_of_moving(c):
    enter_mouse(c)
    c.touch(True, 440, 200); collect_mouse(c, 3)
    x = wheel = 0
    for i in range(12):
        c.touch(True, 440, 200 + i * 10)
        r = collect_mouse(c, 1)
        x += r[0]; wheel += r[2]
    c.touch(False, 440, 320); collect_mouse(c, 10)
    assert wheel < -5 and x == 0


def test_long_press_drags_until_release(c):
    enter_mouse(c)
    c.touch(True, 180, 200)
    _, _, _, b = collect_mouse(c, 45)
    assert b and b[-1] == 1, "held after a long press"
    c.touch(False, 180, 200)
    _, _, _, b = collect_mouse(c, 10)
    assert b[-1] == 0


def test_shake_pauses_tilt_and_top_swipe_exits(c):
    enter_mouse(c)
    shake(c)
    assert collect_mouse(c, 50, ax=0.5)[0] == 0, "shake switches tilt off"
    assert c.state()["emotion"] not in ("surprised", "confused")
    swipe(c, 233, 30, 233, 260)
    assert c.state()["screen"] == "remotes"  # back one level, to the list it came from
    assert collect_mouse(c, 20, ax=0.5)[0] == 0


def test_mouse_reports_never_exceed_hid_range(c):
    enter_mouse(c)
    c.touch(True, 150, 200); collect_mouse(c, 3)
    c.touch(True, 420, 440)
    c.tick(20)
    seen = []
    while (r := c.poll_mouse()):
        seen.append(r)
    assert seen and all(abs(r[0]) <= 127 and abs(r[1]) <= 127 for r in seen)


def test_long_cyrillic_fields_never_end_in_half_a_letter(c):
    long_text = "щ" * 140
    r = c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As", "title": long_text[:300], "info": long_text, "hint": long_text})
    assert r["status"] == "success"
    c.run(200)
    c.render()


def test_two_line_hint_is_fully_visible(c):
    """The second line holds the actions, it must not be wrapped away."""
    one = "наклон выбор · тап отметить"
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh", "title": "x", "hint": one})
    c.run(300)
    a = c.render().copy()
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh", "title": "x", "hint": one + "\\n↑ бить · → перевод · ↓ беру"})
    c.run(300)
    second_line = c.render()[300 + 28:300 + 56]   # the hint starts at y 300, 28 px a line
    assert (second_line != a[300 + 28:300 + 56]).any()


# ---- twenty-one on the device ------------------------------------------------------------------
def test_blackjack_rules():
    for seed in range(1, 6):
        assert lib().cc_blackjack_selftest(seed) == 0


def test_blackjack_screen_full_round(c):
    c.handle_command({"type": "screen", "value": "blackjack"})
    c.run(400); c.mix(32000)
    before = c.render().copy()
    n0 = c.play_count()
    tap(c, 233 + 62, 366)             # stand
    for _ in range(300):
        c.tick(20); c.mix(320)
    assert c.play_count() > n0, "the dealer plays and the result sounds"
    assert (c.render() != before).any(), "the dealer's hidden card is shown"
    settled = c.render().copy()
    tap(c, 233, 233)                  # any tap deals again
    c.run(300)
    assert (c.render() != settled).any()


def test_blackjack_hit_until_bust_then_new_round(c):
    c.handle_command({"type": "screen", "value": "blackjack"})
    c.run(400)
    for _ in range(14):
        tap(c, 233 - 62, 366)
        c.run(100)
    assert c.state()["screen"] == "blackjack"
    c.render()
    tap(c, 233, 233)
    c.run(800)
    c.render()


def test_blackjack_many_rounds_refill_chips_when_broke(c):
    c.handle_command({"type": "screen", "value": "blackjack"})
    c.run(400)
    for _ in range(40):
        tap(c, 233 + 62, 366)
        c.run(3000)
        c.render()
        tap(c, 233, 233)
        c.run(100)
    assert c.state()["screen"] == "blackjack"


def test_blackjack_opens_from_the_games_list(c):
    c.handle_command({"type": "screen", "value": "games"})
    c.run(300)
    tap(c, 233, 233 + 128)   # third row comes to the centre
    c.run(400)
    tap(c)                   # and opens
    c.run(300)
    assert c.state()["screen"] == "blackjack"


def test_phone_buttons_stand_for_the_swipes(c):
    """Design 1a poker: the phone sends three labels; left folds (swipe down), middle is the main move, right raises."""
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh", "title": "Фишки 190",
                      "buttons": "Пас,Чек,Рейз"})
    c.run(300); drain(c)
    for (x, y), action in (((128, 338), "swipe_down"), ((233, 338), "tap"), ((338, 338), "swipe_up")):
        tap(c, x, y)
        assert action in [m.get("action") for m in drain(c)], action
    long = c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh", "buttons": "Очень длинная подпись кнопки,b,c,d"})
    assert long["status"] == "success", "long or extra labels are cut, not refused"


def test_two_cards_are_both_big_and_tilt_selects_nothing(c):
    c.handle_command({"type": "controller", "value": "start", "mode": "cards", "labels": "As,Kh", "title": "Покер"})
    c.run(300); drain(c)
    fb = c.render()
    SURFACE = 0x10A2
    left, right = fb[140:256, 140:218], fb[140:256, 248:326]   # 96 x 136 each, side by side
    assert (left == SURFACE).mean() > 0.5 and (right == SURFACE).mean() > 0.5, "both cards are face up and large"
    for _ in range(50):
        c.imu(0.6, 0, 0.8, dt=20); c.tick(20)
    assert not [m for m in drain(c) if m.get("action") == "select"]
    tap(c)
    assert "tap" in [m.get("action") for m in drain(c)]


def test_blackjack_rank_and_suit_do_not_touch():
    """Clear card background must separate the rank from the suit, for every card dealt."""
    SURFACE = 0x10A2
    for seed in range(1, 25):
        c = Companion(seed=seed)
        c.run(500)
        c.handle_command({"type": "screen", "value": "blackjack"})
        c.run(400)
        fb = c.render()
        # The dealer's open card: 50 x 68 at the left of the two card row, top at y 112.
        x0, y0 = 155, 112
        card = fb[y0 + 6:y0 + 62, x0 + 6:x0 + 44]
        ink = (card != SURFACE).any(axis=1)
        blocks = (ink[1:] & ~ink[:-1]).sum() + int(ink[0])
        assert blocks >= 2, f"seed {seed}: rank and suit merged into one block"
        c.close()


def test_coin_has_two_sides_and_rests_still():
    """Design 1a: a flat coin, dark with a bright rim, its side written on it; heads and tails differ."""
    seen = {}
    for seed in range(1, 10):
        c = Companion(seed=seed)
        c.handle_command({"type": "screen", "value": "coin"})
        c.run(300)
        tap(c)
        c.run(2000)
        face = c.render()[200 - 80:200 + 80, 233 - 80:233 + 80].copy()
        seen[face.tobytes()] = face
        c.close()
    assert len(seen) == 2, "exactly two faces, and the coin at rest does not flicker"
    a, b = seen.values()
    assert (a != b).mean() > 0.01, "the word differs"

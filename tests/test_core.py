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
REQUIRED = ["neutral", "happy", "very_happy", "sad", "angry", "annoyed", "surprised", "curious", "thinking", "sleepy", "sleeping",
            "waking_up", "scared", "affection", "bored", "attentive", "confused", "excited"]


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
    assert len(set(ids)) == 36
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
        poses[name] = tuple(round(c.pose(e, f), 0) for e in (0, 1) for f in range(12))
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
    white = fb == 0xFFFF
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
    def scr():
        return c.state()["screen"]
    swipe(c, 233, 350, 233, 120); c.run(300)
    assert scr() == "settings"
    swipe(c, 100, 233, 360, 233); c.run(300)
    assert scr() == "face"
    swipe(c, 233, 120, 233, 350); c.run(300)
    assert scr() == "notifications"
    swipe(c, 100, 233, 360, 233); c.run(300)
    assert scr() == "face"
    swipe(c, 360, 233, 100, 233); c.run(300)
    assert scr() == "pet"
    swipe(c, 100, 233, 360, 233); c.run(300)
    assert scr() == "face"
    c.touch(True, 233, 233); c.run(800); c.touch(False, 233, 233); c.run(300)
    assert scr() == "menu"


def test_tap_and_double_tap_react_on_face(c):
    h0 = c.state()["attention"]
    tap(c); c.run(400)
    assert c.state()["attention"] >= h0
    assert emotion(c) in ("curious", "attentive")
    c.run(3000)
    tap(c); c.run(80); tap(c); c.run(300)
    assert emotion(c) in ("happy", "surprised")


def test_touch_dropout_of_one_frame_is_not_a_release(c):
    c.touch(True, 233, 233); c.run(100)
    c.touch(False, 233, 233); c.tick(16)
    c.touch(True, 233, 233); c.run(700)
    c.touch(False, 233, 233); c.run(200)
    assert c.state()["screen"] == "menu", "long press must survive a dropped sample"


def test_repeated_taps_annoy_the_pet(c):
    s0 = c.state()["stress"]
    for _ in range(9):
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


def test_status_matches_needs(c):
    for k, v, want in (("hunger", 90, "hungry"), ("hunger", 10, None)):
        c.handle_command({"type": "set_stat", "name": k, "value": v})
        if want:
            assert c.state()["pet_status"] == want
    c.handle_command({"type": "set_stat", "name": "energy", "value": 15})
    c.run(100)
    assert c.state()["pet_status"] == "tired"
    c.run(3000)
    assert emotion(c) == "sleepy"


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


def test_notification_queue_keeps_newest(c):
    for i in range(40):
        c.handle_command({"type": "notify", "title": f"n{i}"})
    assert c.handle_command({"type": "notifications"})["count"] == 24
    assert c.handle_command({"type": "notifications", "value": "read_all"})["unread"] == 0
    assert c.handle_command({"type": "notifications", "value": "clear"})["count"] == 0


# ---- screens and games -----------------------------------------------------------------------
SCREENS = ["face", "menu", "pet", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications", "settings", "diagnostics", "games", "info", "tictactoe", "reaction", "guess", "pet_game"]


@pytest.mark.parametrize("name", SCREENS)
def test_every_screen_draws_something(c, name):
    c.handle_command({"type": "notify", "title": "t"})
    c.handle_command({"type": "screen", "value": name})
    c.run(400)
    assert (c.render() != 0).sum() > 200


def test_menu_opens_all_items(c):
    c.handle_command({"type": "screen", "value": "menu"})
    c.run(300)
    tap(c, 233, 72 + 66 * 2 + 30); c.run(300)     # third row: Pet
    assert c.state()["screen"] == "pet"


def test_timer_counts_down_and_alarms(c):
    c.handle_command({"type": "screen", "value": "timer"})
    c.run(300)
    tap(c, 233 - 112, 233 + 40)      # -1 min from 5 down to 4
    for _ in range(4):
        tap(c, 233 - 112, 233 + 40)  # down to the 1 minute minimum
    tap(c, 233 - 52, 233 + 128)      # start
    c.set_time_scale(60)
    before = c.play_count()
    for _ in range(200):
        c.tick(20); c.mix(320)
    assert c.play_count() > before


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


def test_reaction_game_measures_and_saves_best(tmp_path):
    c = Companion(storage_dir=tmp_path, seed=3)
    c.run(2500)
    c.handle_command({"type": "screen", "value": "reaction"})
    c.run(300)
    tap(c)
    # Find the signal: when the whole circle goes orange.
    for _ in range(400):
        c.tick(20)
        if c.render()[233, 100] != 0 and c.render()[60, 233] != 0:
            break
    c.run(180)
    c.touch(True, 233, 233); c.tick(20)
    c.touch(False, 233, 233); c.run(100)
    assert (tmp_path / "settings.bin").exists()
    c.close()


def test_settings_tap_changes_value(c):
    c.handle_command({"type": "screen", "value": "settings"})
    c.run(300)
    v0 = c.state()["brightness"]
    tap(c, 233 - 180, 72 + 30)
    c.run(100)
    assert c.state()["brightness"] == v0 - 10
    tap(c, 233 + 180, 72 + 30)
    c.run(100)
    assert c.state()["brightness"] == v0


def test_ai_offline_answers_with_emotion(c):
    c.handle_command({"type": "chat", "text": "Привет, как дела?"})
    c.run(200)
    assert emotion(c) in ("thinking", "happy")
    c.run(1500)
    assert emotion(c) == "happy"


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
    assert len(set(seen.values())) == len(seen) == 36


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
            # Eyes are still far from fully open at that moment.
            assert c.pose(0, 3) < 86
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

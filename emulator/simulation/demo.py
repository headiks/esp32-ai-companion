"""DEMO MODE script. Each step is (delay before step in ms, command dict or callable)."""


def steps(ctl):
    def emo(name, hold=2400):
        return {"type": "emotion", "value": name, "hold_ms": hold}

    def screen(name):
        return {"type": "screen", "value": name}

    return [
        (0, screen("face")),
        (200, {"type": "action", "value": "sleep"}),            # start of the character: from sleep...
        (2600, {"type": "action", "value": "wake"}),            # ...awakening
        (2600, emo("neutral")),
        (2200, emo("happy")),
        (2600, emo("surprised")),
        (2400, emo("curious")),
        (2600, emo("sad")),
        (3000, emo("angry")),
        (2600, emo("sleepy", 3000)),
        (3200, {"type": "action", "value": "sleep"}),
        (4000, {"type": "action", "value": "wake"}),
        (2800, {"type": "gesture", "value": "tap", "x": 150, "y": 160}),   # reaction to a touch
        (1800, {"type": "gesture", "value": "double_tap"}),
        (2000, {"type": "action", "value": "pet"}),
        (2600, screen("tictactoe")),
        (1200, {"type": "gesture", "value": "tap", "x": 148, "y": 300}),   # choose "with the pet"
        (1200, {"type": "gesture", "value": "tap", "x": 233, "y": 247}),   # centre cell
        (2400, screen("mouse")),
        (1500, screen("maze")),
        (0, lambda: ctl.set_tilt(0.5, 0.0)),
        (1000, lambda: ctl.set_tilt(0.0, 0.5)),
        (1000, lambda: ctl.set_tilt(0.0, 0.0)),
        (600, {"type": "weather", "value": "rain", "temp": 9}),
        (0, screen("weather")),
        (2400, screen("face")),
        (3000, {"type": "weather", "value": "snow", "temp": -4}),
        (3400, screen("analog_clock")),
        (2600, screen("digital_clock")),
        (2600, screen("face")),
        (800, {"type": "notify", "title": "Демо", "text": "Так выглядит уведомление", "icon": "message", "source": "demo"}),
        (4800, screen("notifications")),
        (2400, screen("face")),
        (800, emo("excited")),
    ]

"""JSON-lines link over USB serial or TCP (pyserial URL, e.g. COM5 or socket://127.0.0.1:7777)."""
import json
import threading

import serial
from serial.tools import list_ports


def list_serial_ports():
    return [p.device for p in list_ports.comports()]


class SerialLink:
    def __init__(self, on_message, on_log=None):
        self.on_message = on_message
        self.on_log = on_log or (lambda _: None)
        self.ser = None
        self._next_id = 0
        self._stop = False
        self._thread = None
        self._lock = threading.Lock()

    @property
    def connected(self):
        return self.ser is not None

    def open(self, port, baud=115200):
        self.close()
        self._stop = False
        self.ser = serial.serial_for_url(port, baud, timeout=0.2)
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def close(self):
        self._stop = True
        ser, self.ser = self.ser, None
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=1)
        if ser:
            ser.close()

    def send(self, cmd):
        """Adds a request_id, returns it. Raises OSError when the link is down."""
        if not self.ser:
            raise OSError("not connected")
        with self._lock:
            self._next_id += 1
            rid = self._next_id
            self.ser.write((json.dumps({**cmd, "request_id": rid}, ensure_ascii=False) + "\n").encode())
        return rid

    def _reader(self):
        buf = b""
        while not self._stop:
            try:
                ser = self.ser
                if ser is None:
                    return
                chunk = ser.read(ser.in_waiting or 1)
            except (OSError, serial.SerialException) as e:
                self.on_log(f"serial error: {e}")
                self.ser = None
                return
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.strip()
                if not line.startswith(b"{"):  # firmware log lines share the USB port
                    if line:
                        self.on_log(line.decode("utf-8", "replace"))
                    continue
                try:
                    self.on_message(json.loads(line))
                except ValueError:
                    self.on_log(f"bad json: {line[:80]!r}")

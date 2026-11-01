"""BLE link via bleak, runs its own asyncio loop in a thread. Same interface as SerialLink."""
import asyncio
import json
import threading

from bleak import BleakClient, BleakScanner

SERVICE_UUID = "6e7c0001-5a1e-4c6b-9d3f-0c0a1f2b3c4d"
RX_UUID = "6e7c0002-5a1e-4c6b-9d3f-0c0a1f2b3c4d"  # app -> device (write)
TX_UUID = "6e7c0003-5a1e-4c6b-9d3f-0c0a1f2b3c4d"  # device -> app (notify)
CHUNK = 180


class BleLink:
    def __init__(self, on_message, on_log=None):
        self.on_message = on_message
        self.on_log = on_log or (lambda _: None)
        self.client = None
        self._next_id = 0
        self._buf = b""
        self._loop = asyncio.new_event_loop()
        threading.Thread(target=self._loop.run_forever, daemon=True).start()

    @property
    def connected(self):
        return bool(self.client and self.client.is_connected)

    def scan(self, timeout=5.0):
        """Returns a list of (name, address) of companion devices. Blocks up to timeout seconds."""
        fut = asyncio.run_coroutine_threadsafe(BleakScanner.discover(timeout=timeout, service_uuids=[SERVICE_UUID]), self._loop)
        return [(d.name or "Companion", d.address) for d in fut.result(timeout + 5)]

    def open(self, address):
        asyncio.run_coroutine_threadsafe(self._connect(address), self._loop).result(30)

    async def _connect(self, address):
        self.client = BleakClient(address, disconnected_callback=lambda _: self.on_log("BLE disconnected"))
        await self.client.connect()
        await self.client.start_notify(TX_UUID, self._on_notify)

    def _on_notify(self, _handle, data):
        self._buf += bytes(data)
        while b"\n" in self._buf:
            line, self._buf = self._buf.split(b"\n", 1)
            try:
                self.on_message(json.loads(line))
            except ValueError:
                self.on_log(f"bad json: {line[:80]!r}")

    def send(self, cmd):
        if not self.connected:
            raise OSError("not connected")
        self._next_id += 1
        rid = self._next_id
        payload = (json.dumps({**cmd, "request_id": rid}, ensure_ascii=False) + "\n").encode()
        asyncio.run_coroutine_threadsafe(self._write(payload), self._loop)
        return rid

    async def _write(self, payload):
        for i in range(0, len(payload), CHUNK):
            await self.client.write_gatt_char(RX_UUID, payload[i:i + CHUNK], response=True)

    def close(self):
        if self.client:
            asyncio.run_coroutine_threadsafe(self.client.disconnect(), self._loop).result(10)
            self.client = None

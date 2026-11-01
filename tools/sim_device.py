"""Simulated device: serves the real Companion over a TCP socket speaking the serial JSON-lines protocol.
Connect desktop app / emulator HIL to  socket://127.0.0.1:7777  to test USB-serial path without hardware."""
import json
import socketserver
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from core.companion import Companion  # noqa: E402

comp = None
lock = threading.Lock()


def ticker():
    while True:
        with lock:
            comp.tick(20)
        time.sleep(0.02)


class H(socketserver.StreamRequestHandler):
    def handle(self):
        for line in self.rfile:
            line = line.strip()
            if not line.startswith(b"{"):
                continue
            try:
                cmd = json.loads(line)
            except ValueError:
                resp = {"status": "error", "error": "bad_json"}
            else:
                with lock:
                    resp = comp.handle_command(cmd)
            self.wfile.write((json.dumps(resp) + "\n").encode())


def serve(port=7777):
    global comp
    if comp is None:
        comp = Companion()
        threading.Thread(target=ticker, daemon=True).start()
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("127.0.0.1", port), H) as s:
        s.serve_forever()


if __name__ == "__main__":
    serve(int(sys.argv[1]) if len(sys.argv) > 1 else 7777)

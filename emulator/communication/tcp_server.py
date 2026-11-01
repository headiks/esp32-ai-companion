"""Lets the Android app connect to the emulator over the network, speaking the same JSON lines as the device.
Clients start unauthenticated and need the PIN, because anything on the local network can reach this port."""
import json
import queue
import socket
import socketserver
import threading


def local_ip():
    """The address other machines on the network use for this PC. No traffic is sent by a UDP connect."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("10.255.255.255", 1))
            return s.getsockname()[0]
        except OSError:
            return "127.0.0.1"


class Client:
    def __init__(self, sock):
        self.sock = sock
        self.authed = False
        self.alive = True
        self.lock = threading.Lock()

    def send(self, msg):
        data = (json.dumps(msg, ensure_ascii=False) + "\n").encode()
        with self.lock:
            try:
                self.sock.sendall(data)
            except OSError:
                self.alive = False


class TcpServer:
    MAX_LINE = 2048

    def __init__(self, port=7777):
        self.lines = queue.Queue()
        self.clients = []
        owner = self

        class Handler(socketserver.StreamRequestHandler):
            def handle(self):
                client = Client(self.connection)
                self.connection.settimeout(1.0)  # a stalled phone must not freeze the UI thread in send()
                owner.clients.append(client)
                try:
                    while client.alive:
                        try:
                            line = self.rfile.readline(TcpServer.MAX_LINE)
                        except socket.timeout:
                            continue
                        if not line:
                            break
                        line = line.strip()
                        if line:
                            owner.lines.put((client, line))
                except OSError:
                    pass
                finally:
                    client.alive = False
                    owner.clients.remove(client)

        socketserver.ThreadingTCPServer.allow_reuse_address = True
        self.server = socketserver.ThreadingTCPServer(("0.0.0.0", port), Handler)
        self.server.daemon_threads = True
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def take_lines(self):
        out = []
        while True:
            try:
                out.append(self.lines.get_nowait())
            except queue.Empty:
                return out

    def broadcast(self, msg):
        for c in list(self.clients):
            if c.authed:  # state and controller input are not for strangers on the network
                c.send(msg)

    def close(self):
        for c in list(self.clients):
            c.alive = False
            try:
                c.sock.close()
            except OSError:
                pass
        self.server.shutdown()
        self.server.server_close()

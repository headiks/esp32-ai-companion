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
                # The timeout protects send() from a stalled phone. It also wakes recv(), hence raw recv and own framing:
                # a buffered file object would refuse to read again after the first timeout.
                self.connection.settimeout(1.0)
                owner.clients.append(client)
                buf = b""
                try:
                    while client.alive:
                        try:
                            chunk = self.connection.recv(4096)
                        except socket.timeout:
                            continue
                        if not chunk:
                            break
                        buf += chunk
                        while b"\n" in buf:
                            line, buf = buf.split(b"\n", 1)
                            if line.strip():
                                owner.lines.put((client, line.strip()))
                        if len(buf) > TcpServer.MAX_LINE:  # no newline in sight: report it and start over
                            owner.lines.put((client, buf[:TcpServer.MAX_LINE]))
                            buf = b""
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

import sys, threading, time, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from emulator.communication.serial_link import SerialLink  # noqa: E402
from tools import sim_device  # noqa: E402


class T(unittest.TestCase):
    def test_roundtrip_over_socket(self):
        threading.Thread(target=sim_device.serve, args=(7791,), daemon=True).start()
        time.sleep(0.4)
        got = []
        link = SerialLink(got.append)
        link.open("socket://127.0.0.1:7791")
        rid = link.send({"type": "emotion", "value": "happy"})
        link.send({"type": "emotion", "value": "bogus"})
        for _ in range(50):
            if len(got) >= 2:
                break
            time.sleep(0.05)
        link.close()
        self.assertEqual(got[0]["status"], "success")
        self.assertEqual(got[0]["request_id"], rid)
        self.assertEqual(got[1]["status"], "error")


if __name__ == "__main__":
    unittest.main()

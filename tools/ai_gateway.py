"""Reference AI gateway. The firmware only knows this small HTTP API, so changing AI vendor never touches firmware.

  POST /v1/chat   {"text": "..."}                -> {"text": "...", "emotion": "happy", "audio_url": "..."?}
  POST /v1/voice  raw 16 kHz mono s16 PCM        -> same (STT, then chat, then TTS)
  GET  /audio/<id>                               -> raw 16 kHz mono s16 PCM of a voice reply

Provider: any OpenAI-compatible API, set AI_BASE_URL, AI_API_KEY, AI_MODEL. Without them it answers with local
rules so the whole chain can be tested offline. Voice needs AI_STT_MODEL and AI_TTS_MODEL. Audio stays in memory
of this process only and is forwarded to the provider you configured.
Run: python tools/ai_gateway.py [port]"""
import io
import json
import os
import sys
import urllib.error
import urllib.request
import uuid
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

EMOTIONS = ["neutral", "happy", "very_happy", "sad", "angry", "annoyed", "surprised", "curious", "thinking", "sleepy", "scared",
            "affection", "bored", "attentive", "confused", "excited"]
SYSTEM = ("You are a tiny digital pet living in a round screen. Answer in the user's language in at most 20 words. "
          'Reply as JSON: {"text": "...", "emotion": one of ' + json.dumps(EMOTIONS) + "}.")
BASE = os.environ.get("AI_BASE_URL", "").rstrip("/")
KEY = os.environ.get("AI_API_KEY", "")
MODEL = os.environ.get("AI_MODEL", "")
STT = os.environ.get("AI_STT_MODEL", "")
TTS = os.environ.get("AI_TTS_MODEL", "")
TTS_VOICE = os.environ.get("AI_TTS_VOICE", "alloy")
audio_store = {}


def _api(path, body, content_type="application/json"):
    req = urllib.request.Request(BASE + path, body, {"Content-Type": content_type, "Authorization": f"Bearer {KEY}"})
    with urllib.request.urlopen(req, timeout=45) as r:
        return r.read()


def local_reply(text):
    t = text.lower()
    if "привет" in t or "hello" in t:
        return {"text": "Привет! Рад тебя слышать.", "emotion": "happy"}
    return {"text": "Хм, расскажи ещё.", "emotion": "curious"}


def chat(text):
    if not (BASE and MODEL):
        return local_reply(text)
    body = json.dumps({"model": MODEL, "messages": [{"role": "system", "content": SYSTEM}, {"role": "user", "content": text}]}).encode()
    content = json.loads(_api("/chat/completions", body))["choices"][0]["message"]["content"]
    try:
        out = json.loads(content)
        return {"text": str(out["text"])[:190], "emotion": out["emotion"] if out.get("emotion") in EMOTIONS else "neutral"}
    except (ValueError, KeyError, TypeError):
        return {"text": content[:190], "emotion": "neutral"}


def transcribe(pcm):
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(16000)
        w.writeframes(pcm)
    boundary = uuid.uuid4().hex
    parts = (f'--{boundary}\r\nContent-Disposition: form-data; name="model"\r\n\r\n{STT}\r\n'
             f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="a.wav"\r\nContent-Type: audio/wav\r\n\r\n').encode()
    body = parts + buf.getvalue() + f"\r\n--{boundary}--\r\n".encode()
    return json.loads(_api("/audio/transcriptions", body, f"multipart/form-data; boundary={boundary}"))["text"]


def speak(text):
    body = json.dumps({"model": TTS, "input": text, "voice": TTS_VOICE, "response_format": "pcm"}).encode()
    pcm24 = _api("/audio/speech", body)  # OpenAI-compatible "pcm" is 24 kHz s16; the device plays 16 kHz
    samples = [int.from_bytes(pcm24[i:i + 2], "little", signed=True) for i in range(0, len(pcm24) - 1, 6)]
    samples2 = [int.from_bytes(pcm24[i + 2:i + 4], "little", signed=True) for i in range(0, len(pcm24) - 3, 6)]
    out = bytearray()
    for a, b in zip(samples, samples2):  # 3 input samples -> 2 output samples
        out += a.to_bytes(2, "little", signed=True) + b.to_bytes(2, "little", signed=True)
    return bytes(out)


class Handler(BaseHTTPRequestHandler):
    def _json(self, code, obj):
        data = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        if n > 2_000_000:
            return self._json(413, {"error": "too large"})
        body = self.rfile.read(n)
        try:
            if self.path == "/v1/chat":
                reply = chat(json.loads(body)["text"])
            elif self.path == "/v1/voice":
                if not (BASE and STT):
                    return self._json(501, {"text": "Голос не настроен на сервере", "emotion": "confused"})
                reply = chat(transcribe(body))
            else:
                return self._json(404, {"error": "not found"})
            if TTS and BASE:
                aid = uuid.uuid4().hex
                audio_store[aid] = speak(reply["text"])
                reply["audio_url"] = f"http://{self.headers['Host']}/audio/{aid}"
            self._json(200, reply)
        except (KeyError, ValueError) as e:
            self._json(400, {"error": str(e)})
        except urllib.error.URLError as e:
            self._json(502, {"text": "AI-сервис недоступен", "emotion": "sad", "error": str(e)})

    def do_GET(self):
        pcm = audio_store.pop(self.path.rsplit("/", 1)[-1], None) if self.path.startswith("/audio/") else None
        if pcm is None:
            return self._json(404, {"error": "not found"})
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(pcm)))
        self.end_headers()
        self.wfile.write(pcm)

    def log_message(self, *a):
        pass


def serve(port=8787):
    ThreadingHTTPServer(("0.0.0.0", port), Handler).serve_forever()


if __name__ == "__main__":
    serve(int(sys.argv[1]) if len(sys.argv) > 1 else 8787)

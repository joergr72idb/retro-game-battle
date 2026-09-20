"""
HTTP-polling bridge (C64/Meatloaf, C64/WiC64, CPC/M4). Ported from
retro-tron-battle/server/tron_server.py's HTTPPlayerConn/
http_handle_request/handle_http_connection, extended for:
  - the new VARIANT path segment: /join/<platform>/<name>/<variant>/<pin-or-NONE>
  - GAME<n> selections arriving via /tick (reuses the existing tick slot -
    see docs/protocol.md - recognized before the U/D/L/R direction map)
"""

import asyncio
import time
import urllib.parse
import uuid

from engine.model import display_name_for, log
from engine.pairing import join

http_sessions: dict = {}
recently_expired_sessions: dict = {}


class HTTPPlayerConn:
    """Stands in for a (StreamReader, StreamWriter) pair for an
    HTTP-polled player - ported verbatim from the reference."""

    def __init__(self, session_id: str = ""):
        self._inbox = asyncio.Queue()
        self._outbox = []
        self.closed = False
        self.session_id = session_id
        self.platform = ""
        self.name = ""
        self.last_tick_at = None

    async def readline(self):
        return await self._inbox.get()

    async def read(self, n=256):
        return await self._inbox.get()

    def at_eof(self):
        return self.closed

    def feed_line(self, text: str):
        self._inbox.put_nowait((text + "\n").encode("ascii"))

    def write(self, data: bytes):
        self._outbox.append(data.decode("ascii", errors="ignore"))

    async def drain(self):
        pass

    def close(self):
        self.closed = True
        if self.session_id:
            loop = asyncio.get_running_loop()
            loop.call_later(15, self._expire)

    def _expire(self):
        if self.session_id and http_sessions.get(self.session_id) is self:
            del http_sessions[self.session_id]
            recently_expired_sessions[self.session_id] = time.monotonic()

    def get_extra_info(self, *a, **k):
        return None

    def pop_outbox(self) -> str:
        text = "".join(self._outbox)
        self._outbox.clear()
        return text


HTTP_DIR_MAP = {"U": "MOVE U", "D": "MOVE D", "L": "MOVE L", "R": "MOVE R"}


async def http_handle_request(path: str) -> str:
    """/join/<platform>/<name>/<variant>/<pin-or-NONE> and
    /tick/<session>/<direction-or-GAMEn-or-N>"""
    segments = [s for s in path.strip("/").split("/") if s != ""]

    if not segments:
        return "GAME BATTLE SERVER OK"

    if segments[0] == "join" and len(segments) >= 3:
        platform = segments[1].upper()
        name = segments[2]
        variant = segments[3] if len(segments) > 3 else ""
        pin = segments[4] if len(segments) > 4 and segments[4].upper() != "NONE" else ""
        name = display_name_for(name, pin)

        session_id = uuid.uuid4().hex[:8].upper()
        conn = HTTPPlayerConn(session_id)
        conn.platform = platform
        conn.name = name
        http_sessions[session_id] = conn
        print(f"[http] JOIN: new session stored: {session_id!r}  (current keys: {list(http_sessions.keys())!r})")

        asyncio.create_task(join(platform, name, variant, pin, conn, conn, peer_desc="HTTP"))
        await asyncio.sleep(0.05)
        return f"SESSION {session_id}\n" + conn.pop_outbox()

    if segments[0] == "tick" and len(segments) >= 2:
        session_id = segments[1].upper()  # deliberately case-tolerant, see Meatloaf PETSCII note
        token = segments[2].upper() if len(segments) > 2 else "N"
        conn = http_sessions.get(session_id)
        if conn is None:
            expired_ago = recently_expired_sessions.get(session_id)
            reason = (f"expired {time.monotonic() - expired_ago:.1f}s ago"
                      if expired_ago is not None else "never existed here")
            print(f"[http] TICK: requested session {session_id!r} NOT found "
                  f"({reason}). current keys: {list(http_sessions.keys())!r}")
            return "ERR UNKNOWN SESSION"
        now = time.monotonic()
        if conn.last_tick_at is not None:
            gap_ms = (now - conn.last_tick_at) * 1000
            log(f"[tick-latency] {conn.platform}/{conn.name}: {gap_ms:.0f} ms since last /tick")
        conn.last_tick_at = now

        was_selection = False
        if token.startswith("GAME") and token[4:].isdigit():
            conn.feed_line(f"GAME {token[4:]}")
            was_selection = True
        elif token in HTTP_DIR_MAP:
            conn.feed_line(HTTP_DIR_MAP[token])

        if was_selection:
            # give the selection a brief moment to resolve (possibly an
            # instant pairing), same rationale as /join's own short sleep
            await asyncio.sleep(0.05)
        return conn.pop_outbox()

    return "ERR UNKNOWN REQUEST"


async def handle_http_connection(reader, writer):
    try:
        request_line = await asyncio.wait_for(reader.readline(), timeout=5)
        if not request_line:
            writer.close()
            return
        while True:
            line = await asyncio.wait_for(reader.readline(), timeout=2)
            if not line or line in (b"\r\n", b"\n"):
                break

        try:
            method, raw_path, _ = request_line.decode("ascii", errors="ignore").split()
        except ValueError:
            print(f"[http] couldn't parse request line: {request_line!r}")
            writer.close()
            return

        path = urllib.parse.unquote(raw_path)
        print(f"[http] {method} {path!r}  (raw request line: {request_line!r})")
        body = await http_handle_request(path) if method == "GET" else "ERR METHOD"

        response = (
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            f"Content-Length: {len(body)}\r\n"
            "Connection: close\r\n"
            "\r\n" + body
        )
        writer.write(response.encode("ascii", errors="ignore"))
        await writer.drain()
    except Exception as e:
        print(f"[http] request error: {e}")
    finally:
        try:
            writer.close()
        except Exception:
            pass

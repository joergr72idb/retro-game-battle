#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 joergr72idb - see LICENSE
"""Test server for test/fntest.c.

    python3 test/testserver.py [port] [timeout_seconds]

Each connection says what it is in its first line: REPORT (the results, printed here),
or T1-T4 (a test case). An HTTP server on port+2 answers the HTTP tests (H1-H9). Exits 0
when fntest reports DONE PASS and the server-side checks passed, 1 otherwise (or on timeout).
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 6510
TIMEOUT = float(sys.argv[2]) if len(sys.argv) > 2 else 300

result = {"done": None, "server_ok": True}
finished = threading.Event()


def pattern(n, seed):
    return bytes((i * 7 + seed) & 255 for i in range(n))


def read_line(conn):
    data = b""
    while not data.endswith(b"\n"):
        b = conn.recv(1)
        if not b:
            break
        data += b
    return data.strip().decode("ascii", "replace")


def read_exact(conn, n):
    data = b""
    while len(data) < n:
        b = conn.recv(n - len(data))
        if not b:
            break
        data += b
    return data


def handle(conn):
    with conn:
        conn.settimeout(60)
        try:
            what = read_line(conn)
            if what == "REPORT":
                buf = b""
                while True:
                    b = conn.recv(256)
                    if not b:
                        break
                    buf += b
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        text = line.strip().decode("ascii", "replace")
                        print("CPC:", text, flush=True)
                        if text.startswith("DONE"):
                            result["done"] = text
                            finished.set()
            elif what == "T1":
                conn.sendall(pattern(3000, 3))
            elif what == "T2":
                got = read_exact(conn, 2000)
                ok = got == pattern(2000, 5)
                print("server: T2 received", len(got), "bytes,", "ok" if ok else "WRONG", flush=True)
                result["server_ok"] &= ok
                conn.sendall(b"OK2\n")
                read_exact(conn, 1)     # wait for the client to close
            elif what == "T3":
                data = pattern(500, 9)
                for i in range(0, 500, 100):
                    conn.sendall(data[i:i + 100])
                    time.sleep(0.2)
            elif what == "T4":
                conn.sendall(b"HI\n")
                read_exact(conn, 1)
            else:
                print("server: unknown connection", repr(what), flush=True)
                result["server_ok"] = False
        except OSError as e:
            print("server:", what if "what" in dir() else "?", "error", e, flush=True)
            result["server_ok"] = False


class Http(BaseHTTPRequestHandler):
    """the HTTP tests: checks every request, answers with what the CPC compares"""
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt, *args):
        pass

    def answer(self, code, body, extra=()):
        ok = (self.request_version == "HTTP/1.0"
              and self.headers.get("User-Agent") == "fujinet-network-cpc"
              and self.headers.get("Host", "").endswith(":%d" % (PORT + 2)))
        if not ok:
            print("server: bad request head", self.requestline, dict(self.headers), flush=True)
            result["server_ok"] = False
        self.send_response(code)
        self.send_header("Content-Length", str(len(body)))
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        try:
            self.wfile.write(body)
        except BrokenPipeError:     # a request the CPC closed without finishing (H-collect)
            pass

    def do_GET(self):
        if self.path == "/big":         # a header line longer than the CPC's buffer, 3000 bytes
            self.answer(200, pattern(3000, 11), [("X-Long", "a" * 400)])
        elif self.path == "/hdr":
            self.answer(200, b"X=" + self.headers.get("X-Test", "").encode())
        elif self.path == "/a%20b":
            self.answer(200, b"SPACE")
        else:
            self.answer(404, b"nope")

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        data = self.rfile.read(n)
        if n >= 100:
            data = b"OK" if data == pattern(n, 13) else b"BAD"
        self.answer(200, ("%s LEN=%d:" % (self.command, n)).encode() + data)

    do_PUT = do_POST

    def do_DELETE(self):
        self.answer(200, b"DELETED" if self.path == "/item" else b"?")


def main():
    http = ThreadingHTTPServer(("0.0.0.0", PORT + 2), Http)
    threading.Thread(target=http.serve_forever, daemon=True).start()
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", PORT))
    srv.listen(8)
    srv.settimeout(0.5)
    print("test server on port", PORT, flush=True)
    end = time.time() + TIMEOUT
    while not finished.is_set() and time.time() < end:
        try:
            conn, _ = srv.accept()
        except socket.timeout:
            continue
        threading.Thread(target=handle, args=(conn,), daemon=True).start()
    if not finished.is_set():
        print("server: timeout, no DONE from the CPC", flush=True)
        sys.exit(1)
    ok = result["done"] == "DONE PASS" and result["server_ok"]
    print("RESULT:", "PASS" if ok else "FAIL", flush=True)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

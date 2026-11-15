#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 joergr72idb - see LICENSE
"""Test server for test/fntest.c.

    python3 test/testserver.py [port] [timeout_seconds]

Each connection says what it is in its first line: REPORT (the results, printed here),
or T1-T4 (a test case). Exits 0 when fntest reports DONE PASS and the server-side
checks passed, 1 otherwise (or on timeout).
"""
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


def main():
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

#!/usr/bin/env python3
"""Real-time run of CPCSyntaxError for `make shot`: starts the emulator with a hidden window and
its command API on 127.0.0.1:PORT, types CMD, then KEYS, and after FRAMES frames (counted from
CMD) saves a screenshot. In real time (unlike the emulator's own --shot mode, which runs as fast
as it can), so network programs see the same timing as on a real CPC.

    cpcshot.py PORT FRAMES 'CMD' 'KEYS' OUT.png -- cpcse-command...

KEYS: commands separated by ';', one after another after CMD (50 frames = 1 s):
    wait N            nothing for N frames
    type TEXT         typed as a person would (capitals with SHIFT)
    key NAME          one key, held 4 frames: RETURN/ENTER SPACE ESC DEL COPY TAB UP DOWN LEFT RIGHT,
                      F0-F9, a letter or digit, or an emulator key code (KeyA, Digit1, ControlLeft, ...)
    joy DIR [N]       joystick 0 for N frames (default 16), as taps of the cursor keys (the emulator's
                      joystick emulation): up down left right; fire = Ctrl
"""
import json
import os
import signal
import socket
import subprocess
import sys
import time

NAMES = {"RETURN": "Enter", "ENTER": "Enter", "SPACE": "Space", "ESC": "Escape", "DEL": "Backspace",
         "COPY": "AltLeft", "TAB": "Tab", "UP": "ArrowUp", "DOWN": "ArrowDown", "LEFT": "ArrowLeft",
         "RIGHT": "ArrowRight", "FIRE": "ControlLeft"}
JOY = {"up": "ArrowUp", "down": "ArrowDown", "left": "ArrowLeft", "right": "ArrowRight", "fire": "ControlLeft"}


def key_code(name):
    up = name.upper()
    if up in NAMES:
        return NAMES[up]
    if len(up) == 2 and up[0] == "F" and up[1].isdigit():
        return "F" + up[1]
    if len(name) == 1 and name.isalpha():
        return "Key" + up
    if len(name) == 1 and name.isdigit():
        return "Digit" + name
    if len(name) > 1:
        return name                      # an emulator key code as it is
    sys.exit(f"cpcshot: unknown key {name!r}")


def parse(script):
    """-> list of (request, frames it takes); exits on errors before the emulator starts"""
    out = []
    for cmd in script.split(";"):
        cmd = cmd.strip()
        if not cmd:
            continue
        word, _, arg = cmd.partition(" ")
        if word == "wait":
            out.append((f"frames {int(arg)}", int(arg)))
        elif word == "type":
            out.append(({"cmd": "type", "text": arg.replace("\\n", "\n"), "wait": 1}, 8 * len(arg)))
        elif word == "key":
            out.append(({"cmd": "key", "key": key_code(arg.strip())}, 0))
            out.append(("wait typed", 8))
        elif word == "joy":
            parts = arg.split()
            if parts[0].lower() not in JOY:
                sys.exit(f"cpcshot: unknown direction {parts[0]!r} (up down left right fire)")
            n = int(parts[1]) if len(parts) > 1 else 16
            for _ in range(max(1, n // 8)):              # a tap is 4 frames down, 4 up
                out.append(({"cmd": "key", "key": JOY[parts[0].lower()]}, 0))
            out.append(("wait typed", 8 * max(1, n // 8)))
        else:
            sys.exit(f"cpcshot: unknown command {word!r}")
    return out


class Api:
    def __init__(self, port):
        deadline = time.time() + 30
        while True:
            try:
                self.s = socket.create_connection(("127.0.0.1", port), timeout=120)
                break
            except OSError:
                if time.time() > deadline:
                    sys.exit("cpcshot: the emulator's command API did not come up")
                time.sleep(0.2)
        self.f = self.s.makefile("rw")

    def __call__(self, req):
        self.f.write((json.dumps(req) if isinstance(req, dict) else req) + "\n")
        self.f.flush()
        line = self.f.readline()
        if not line:
            sys.exit("cpcshot: the emulator closed the connection")
        r = json.loads(line)
        if not r.get("ok"):
            sys.exit(f"cpcshot: {req} -> {r.get('error')}")
        return r


def main():
    if "--" not in sys.argv or sys.argv.index("--") != 6:
        sys.exit(__doc__)
    port, frames, boot, script, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
    boot = boot.replace("\\n", "\n")                 # RUN"X"\n from the command line: \n = ENTER
    emu = sys.argv[7:]
    steps = parse(script)
    need = sum(n for _, n in steps)
    if need > frames:
        sys.exit(f"cpcshot: the script needs about {need} frames, FRAMES is {frames}")

    probe = socket.socket()
    probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        probe.bind(("127.0.0.1", port))
    except OSError:
        sys.exit(f"cpcshot: port {port} is in use (another emulator with its API on?) - use API_PORT=...")
    finally:
        probe.close()

    env = dict(os.environ, CPCSE_HIDDEN_WINDOW="1", SDL_AUDIODRIVER="dummy")
    log = open(os.path.splitext(out)[0] + ".log", "w")
    proc = subprocess.Popen(emu + [f"--api={port}"], env=env, stdout=log, stderr=subprocess.STDOUT,
                            start_new_session=True)
    try:
        api = Api(port)
        while not api("status").get("booted"):
            time.sleep(0.2)
        api("frames 100")                                  # the firmware's start-up
        start = api("status")["frames"]
        if boot:
            api({"cmd": "type", "text": boot, "wait": 1})
        for req, _ in steps:
            api(req)
        left = frames - (api("status")["frames"] - start)
        if left > 0:
            api(f"frames {left}")
        api({"cmd": "screenshot", "path": os.path.abspath(out)})
        print(f"cpcshot: {api('status')['frames'] - start} frames, screenshot {out}")
        api("quit")
        proc.wait(timeout=10)
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)


main()

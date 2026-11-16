#!/usr/bin/env python3
"""Headless run of VICE for `make shot`: starts x64sc without a window (SDL dummy video, dummy
sound) with its binary monitor on 127.0.0.1:PORT, presses keys / moves joysticks at the times of a
short script, and after FRAMES frames saves the screen as PNG and the text screen as text.

    c64shot.py PORT FRAMES 'SCRIPT' OUT_PREFIX -- x64sc-command...

SCRIPT: commands separated by ';', one after another from the start (PAL: 50 frames = 1 s; timed
by the clock, as VICE runs at real speed):
    wait N               nothing for N frames
    type TEXT            typed through the keyboard buffer (as the KERNAL reads it: BASIC, cc65's
                         cgetc); upper case letters come out shifted
    key NAME             one key through the keyboard buffer: RETURN DEL HOME CLR UP DOWN LEFT RIGHT
                         F1..F8 RUNSTOP SPACE, or a character
    press NAME [N]       one key held in the keyboard matrix for N frames (default 3), for programs
                         that scan the keyboard themselves: a letter or digit, SPACE RETURN DEL
                         RUNSTOP F1 F3 F5 F7 DOWN RIGHT CTRL CBM LSHIFT RSHIFT HOME, or RESTORE;
                         several at once with '+' (pressed in that order 2 frames apart, released
                         together): press RUNSTOP+RESTORE, press LSHIFT+X
    joy DIR [N]          joystick in port 2 for N frames (default 10): up down left right upleft ...,
                         any of them +fire, or fire alone
    joy1 DIR [N]         the same in port 1
"""
import os
import signal
import socket
import struct
import subprocess
import sys
import time
import zlib

FPS = 50.0

# ---- script ---------------------------------------------------------------------------------------
PETSCII_KEYS = {
    "RETURN": 13, "DEL": 20, "HOME": 19, "CLR": 147, "UP": 145, "DOWN": 17, "LEFT": 157,
    "RIGHT": 29, "F1": 133, "F3": 134, "F5": 135, "F7": 136, "F2": 137, "F4": 138, "F6": 139,
    "F8": 140, "RUNSTOP": 3, "SPACE": 32,
}
# keyboard matrix (row = CIA1 port A bit, column = port B bit)
MATRIX = {}
for row, keys in enumerate([
        ["DEL", "RETURN", "RIGHT", "F7", "F1", "F3", "F5", "DOWN"],
        ["3", "W", "A", "4", "Z", "S", "E", "LSHIFT"],
        ["5", "R", "D", "6", "C", "F", "T", "X"],
        ["7", "Y", "G", "8", "B", "H", "U", "V"],
        ["9", "I", "J", "0", "M", "K", "O", "N"],
        ["+", "P", "L", "-", ".", ":", "@", ","],
        ["POUND", "*", ";", "HOME", "RSHIFT", "=", "UPARROW", "/"],
        ["1", "LEFTARROW", "CTRL", "2", "SPACE", "CBM", "Q", "RUNSTOP"]]):
    for col, k in enumerate(keys):
        MATRIX[k] = (row, col)
STICK = {"up": 1, "down": 2, "left": 4, "right": 8, "upleft": 5, "upright": 9,
         "downleft": 6, "downright": 10, "": 0}


def petscii(c):
    o = ord(c)
    if "a" <= c <= "z":
        return o - 32               # unshifted letter
    if "A" <= c <= "Z":
        return o + 128              # shifted letter
    if 32 <= o < 96 and c not in "\\_`":
        return o
    sys.exit(f"c64shot: no PETSCII key for {c!r}")


def parse(script, total):
    """-> list of (frame, action, args); exits on errors before VICE starts"""
    out, t = [], 0
    for cmd in script.split(";"):
        cmd = cmd.strip()
        if not cmd:
            continue
        word, _, arg = cmd.partition(" ")
        parts = arg.split()
        if word == "wait":
            t += int(arg)
        elif word == "type":
            data = bytes(petscii(c) for c in arg)
            out.append((t, "feed", data))
            t += 2 * len(data)                    # the KERNAL takes about one key per frame
        elif word == "key":
            name = arg.strip()
            if name.upper() in PETSCII_KEYS:
                data = bytes([PETSCII_KEYS[name.upper()]])
            elif len(name) == 1:
                data = bytes([petscii(name)])
            else:
                sys.exit(f"c64shot: unknown key {name!r}")
            out.append((t, "feed", data))
            t += 3
        elif word == "press":
            # A+B+...: pressed in this order 2 frames apart, released together - the KERNAL sees
            # RUN/STOP only at its next keyboard scan, so RUNSTOP+RESTORE needs RUN/STOP down first
            names = [parts[0].upper()] if parts[0] == "+" else parts[0].upper().split("+")
            n = int(parts[1]) if len(parts) > 1 else 3
            for i, name in enumerate(names):
                if name != "RESTORE" and name not in MATRIX:
                    sys.exit(f"c64shot: unknown matrix key {name!r}")
                out.append((t + 2 * i, "matrix", (name, n + 2 * (len(names) - 1 - i))))
            t += n + 2 * (len(names) - 1) + 3
        elif word in ("joy", "joy1"):
            n = int(parts[1]) if len(parts) > 1 else 10
            d = parts[0].lower()
            fire = d == "fire" or d.endswith("+fire")
            d = "" if d == "fire" else d.removesuffix("+fire")
            if d not in STICK:
                sys.exit(f"c64shot: unknown direction {parts[0]!r}")
            out.append((t, "joy", (2 if word == "joy" else 1, STICK[d] | (16 if fire else 0), n)))
            t += n + 3
        else:
            sys.exit(f"c64shot: unknown command {word!r}")
    if t > total:
        sys.exit(f"c64shot: the script needs {t} frames, FRAMES is {total}")
    return out


# ---- binary monitor -------------------------------------------------------------------------------
class Monitor:
    def __init__(self, port):
        deadline = time.time() + 30
        while True:
            try:
                self.s = socket.create_connection(("127.0.0.1", port), timeout=10)
                break
            except OSError:
                if time.time() > deadline:
                    sys.exit("c64shot: VICE's binary monitor did not come up")
                time.sleep(0.2)
        self.rid = 0

    def _recv(self, n):
        b = b""
        while len(b) < n:
            chunk = self.s.recv(n - len(b))
            if not chunk:
                sys.exit("c64shot: VICE closed the monitor connection")
            b += chunk
        return b

    def cmd(self, ctype, body=b""):
        """send a command, return its response body (events in between are skipped)"""
        self.rid += 1
        self.s.sendall(struct.pack("<BBIIB", 2, 2, len(body), self.rid, ctype) + body)
        while True:
            stx, api, length, rtype, err, rid = struct.unpack("<BBIBBI", self._recv(12))
            data = self._recv(length)
            if rid == self.rid:
                if err:
                    sys.exit(f"c64shot: monitor command {ctype:#x} failed, error {err:#x}")
                return data

    def resume(self):
        self.cmd(0xAA)

    def mem(self, start, end):
        return self.cmd(0x01, struct.pack("<BHHBH", 0, start, end, 0, 0))


# ---- output ---------------------------------------------------------------------------------------
def write_png(path, w, h, rgb_rows):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    raw = b"".join(b"\0" + r for r in rgb_rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def screenshot(mon, path):
    d = mon.cmd(0x84, bytes([0, 0]))                      # display, 8 bit indexed
    info_len = struct.unpack_from("<I", d, 0)[0]
    dw, dh, xo, yo, iw, ih, bpp = struct.unpack_from("<HHHHHHB", d, 4)
    buf = d[4 + info_len + 4:]
    p = mon.cmd(0x91, bytes([0]))                         # palette
    n = struct.unpack_from("<H", p, 0)[0]
    pal, o = [], 2
    for _ in range(n):
        size = p[o]
        pal.append(bytes(p[o + 1:o + 4]))
        o += 1 + size
    # the visible picture with a border of 32 pixels around the inner 320x200 area
    x0, y0 = max(0, xo - 32), max(0, yo - 35)
    x1, y1 = min(dw, xo + iw + 32), min(dh, yo + ih + 35)
    rows = [b"".join(pal[buf[y * dw + x]] for x in range(x0, x1)) for y in range(y0, y1)]
    write_png(path, x1 - x0, y1 - y0, rows)


def text_screen(mon, path):
    d018, dd00 = mon.mem(0xD018, 0xD018)[2], mon.mem(0xDD00, 0xDD00)[2]
    base = (3 - (dd00 & 3)) * 0x4000 + (d018 >> 4) * 0x400
    lower = d018 & 2                                     # the upper/lower case character set
    scr = mon.mem(base, base + 999)[2:]
    lines = []
    for r in range(25):
        line = ""
        for c in scr[r * 40:r * 40 + 40]:
            c &= 0x7F                                    # reverse characters as normal ones
            if c == 0:
                ch = "@"
            elif c <= 26:
                ch = chr(c + (96 if lower else 64))
            elif c < 32:
                ch = "[£]^_"[c - 27]                       # [ pound ] up-arrow left-arrow
            elif c < 64:
                ch = chr(c)
            elif lower and 65 <= c <= 90:
                ch = chr(c)
            else:
                ch = " " if c == 96 else "#"             # graphics characters
            line += ch
        lines.append(line.rstrip())
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


# ---- main -----------------------------------------------------------------------------------------
def main():
    if "--" not in sys.argv or sys.argv.index("--") != 5:
        sys.exit(__doc__)
    port, frames, script, outp = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4]
    vice = sys.argv[6:]
    events = parse(script, frames)

    probe = socket.socket()
    probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)   # a closed earlier run's TIME_WAIT is fine
    try:
        probe.bind(("127.0.0.1", port))
    except OSError:
        sys.exit(f"c64shot: monitor port {port} is in use (another VICE?) - use MON_PORT=...")
    finally:
        probe.close()

    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    if env.get("BRIDGE_LIBPATH") and env.get("BRIDGE_DIR"):    # where VICE finds the bridge (libopencbm)
        env[env["BRIDGE_LIBPATH"]] = env["BRIDGE_DIR"]
    log = open(outp + ".vice.log", "w")
    proc = subprocess.Popen(vice + ["-binarymonitor", "-binarymonitoraddress",
                                    f"ip4://127.0.0.1:{port}", "-sounddev", "dummy"],
                            env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        mon = Monitor(port)
        mon.resume()
        t0 = time.time()
        for frame, action, args in events:
            time.sleep(max(0, t0 + frame / FPS - time.time()))
            if action == "feed":
                mon.cmd(0x72, bytes([len(args)]) + args)
            elif action == "matrix":
                name, n = args
                row, col = (0xFF, 0) if name == "RESTORE" else MATRIX[name]
                mon.cmd(0x74, struct.pack("<BBBH", row, col, 1, n))
            elif action == "joy":
                p, value, n = args
                mon.cmd(0xA3, struct.pack("<BHH", p, value, n))
            mon.resume()
        time.sleep(max(0, t0 + frames / FPS - time.time()))
        mon.cmd(0x81)                                     # stops the machine for the reads
        screenshot(mon, outp + ".png")
        text_screen(mon, outp + ".txt")
        print(f"c64shot: {frames} frames, screen {outp}.png, text {outp}.txt")
        try:
            mon.cmd(0xBB)                                 # quit VICE
        except SystemExit:
            pass
        proc.wait(timeout=10)
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)


main()

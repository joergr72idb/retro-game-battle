#!/usr/bin/env python3
"""Turns a short key script into an atari800 input playback file (-playback), for headless runs
(make shot). atari800 reads one frame of input per line group from the file and quits at its end,
so the file also sets how many frames the run lasts.

    keyscript.py FRAMES 'SCRIPT' > file

SCRIPT: commands separated by ';', run one after another from frame 0 (PAL: 50 frames = 1 s):
    wait N               nothing pressed for N frames
    type TEXT            types TEXT as a person would: upper case letters with Shift (so the
                         Atari's caps lock doesn't matter for them); each key 3 frames down, 3 up
    key NAME [N]         one key for N frames (default 3), then 3 up: RETURN SPACE ESC TAB
                         BACKSPACE DELETE UP DOWN LEFT RIGHT HELP BREAK CAPS, a single character,
                         CTRL-x / SHIFT-x, or a console key START SELECT OPTION
    joy DIR [N]          joystick 0 for N frames (default 10): up down left right upleft upright
                         downleft downright, any of them +fire, or fire alone; then released
    joy1 DIR [N]         the same for joystick 1
After the script, nothing is pressed until FRAMES. The script must fit into FRAMES.
"""
import sys

# AKEY codes (atari800 akey.h); 0x40 = Shift, 0x80 = Ctrl
LETTERS = dict(zip("abcdefghijklmnopqrstuvwxyz",
                   [0x3f, 0x15, 0x12, 0x3a, 0x2a, 0x38, 0x3d, 0x39, 0x0d, 0x01, 0x05, 0x00, 0x25,
                    0x23, 0x08, 0x0a, 0x2f, 0x28, 0x3e, 0x2d, 0x0b, 0x10, 0x2e, 0x16, 0x2b, 0x17]))
CHARS = {
    "0": 0x32, "1": 0x1f, "2": 0x1e, "3": 0x1a, "4": 0x18, "5": 0x1d, "6": 0x1b, "7": 0x33,
    "8": 0x35, "9": 0x30, " ": 0x21, "!": 0x5f, '"': 0x5e, "#": 0x5a, "$": 0x58, "%": 0x5d,
    "&": 0x5b, "'": 0x73, "@": 0x75, "(": 0x70, ")": 0x72, "<": 0x36, ">": 0x37, "=": 0x0f,
    "?": 0x66, "-": 0x0e, "+": 0x06, "*": 0x07, "/": 0x26, ":": 0x42, ";": 0x02, ",": 0x20,
    ".": 0x22, "_": 0x4e, "[": 0x60, "]": 0x62, "^": 0x47, "\\": 0x46, "|": 0x4f,
}
NAMES = {
    "RETURN": 0x0c, "SPACE": 0x21, "ESC": 0x1c, "TAB": 0x2c, "BACKSPACE": 0x34, "DELETE": 0x34,
    "UP": 0x8e, "DOWN": 0x8f, "LEFT": 0x86, "RIGHT": 0x87, "HELP": 0x11, "BREAK": -5,
    "CAPS": 0x3c, "CLEAR": 0x76,
}
CONSOL = {"START": 0x01, "SELECT": 0x02, "OPTION": 0x04}     # bit cleared = pressed
STICK = {"up": 0x0e, "down": 0x0d, "left": 0x0b, "right": 0x07, "upleft": 0x0a,
         "upright": 0x06, "downleft": 0x09, "downright": 0x05, "": 0x0f}


def char_code(c):
    if c.lower() in LETTERS:
        return LETTERS[c.lower()] | (0x40 if c.isupper() else 0)
    if c in CHARS:
        return CHARS[c]
    sys.exit(f"keyscript: no key for {c!r}")


def key_code(name):
    up = name.upper()
    if up in NAMES:
        return NAMES[up]
    for prefix, bit in (("CTRL-", 0x80), ("SHIFT-", 0x40)):
        if up.startswith(prefix) and len(name) == len(prefix) + 1:
            return char_code(name[-1]) | bit
    if len(name) == 1:
        return char_code(name)
    sys.exit(f"keyscript: unknown key {name!r}")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    total = int(sys.argv[1])
    frames = []                   # per frame: key, shift, consol, stick0, stick1, trig0, trig1

    def hold(n, key=-1, consol=0x07, stick0=0x0f, stick1=0x0f, trig0=1, trig1=1):
        shift = 1 if key >= 0 and key & 0x40 else 0
        frames.extend([(key, shift, consol, stick0, stick1, trig0, trig1)] * n)

    for cmd in sys.argv[2].split(";"):
        word, _, arg = cmd.strip().partition(" ")
        if not word:
            continue
        if word == "wait":
            hold(int(arg))
        elif word == "type":
            for c in arg:
                hold(3, char_code(c))
                hold(3)
        elif word == "key":
            parts = arg.split()
            n = int(parts[1]) if len(parts) > 1 else 3
            if parts[0].upper() in CONSOL:
                hold(n, consol=0x07 & ~CONSOL[parts[0].upper()])
            else:
                hold(n, key_code(parts[0]))
            hold(3)
        elif word in ("joy", "joy1"):
            parts = arg.split()
            n = int(parts[1]) if len(parts) > 1 else 10
            d = parts[0].lower()
            fire = d == "fire" or d.endswith("+fire")
            d = "" if d == "fire" else d.removesuffix("+fire")
            if d not in STICK:
                sys.exit(f"keyscript: unknown direction {parts[0]!r}")
            if word == "joy":
                hold(n, stick0=STICK[d], trig0=0 if fire else 1)
            else:
                hold(n, stick1=STICK[d], trig1=0 if fire else 1)
            hold(3)
        else:
            sys.exit(f"keyscript: unknown command {word!r}")
    if len(frames) > total:
        sys.exit(f"keyscript: the script needs {len(frames)} frames, FRAMES is {total}")
    hold(total - len(frames))

    # the format atari800 (-record) writes and -playback reads (master after 7.2.1; 7.2.1 itself
    # reads it differently): header, the POKEY random seed, then two lines per frame - key, shift,
    # console keys, port 0 (joystick 0 low nibble, joystick 1 high nibble), port 1 (joysticks 2/3),
    # the four triggers; then a screen checksum, only compared and logged ("adler32 does not
    # match" is expected)
    out = ["Atari800 event recording, version: 1", "0"]
    for key, shift, consol, s0, s1, t0, t1 in frames:
        out += [f"{key} {shift} {consol} {s0 | s1 << 4} 255 {t0} {t1} 1 1 ", "00000000 "]
    print("\n".join(out))


main()

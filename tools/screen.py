#!/usr/bin/env python3
"""Prints the C64 text screen from a VICE monitor memory dump (m 0400 07e7)."""
import re, sys

data = []
for line in open(sys.argv[1], errors="replace"):
    m = re.search(r">C:([0-9a-f]{4})((?:\s+[0-9a-f]{2}){1,16})", line)
    if m:
        data += [int(x, 16) for x in m.group(2).split()]

def char(c):
    c &= 0x7F                       # ignore reverse video
    if c == 0:
        return "@"
    if 1 <= c <= 26:
        return chr(c + 96)
    if 32 <= c <= 63:
        return chr(c)
    if 65 <= c <= 90:               # uppercase in the lower/upper case character set
        return chr(c)
    return " "

text = "".join(char(c) for c in data[:1000])
for i in range(0, len(text), 40):
    print(text[i:i + 40].rstrip())

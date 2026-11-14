#!/usr/bin/env python3
"""Turns an SDCC .ihx file into a CPC binary file with an AMSDOS header.

    cpcbin.py in.ihx OUT.BIN [entry]     entry defaults to the lowest address (the crt0 jump)

BASIC loads the result with RUN"OUT" (from disc or from the M4's SD card).
"""
import os, sys

def read_ihx(path):
    mem = {}
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        n, addr, kind = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        if kind == 0:
            for i in range(n):
                mem[addr + i] = int(line[9 + 2 * i:11 + 2 * i], 16)
    lo, hi = min(mem), max(mem)
    return lo, bytes(mem.get(a, 0) for a in range(lo, hi + 1))

def amsdos_header(name, load, entry, length):
    base, _, ext = os.path.basename(name).upper().partition(".")
    h = bytearray(128)
    h[1:9] = base[:8].ljust(8).encode("ascii")
    h[9:12] = ext[:3].ljust(3).encode("ascii")
    h[18] = 2                                   # binary
    h[21:23] = load.to_bytes(2, "little")
    h[23] = 0xFF
    h[24:26] = length.to_bytes(2, "little")
    h[26:28] = entry.to_bytes(2, "little")
    h[64:67] = length.to_bytes(3, "little")
    h[67:69] = (sum(h[0:67]) & 0xFFFF).to_bytes(2, "little")
    return bytes(h)

load, data = read_ihx(sys.argv[1])
entry = int(sys.argv[3], 0) if len(sys.argv) > 3 else load
with open(sys.argv[2], "wb") as f:
    f.write(amsdos_header(sys.argv[2], load, entry, len(data)) + data)
print(f"{sys.argv[2]}: load &{load:04X}, entry &{entry:04X}, {len(data)} bytes")

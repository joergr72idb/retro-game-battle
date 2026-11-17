# Development Kit for the Amstrad/Schneider CPC and the M4 Board

A self-contained toolchain for macOS and Linux for CPC programs that use the M4 board's network, tested in an
emulator whose M4 reaches the internet through the computer's network. Everything is built into `opt/` inside this
folder; nothing is installed system-wide except a few Homebrew or apt packages.

**Vibe coded:** this kit - the setup scripts, Makefiles, tools, samples and docs - was generated with
[Claude Code](https://claude.com/claude-code), Anthropic's AI coding assistant, from prompts; every commit
carries a `Co-Authored-By: Claude` line. The tools it builds are other people's work (see below).

| Tool | Version | What it is |
|---|---|---|
| [SDCC](https://sdcc.sourceforge.net/) | 4.6.0 (Homebrew) / 4.5.0 (apt) | C compiler for the Z80 |
| [RASM](https://github.com/EdouardBERGE/rasm) | 3.3.3 | Z80 assembler, writes AMSDOS files directly |
| [iDSK](https://github.com/cpcsdk/idsk) | 0.20 | Creates `.dsk` disc images |
| [CPCSyntaxError](https://github.com/WacKEDmaN/CPCSyntaxError) | 0.16.0 | Emulator (MIT) with M4 board: SD card = a Mac folder, WiFi = the Mac's network, `\|HTTPGET`/`\|HTTPMEM`, TCP sockets. Includes the CPC ROMs and Duke's M4 ROM |
| `lib/m4` | ours | C library for the M4: HTTP into memory, TCP client sockets, DNS |
| `lib/cpc` | ours | SDCC startup code (`crt0.s`), `printf` and keyboard through the CPC firmware |
| [fujinet-network-cpc](https://github.com/joergr72idb/fujinet-network-cpc) | 841f7c0, **optional** | FujiNet's fujinet-lib network API (`network_open`, `network_read`, ... TCP and HTTP) on the M4. GPL-3.0-or-later |

```
sdcc / rasm ──► build/*.BIN (AMSDOS header) ──► m4sd/ (the emulated M4's SD card) + build/samples.dsk
                                                  │
CPCSyntaxError (CPC 6128 + M4) ── M4 WiFi = the Mac's network ──► test server 127.0.0.1:8765 / internet
```

## Setup

```sh
tools/setup-macos.sh           # macOS: Homebrew packages + builds everything missing into opt/
tools/setup-linux.sh           # Linux (Debian/Ubuntu): checks the apt packages, then the same
tools/setup-linux.sh cpcse     # rebuild single parts: rasm idsk cpcse
tools/setup-linux.sh fujinet   # optional: fetch fujinet-network-cpc into src/ (also setup-macos.sh fujinet)
```

Linux needs these packages first (`setup-linux.sh` lists the missing ones and stops):
`sudo apt install build-essential git cmake ninja-build libsdl2-dev libgl-dev sdcc`

## Windows (WSL2) - not tested yet

The kit should run on Windows inside **WSL2** (a real Linux within Windows) with the Linux setup
above. Not tested yet - reports are welcome (GitHub issues).

1. In PowerShell as administrator: `wsl --install -d Ubuntu-26.04` (the kit is tested on Ubuntu
   26.04; older releases lack some packages), restart, create the Linux user.
2. In the Ubuntu window: clone the kit into the Linux home (`~/`), **not** under `/mnt/c/...` -
   that is slow and its file permissions break the build. Then follow the Linux steps above.
3. Emulator windows and sound come through WSLg (Windows 11, or Windows 10 with a current WSL:
   `wsl --update`). `make shot` needs no window.
4. Network: emulator, FujiNet-PC and internet work as on Linux. Connections *into* WSL from other
   machines on the LAN (e.g. a real computer to a server running in WSL) need mirrored networking -
   `%UserProfile%\.wslconfig`:
   ```
   [wsl2]
   networkingMode=mirrored
   ```
   then `wsl --shutdown`, and allow the port in the Windows firewall.

`tools/setup-linux.sh` recognizes WSL and prints these hints (it stops when the kit is under `/mnt/`).

## Use

```sh
make                           # samples -> build/ and the SD card m4sd/, plus build/samples.dsk
make www                       # test web server for the network samples (127.0.0.1:8765, samples/www)
make run                       # emulator window, CPC 6128 with M4 (starts the test server too)
make shot CMD='RUN"HELLOC"'    # run without a visible window, screenshot -> build/shot.png (FRAMES=600)
make shot CMD='RUN"HELLOC"' KEYS='wait 300; key SPACE; type PRINT 6*7\n'
make fastshot CMD='RUN"HELLOC"'   # the emulator's own headless mode: faster than real time, no KEYS,
                               # the sound of the run in build/shot.wav
make stop                      # stops the test web server
make fujinet                   # optional: build/FNHELLO.BIN with fujinet-network-cpc, onto the SD card
```

On the CPC (the M4's SD card is the current drive):

```
RUN"HELLOC"                              C: M4 version, HTTP download into memory, TCP socket
MEMORY &3FFF:LOAD"HELLOA":CALL &4000     assembler: prints a line, back to BASIC after a key
RUN"HELLO.BAS"                           BASIC: |NETSTAT and |HTTPMEM (the game clients' pattern)
RUN"FNHELLO"                             C with fujinet-network (after make fujinet): HTTP GET + TCP
RUN"DEMO"                                the demo (below); it runs until you reset the CPC
```

`RUN` of a binary resets the firmware: the CPC restarts when the program ends. To return to
BASIC, reserve the memory and `CALL` it: `MEMORY &3FFF:LOAD"NAME":CALL &4000`.

`make shot` is for checking a program without watching it (scripts, AI assistants): the emulator
runs in real time with a hidden window and its command API on `127.0.0.1:API_PORT` (default 6128),
`tools/cpcshot.py` types `CMD`, then `KEYS`, and after `FRAMES` frames (counted from `CMD`, 50 = 1 s)
saves `build/shot.png`. `KEYS`: commands separated by `;` - `wait N` (frames), `type TEXT` (`\n` =
ENTER), `key NAME` (RETURN SPACE ESC DEL COPY TAB UP DOWN LEFT RIGHT F0-F9, a letter or digit, or an
emulator key code such as `ControlLeft`), `joy DIR [N]` (up, down, left, right, fire: taps of the
cursor keys and Ctrl, which the emulator's joystick emulation turns into joystick 0). It needs a
desktop session (the window is hidden, not absent). `make fastshot` uses the emulator's own `--shot`
mode instead: no window at all, but as fast as it can, so a network program sees other timing.
It also records the sound of the run to `build/shot.wav` (the emulator's `--wav`; it only works in
this mode). E.g. `ffmpeg -i build/shot.wav -lavfi showspectrumpic=s=1200x500 spec.png` shows the
notes without listening.

## The demo (`samples/demo.c`, `demo_fx.s`, `demo_raster.py`)

A small demoscene-style screen: a big logo in the firmware font (doubled) with a rainbow running
through it, raster bars over the whole width including the border, an info panel, a scroller with a
rainbow, a 3-voice AY tune (melody, chord arpeggio, drums and bass sharing one voice) and the
current temperature in Celle, which the CPC fetches itself through the M4 (HTTP + JSON from
open-meteo.com - the M4 has no HTTPS, open-meteo answers plain HTTP too). It is the CPC version
of the Atari and C64 kits' demos, with the same tune. `make run`, then `RUN"DEMO"`.

![The demo in CPCSyntaxError: rainbow logo with the temperature, raster bars, info panel, scroller](docs/demo.png)

How it works: all colours change through the Gate Array on every scan line, from a table of 4 bytes
per line (pen, colour, pen, colour). The CPC has no bad lines, and every Z80 instruction takes a
whole number of microseconds, so `demo_raster.py` generates the raster routine as 201 unrolled
lines of exactly 64 µs, timing counted, not measured. The spare time on each line shifts the
scroller by a byte (LDI), so the frame's remaining time is enough for the music and the next
frame's colours. The routine starts from the Gate Array interrupt at line 294 (HALT, then a fixed
delay: `RASTER_DELAY` in the Makefile), which puts the colour writes into the horizontal blank.

An AI assistant (Claude Code) generated the demo from this prompt, after it had made the Atari and
C64 demos; no code was written by hand. It checked the demo in the kit's loops (`make shot`
screenshots, a fixed and a moving test pattern decoded line by line, the RAM dumped 100 frames
apart to confirm the full 50 frames per second, and a spectrogram of the `make fastshot` sound)
and fixed what it found there. One bug: a step of 3 instead of 2 in the routine that clears the bar
areas overwrote the table's pen bytes. The timing was right the first time.

> now build the same demo for the cpc toolchain

## The same demo on four machines

The kits for the Atari XL, the C64, the CPC and the Apple II each have this demo as a sample: the same
tune and the temperature in Celle, fetched live over the network - each made with its own machine's means.

| [Atari XL + FujiNet](https://github.com/joergr72idb/development-kit-atari-xl-and-fujinet) | [C64 + FujiNet](https://github.com/joergr72idb/development-kit-c64-and-fujinet) |
|:---:|:---:|
| ![The demo on the Atari XL](docs/demos/atari.png) | ![The demo on the C64](docs/demos/c64.png) |
| display list interrupts, fine scrolling with HSCROL, POKEY | letters as holes in characters with a rainbow behind them, $D016, SID |
| **[CPC + M4](https://github.com/joergr72idb/development-kit-cpc-and-m4)** | **[Apple II + FujiNet](https://github.com/joergr72idb/development-kit-apple2-and-fujinet)** |
| ![The demo on the CPC](docs/demos/cpc.png) | ![The demo on the Apple II](docs/demos/apple2.png) |
| Gate Array colours on every scan line, counted to the microsecond, AY | lo-res drawn again every frame into the hidden page, Mockingboard |

## The M4 library (`lib/m4/m4.h`)

```c
m4_init();                                         /* find the M4 ROM: 0 = ok */
n = m4_http_get("http://host:port/path", buf, max); /* like |HTTPMEM, up to 16 KB */
m4_dns("example.com", ip);                         /* ip = {a, b, c, d} */
s = m4_socket(); m4_connect(s, ip, port);          /* TCP client, sockets 1-4 */
m4_send(s, data, len); n = m4_recv(s, buf, max);   /* recv: 0 = nothing yet, -1 = closed */
m4_close(s);
```

It follows Duke's documented command interface ([m4info.txt](http://www.spinpoint.org/cpc/m4info.txt)):
command bytes to port `&FE00`, a write to `&FC00` starts the command, the answer is read from the
M4 ROM (pointer at `&FF02`, socket table at `&FF06`). The M4 keeps IP addresses last octet first;
the library takes and returns them in normal order.

## Optional: the FujiNet network API (`fujinet-network-cpc`)

For programs that should also run on the Atari, C64 or Apple II with a FujiNet:
[fujinet-network-cpc](https://github.com/joergr72idb/fujinet-network-cpc) offers the network calls of
FujiNet's fujinet-lib on top of `lib/m4`, so the same network code builds for all of them.

```c
network_init();
network_open("N1:HTTP://127.0.0.1:8765/hello.txt", OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
network_status("N1:", &waiting, &connected, &err);   /* err 136 = end of data */
n = network_read_nb("N1:", buf, waiting);
network_close("N1:");
```

`tools/setup-linux.sh fujinet` (or `setup-macos.sh fujinet`) clones it at the tested commit into
`src/fujinet-network-cpc`; `make fujinet` builds the sample `samples/fnhello.c`. In your own program:
compile `$(DEV)/src/fujinet-network-cpc/src/fujinet-network.c` with
`-I $(DEV)/src/fujinet-network-cpc/include -I $(DEV)/lib/m4` and link it with `m4.rel` (add
`-DFN_NO_HTTP` if you only need TCP: about 2.9 KB smaller). What works and what doesn't (no HTTPS,
UDP or listening yet) is in its README.

It is not part of the default setup because of its licence: the kit is MIT, the library is
**GPL-3.0-or-later** (as fujinet-lib, so one program can use both). A program linking it is GPL-3
too; `lib/m4` alone stays MIT.

## Notes

- **Test server vs. real hardware:** the samples fetch from `127.0.0.1:8765`, the Mac running the
  emulator. On a real CPC + M4, use the Mac's LAN address instead.
- **Keyboard:** the kit starts the emulator with the PC key layout (each Mac key types its character).
  Joystick emulation is on: the cursor keys, Ctrl and Alt are the joystick until you switch it off
  in the emulator (Input menu).
- The emulator keeps its settings in `build/cpcse-home` (`CPCSE_HOME`), never in the home folder.
- **Ports:** test server 127.0.0.1:8765 only. The Atari kit (8000, UDP 9997) and the C64 kit
  (8001, TCP 1541) can run at the same time.
- CPCSyntaxError's licence is MIT; its `roms/` are Amstrad's firmware (free for emulators) and
  Duke's M4 ROM (included with his permission). The other M4 C libraries found
  (M4sockslib, cpc-sdcc) have no licence, so `lib/m4` is written from the documentation instead.

## Programs built with this kit

- [Retro Game Battle](https://github.com/joergr72idb/retro-game-battle) - its CPC client uses
  fujinet-network-cpc and shares its protocol code with the Atari, C64 and Apple II clients

- [cpc-telnet](https://github.com/joergr72idb/cpc-telnet) - a telnet client for the CPC with the M4 board,
  made for the [Berlin Blues BBS](https://github.com/joergr72idb/atari-fujinet-berlin-blues-bbs): 40 or 80 columns, ANSI
  cursor sequences, writes the session to a text file on the SD card or a disc

## Built on the work of others

This kit is only glue: setup scripts that download and build other people's software at fixed
versions, plus Makefiles and settings that wire it together. Everything that does the real work
comes from these projects - used unmodified. **A warm thank you to all their maintainers and
contributors!**

- [SDCC](https://sdcc.sourceforge.net/) ([sources](https://sourceforge.net/projects/sdcc/)) - the C compiler for the Z80
- [RASM](https://github.com/EdouardBERGE/rasm) by Edouard Berge - the Z80 assembler
- [iDSK](https://github.com/cpcsdk/idsk) - CPC disc images
- [CPCSyntaxError](https://github.com/WacKEDmaN/CPCSyntaxError) - the emulator with M4 board emulation (sockets,
  `|HTTPGET`/`|HTTPMEM`, SD card); it brings Amstrad's CPC firmware (free for emulators) and Duke's M4 ROM
- the [M4 board](http://www.spinpoint.org/) by [Duke](https://github.com/M4Duke) - the hardware this kit is
  about; our `lib/m4` is written from his documented command interface
  ([m4info.txt](http://www.spinpoint.org/cpc/m4info.txt))

**Underneath:** [Homebrew](https://github.com/Homebrew/brew) or Debian/Ubuntu packages, [SDL](https://github.com/libsdl-org/SDL),
[CMake](https://github.com/Kitware/CMake) and [Ninja](https://github.com/ninja-build/ninja).

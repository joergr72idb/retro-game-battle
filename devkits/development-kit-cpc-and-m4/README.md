# Development Kit for the Amstrad/Schneider CPC and the M4 Board

A self-contained toolchain for macOS and Linux for CPC programs that use the M4 board's network, tested in an
emulator whose M4 reaches the internet through the computer's network. Everything is built into `opt/` inside this
folder; nothing is installed system-wide except a few Homebrew or apt packages.

| Tool | Version | What it is |
|---|---|---|
| [SDCC](https://sdcc.sourceforge.net/) | 4.6.0 (Homebrew) / 4.5.0 (apt) | C compiler for the Z80 |
| [RASM](https://github.com/EdouardBERGE/rasm) | 3.3.3 | Z80 assembler, writes AMSDOS files directly |
| [iDSK](https://github.com/cpcsdk/idsk) | 0.20 | Creates `.dsk` disc images |
| [CPCSyntaxError](https://github.com/WacKEDmaN/CPCSyntaxError) | 0.16.0 | Emulator (MIT) with M4 board: SD card = a Mac folder, WiFi = the Mac's network, `\|HTTPGET`/`\|HTTPMEM`, TCP sockets. Includes the CPC ROMs and Duke's M4 ROM |
| `lib/m4` | ours | C library for the M4: HTTP into memory, TCP client sockets, DNS |
| `lib/cpc` | ours | SDCC startup code (`crt0.s`), `printf` and keyboard through the CPC firmware |

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
```

Linux needs these packages first (`setup-linux.sh` lists the missing ones and stops):
`sudo apt install build-essential git cmake ninja-build libsdl2-dev libgl-dev sdcc`

## Use

```sh
make                           # samples -> build/ and the SD card m4sd/, plus build/samples.dsk
make www                       # test web server for the network samples (127.0.0.1:8765, samples/www)
make run                       # emulator window, CPC 6128 with M4 (starts the test server too)
make shot CMD='RUN"HELLOC"'    # headless run, screenshot -> build/shot.png (FRAMES=600 by default)
make stop                      # stops the test web server
```

On the CPC (the M4's SD card is the current drive):

```
RUN"HELLOC"                              C: M4 version, HTTP download into memory, TCP socket
MEMORY &3FFF:LOAD"HELLOA":CALL &4000     assembler: prints a line, back to BASIC after a key
RUN"HELLO.BAS"                           BASIC: |NETSTAT and |HTTPMEM (the game clients' pattern)
```

`RUN` of a binary resets the firmware: the CPC restarts when the program ends. To return to
BASIC, reserve the memory and `CALL` it: `MEMORY &3FFF:LOAD"NAME":CALL &4000`.

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

# Development kit for Atari XL + FujiNet: cc65 (C + ca65 assembler), FastBasic, fujinet-lib,
# atari800 with NetSIO and FujiNet-PC. All tools live in opt/
# (built by tools/setup-macos.sh or tools/setup-linux.sh).
#
#   make              build the samples into build/
#   make run          FujiNet-PC + atari800 with build/hello-c.xex (other program: make run XEX=path)
#   make run JOY=1    the same with joystick 0 on the cursor keys, fire = Space
#   make stop         stop this kit's FujiNet-PC
#   make shot         headless run (no window) of XEX for FRAMES frames (default 600 = 12 s PAL), keys and
#                     joystick from KEYS (tools/keyscript.py), last frame -> build/shot.png, e.g.
#                     make shot KEYS='wait 300; type HELLO; key RETURN; joy up+fire 20'
#   make run NETSIO_PORT=9996 WEB_PORT=8002   other ports, e.g. next to another FujiNet-PC + atari800
#
# Before starting, tools/check-ports refuses to run when the ports are in use: another atari800 on
# NETSIO_PORT (our FujiNet-PC would join it), another FujiNet-PC set to NETSIO_PORT (it would join
# our atari800 - two FujiNets on one bus, nothing boots) or another program on WEB_PORT.
#
# Other projects use the kit with  DEV ?= ../development-kit-atari-xl-and-fujinet  (see README).

DEV       := $(CURDIR)
CL65      := $(DEV)/opt/cc65/bin/cl65
FASTBASIC := $(DEV)/opt/fastbasic/fastbasic
FNLIB     := $(DEV)/opt/fujinet-lib
FNLIBA    := $(wildcard $(FNLIB)/fujinet-atari-*.lib)
ATARI800  := $(DEV)/opt/atari800/bin/atari800
FUJINET   := $(DEV)/opt/fujinet-pc
# GNU sed (Linux) and BSD sed (macOS) differ in -i
SED_I     := $(if $(filter Darwin,$(shell uname -s)),sed -i '',sed -i)
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

# -t atari, not atarixl: fujinet-lib calls the OS SIO vector, which atarixl switches out
CFLAGS    := -t atari -O -I $(FNLIB) -g
XEX       ?= build/hello-c.xex

.PHONY: all run fujinet stop clean shot

all: build/hello-c.xex build/hello-asm.xex build/hello-fb.xex

build/hello-c.xex: samples/hello.c | build
	$(CL65) $(CFLAGS) -m build/hello-c.map -Ln build/hello-c.lbl -o $@ $< $(FNLIBA)

build/hello-asm.xex: samples/hello.s | build
	$(CL65) -t atari -C atari-asm.cfg -Ln build/hello-asm.lbl -o $@ $<

build/hello-fb.xex: samples/hello.bas | build
	$(FASTBASIC) -o $@ $<

build:
	mkdir -p build/logs

# Ports - chosen so the Atari and the C64 kit can run side by side on one machine:
#   Atari kit: web UI 127.0.0.1:8000, NetSIO UDP 9997 | C64 kit: web UI 127.0.0.1:8001, NetIEC TCP 1541
WEB_PORT  ?= 8000
NETSIO_PORT ?= 9997

# FujiNet-PC: NetSIO (UDP) on NETSIO_PORT, web UI http://127.0.0.1:$(WEB_PORT) (local only), SD card = opt/fujinet-pc/SD
# The [BOIP] port in fnconfig.ini is set from NETSIO_PORT on every start.
fujinet: | build
	@tools/check-ports $(WEB_PORT) $(NETSIO_PORT) $(FUJINET)
	@$(SED_I) '/^\[BOIP\]/,/^\[/ s/^port=[0-9]*/port=$(NETSIO_PORT)/' $(FUJINET)/fnconfig.ini
	@pgrep -f "$(FUJINET)/[f]ujinet" >/dev/null || \
	  ( nohup $(DEV)/tools/run-fujinet -u http://127.0.0.1:$(WEB_PORT) >> $(DEV)/build/logs/fujinet.log 2>&1 & sleep 2 )

# Keyboard joystick: off by default (programs that read keys, like the BBS sysop console, need
# every key). JOY=1: joystick 0 = cursor keys, fire = Space (atari800.cfg SDL2_JOY_0_*).
JOY       ?= 0
KBDJOY    := $(if $(filter 1,$(JOY)),-kbdjoy0 -no-kbdjoy1,-no-kbdjoy0 -no-kbdjoy1)

run: $(XEX) fujinet
	$(ATARI800) -config atari800.cfg $(KBDJOY) -xl -netsio $(NETSIO_PORT) -run $(XEX)

# headless: no window or sound, input from a playback file (atari800 quits at its end), the screen
# recorded as video and its last frame saved. A new atari800 never links to a FujiNet-PC that is
# already running, so this kit's FujiNet-PC is restarted first (check-ports: not while an atari800
# of `make run` uses NETSIO_PORT) and stopped afterwards; its log stays in build/logs/fujinet.log.
FRAMES    ?= 600
KEYS      ?=
shot: $(XEX) | build
	@command -v ffmpeg >/dev/null || { echo "make shot needs ffmpeg: sudo apt install ffmpeg (macOS: brew install ffmpeg)"; exit 1; }
	@tools/check-ports $(WEB_PORT) $(NETSIO_PORT) $(FUJINET)
	python3 -I tools/keyscript.py $(FRAMES) '$(KEYS)' > build/shot.playback
	@$(MAKE) -s stop
	@$(MAKE) -s fujinet
	rm -f build/shot.avi build/shot.png
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(ATARI800) -config atari800.cfg -no-kbdjoy0 -no-kbdjoy1 -xl \
	  -netsio $(NETSIO_PORT) -playback build/shot.playback -record-video -vname build/shot.avi \
	  -run $(XEX) > build/logs/shot.log 2>&1 < /dev/null || true
	@$(MAKE) -s stop
	ffmpeg -loglevel error -sseof -0.2 -i build/shot.avi -update 1 -frames:v 1 -y build/shot.png
	@echo "screenshot: build/shot.png"

# only this kit's instance (matched by its path), never other projects' FujiNet-PC
# ([f]ujinet: the regex still matches the process, but not the recipe shell, whose command line holds the pattern)
stop:
	@pkill -f "$(DEV)/tools/[r]un-fujinet" || true
	@pkill -f "$(FUJINET)/[f]ujinet" || true

clean:
	rm -f build/*.xex build/*.map build/*.lbl build/*.o build/shot.*

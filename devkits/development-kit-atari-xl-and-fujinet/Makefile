# Development kit for Atari XL + FujiNet: cc65 (C + ca65 assembler), FastBasic, fujinet-lib,
# atari800 with NetSIO and FujiNet-PC. All tools live in opt/ (built by tools/setup.sh).
#
#   make              build the samples into build/
#   make run          FujiNet-PC + atari800 with build/hello-c.xex (other program: make run XEX=path)
#   make run JOY=1    the same with joystick 0 on the cursor keys, fire = Space
#   make stop         stop this kit's FujiNet-PC
#
# Other projects use the kit with  DEV ?= ../development-kit-atari-xl-and-fujinet  (see README).

DEV       := $(CURDIR)
CL65      := $(DEV)/opt/cc65/bin/cl65
FASTBASIC := $(DEV)/opt/fastbasic/fastbasic
FNLIB     := $(DEV)/opt/fujinet-lib
FNLIBA    := $(wildcard $(FNLIB)/fujinet-atari-*.lib)
ATARI800  := $(DEV)/opt/atari800/bin/atari800
FUJINET   := $(DEV)/opt/fujinet-pc
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

# -t atari, not atarixl: fujinet-lib calls the OS SIO vector, which atarixl switches out
CFLAGS    := -t atari -O -I $(FNLIB) -g
XEX       ?= build/hello-c.xex

.PHONY: all run fujinet stop clean

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
	@sed -i '' '/^\[BOIP\]/,/^\[/ s/^port=.*/port=$(NETSIO_PORT)/' $(FUJINET)/fnconfig.ini
	@pgrep -f "$(FUJINET)/fujinet" >/dev/null || \
	  ( nohup $(DEV)/tools/run-fujinet -u http://127.0.0.1:$(WEB_PORT) >> $(DEV)/build/logs/fujinet.log 2>&1 & sleep 2 )

# Keyboard joystick: off by default (programs that read keys, like the BBS sysop console, need
# every key). JOY=1: joystick 0 = cursor keys, fire = Space (atari800.cfg SDL2_JOY_0_*).
JOY       ?= 0
KBDJOY    := $(if $(filter 1,$(JOY)),-kbdjoy0 -no-kbdjoy1,-no-kbdjoy0 -no-kbdjoy1)

run: $(XEX) fujinet
	$(ATARI800) -config atari800.cfg $(KBDJOY) -xl -netsio $(NETSIO_PORT) -run $(XEX)

# only this kit's instance (matched by its path), never other projects' FujiNet-PC
stop:
	@pkill -f "$(DEV)/tools/run-fujinet" || true
	@pkill -f "$(FUJINET)/fujinet" || true

clean:
	rm -f build/*.xex build/*.map build/*.lbl build/*.o

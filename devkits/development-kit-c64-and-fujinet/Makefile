# Development kit for the C64 + FujiNet: cc65 (C + ca65 assembler), fujinet-lib, VICE (FujiNet fork
# with the NetIEC bridge, incl. petcat and c1541) and FujiNet-PC built for IEC (incl. Meatloaf drives).
# All tools live in opt/ (built by tools/setup-macos.sh or tools/setup-linux.sh).
#
#   make              build the samples into build/ (PRGs + build/samples.d64)
#   make run          FujiNet-PC + VICE with build/hello-c.prg (other program: make run PRG=path)
#   make config       FujiNet-PC + VICE with FujiNet as drive 8, for CONFIG.PRG
#   make stop         stop this kit's FujiNet-PC
#   make shot         headless run (no window) of PRG for FRAMES frames (default 600 = 12 s PAL), keys and
#                     joystick from KEYS (tools/c64shot.py), screen -> build/shot.png, text screen -> build/shot.txt
#                     e.g. make shot KEYS='wait 300; type hello; key RETURN; joy up+fire 20'
#   make run NETIEC_PORT=1542 WEB_PORT=8003   other ports, e.g. next to another FujiNet-PC for IEC
#
# Before starting, tools/check-ports refuses to run when the ports are in use: another program on
# WEB_PORT or NETIEC_PORT (e.g. another FujiNet-PC - our VICE would talk to it), and for run/config
# another VICE already connected to NETIEC_PORT (two C64s on one FujiNet-PC).
#
# FujiNet devices on the emulated bus: 16 network, 29 clock, 30 FujiNet. More with
#   make run UNITS="8 16 29 30"   (drive 8 from FujiNet-PC instead of VICE's own drive)

DEV       := $(CURDIR)
CL65      := $(DEV)/opt/cc65/bin/cl65
FNLIB     := $(DEV)/opt/fujinet-lib
FNLIBA    := $(wildcard $(FNLIB)/fujinet-c64-*.lib)
VICE      := $(DEV)/opt/vice/bin
BRIDGE    := $(DEV)/opt/vice-bridge
FUJINET   := $(DEV)/opt/fujinet-pc
# GNU sed (Linux) and BSD sed (macOS) differ in -i
SED_I     := $(if $(filter Darwin,$(shell uname -s)),sed -i '',sed -i)
# where VICE finds the bridge (libopencbm): DYLD_ on macOS, LD_ on Linux
LIBPATH   := $(if $(filter Darwin,$(shell uname -s)),DYLD_LIBRARY_PATH,LD_LIBRARY_PATH)
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

CFLAGS    := -t c64 -O -I $(FNLIB) -g
PRG       ?= build/hello-c.prg
UNITS     ?= 16 29 30

# units 8-11 replace VICE's own drives (no drive, OpenCBM backend), 12-30 become real devices
VICE_UNITS := $(foreach u,$(UNITS),$(if $(filter 8 9 10 11,$(u)),\
                -drive$(u)type 0 +drive$(u)truedrive -devicebackend$(u) 2 -trapdevice$(u),\
                -realdevice$(u)))

.PHONY: all run config fujinet stop clean shot

all: build/hello-c.prg build/hello-asm.prg build/hello-bas.prg build/samples.d64

build/hello-c.prg: samples/hello.c | build
	$(CL65) $(CFLAGS) -m build/hello-c.map -Ln build/hello-c.lbl -o $@ $< $(FNLIBA)

build/hello-asm.prg: samples/hello.s | build
	$(CL65) -t c64 -C c64-asm.cfg -u __EXEHDR__ -Ln build/hello-asm.lbl -o $@ $<

build/hello-bas.prg: samples/hello.bas | build
	$(VICE)/petcat -w2 -o $@ -- $<

build/samples.d64: build/hello-c.prg build/hello-asm.prg build/hello-bas.prg
	rm -f $@
	$(VICE)/c1541 -silent -format "samples,01" d64 $@ \
	  -write build/hello-c.prg hello-c -write build/hello-asm.prg hello-asm -write build/hello-bas.prg hello-bas

build:
	mkdir -p build/logs

# Ports - chosen so the Atari and the C64 kit can run side by side on one machine:
#   Atari kit: web UI 127.0.0.1:8000, NetSIO UDP 9997 | C64 kit: web UI 127.0.0.1:8001, NetIEC TCP 1541
WEB_PORT  ?= 8001
NETIEC_PORT ?= 1541

# FujiNet-PC: NetIEC (TCP) on NETIEC_PORT, web UI http://127.0.0.1:$(WEB_PORT) (local only), SD card = opt/fujinet-pc/SD
# The [BOIP] port in fnconfig.ini is set from NETIEC_PORT on every start.
fujinet: | build
	@tools/check-ports $(WEB_PORT) $(NETIEC_PORT) $(FUJINET)
	@$(SED_I) '/^\[BOIP\]/,/^\[/ s/^port=[0-9]*/port=$(NETIEC_PORT)/' $(FUJINET)/fnconfig.ini
	@pgrep -f "$(FUJINET)/[f]ujinet" >/dev/null || \
	  ( nohup $(DEV)/tools/run-fujinet -u http://127.0.0.1:$(WEB_PORT) >> $(DEV)/build/logs/fujinet.log 2>&1 & sleep 2 )

# -config/-logfile: VICE's settings and log live in build/, never in ~/.config or ~/.local
# keymap: German Mac keyboard, C64 "<-" on the § key (FujiNet CONFIG needs it to boot)
VICE_ARGS := -config $(DEV)/build/vicerc -logfile $(DEV)/build/logs/vice-run.log \
             -keymap 0 -symkeymap $(DEV)/keymaps/c64-mac-de.vkm

run: $(PRG) fujinet
	@tools/check-ports $(WEB_PORT) $(NETIEC_PORT) $(FUJINET) vice
	$(LIBPATH)=$(BRIDGE) NETIEC_PORT=$(NETIEC_PORT) $(VICE)/x64sc $(VICE_ARGS) $(VICE_UNITS) -autostart $(PRG)

# VICE with FujiNet as drive 8 and no program: for FujiNet's CONFIG (LOAD"CONFIG.PRG",8)
config: fujinet
	@tools/check-ports $(WEB_PORT) $(NETIEC_PORT) $(FUJINET) vice
	$(LIBPATH)=$(BRIDGE) NETIEC_PORT=$(NETIEC_PORT) $(VICE)/x64sc $(VICE_ARGS) \
	  $(foreach u,8 16 29 30,$(if $(filter 8,$(u)),-drive8type 0 +drive8truedrive -devicebackend8 2 -trapdevice8,-realdevice$(u)))

# headless: VICE with SDL's dummy video and sound, driven through its binary monitor on MON_PORT
# (127.0.0.1) by tools/c64shot.py: keys/joystick at the script's frames, then screen + text screen.
FRAMES    ?= 600
KEYS      ?=
MON_PORT  ?= 6520
shot: $(PRG) fujinet
	@tools/check-ports $(WEB_PORT) $(NETIEC_PORT) $(FUJINET) vice
	rm -f build/shot.png build/shot.txt
	$(LIBPATH)=$(BRIDGE) NETIEC_PORT=$(NETIEC_PORT) python3 -I tools/c64shot.py $(MON_PORT) $(FRAMES) '$(KEYS)' \
	  $(DEV)/build/shot -- $(VICE)/x64sc $(subst vice-run.log,vice-shot.log,$(VICE_ARGS)) $(VICE_UNITS) -autostart $(PRG)

# only this kit's instance (matched by its path), never other projects' FujiNet-PC
# ([f]ujinet: the regex still matches the process, but not the recipe shell, whose command line holds the pattern)
stop:
	@pkill -f "$(DEV)/tools/[r]un-fujinet" || true
	@pkill -f "$(FUJINET)/[f]ujinet" || true

clean:
	rm -f build/*.prg build/*.d64 build/*.map build/*.lbl build/*.o build/shot.*

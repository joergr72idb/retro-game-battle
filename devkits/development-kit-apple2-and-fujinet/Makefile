# Development kit for the Apple II + FujiNet: cc65 (C + ca65), fujinet-lib, cadius (ProDOS disk
# images), ProDOS 2.4.3 (boot disks) and FujiNet Go Apple II (AppleWin core + FujiNet built in).
# All tools live in opt/ (built by tools/setup-macos.sh or tools/setup-linux.sh).
#
#   make              the demo sample: build/demo.po (ProDOS boot disk with DEMO.SYSTEM)
#   make run DISK=path/to/disk.po   put a disk image on the emulated FujiNet's SD card, start the emulator
#   make run                        just start the emulator (it boots into FujiNet's CONFIG)
#   make shot DISK=disk.po KEYS='wait 300; type Hello; key RETURN'
#                                   headless run (no window): boots the disk straight away (no CONFIG),
#                                   types KEYS, after FRAMES frames (default 900 = 15 s) the screen ->
#                                   build/shot.png, the text screen -> build/shot.txt, FujiNet's log ->
#                                   build/shot.fujinet.log, the sound -> build/shot.wav. Without DISK it stays in CONFIG.
#
# In CONFIG: host SD -> the image -> slot 1 -> ESC boots it.
# Ports: 1985 (emulator <-> FujiNet, SmartPort over SLIP) and 64001 (FujiNet web UI), 127.0.0.1 only.

DEV     := $(CURDIR)
APP     := $(if $(filter Darwin,$(shell uname -s)),$(DEV)/opt/fngo/FujiNet Go Apple II.app/Contents/MacOS/FujiNet Go Apple II,$(DEV)/opt/fngo/fujinet-go-apple2-gnome)
# settings and data in build/, never in ~/.config or ~/.local/share
XDG_ENV := XDG_CONFIG_HOME=$(DEV)/build/xdg-config XDG_DATA_HOME=$(DEV)/build/xdg-data
SD      := $(DEV)/build/xdg-data/fujinet-go-apple2/fujinet/SD

.PHONY: all run shot clean

# ---- the demo sample: a ProDOS boot disk that starts DEMO.SYSTEM by itself (build/demo.po)
CL65    := $(DEV)/opt/cc65/bin/cl65
FNLIB   := $(DEV)/opt/fujinet-lib
FNLIBA  := $(wildcard $(FNLIB)/fujinet-apple2-*.lib)
CADIUS  := $(DEV)/opt/cadius/cadius
PRODOS  := $(DEV)/opt/prodos/PRODOS
BOOT    := $(DEV)/opt/prodos/bootblocks.bin
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

all: build/demo.po

build/DEMO.SYSTEM: samples/demo.c samples/demo_fx.s
	mkdir -p build
	$(CL65) -t apple2enh -O -I $(FNLIB) -C apple2enh-system.cfg -m build/demo.map -o $@ \
	  samples/demo.c samples/demo_fx.s $(FNLIBA)

# cadius takes type and load address from the name suffix (#FF2000 = SYS at $2000) and writes no
# boot blocks: they come from the ProDOS 2.4.3 disk. The image contains ProDOS: never commit it.
build/demo.po: build/DEMO.SYSTEM $(PRODOS) $(BOOT)
	rm -rf $@ build/disk
	mkdir -p build/disk
	cp $(PRODOS) build/disk/PRODOS#FF0000
	cp build/DEMO.SYSTEM build/disk/DEMO.SYSTEM#FF2000
	$(CADIUS) CREATEVOLUME $@ DEMO 140KB --quiet
	$(CADIUS) ADDFILE $@ /DEMO build/disk/PRODOS#FF0000 --quiet
	$(CADIUS) ADDFILE $@ /DEMO build/disk/DEMO.SYSTEM#FF2000 --quiet
	dd if=$(BOOT) of=$@ bs=1024 count=1 conv=notrunc 2>/dev/null

clean:
	rm -rf build/DEMO.SYSTEM build/demo.po build/demo.map build/disk build/shot.*

run:
	mkdir -p "$(SD)"
	$(if $(DISK),cp "$(DISK)" "$(SD)/")
	$(XDG_ENV) "$(APP)"

# ---- make shot: tools/a2shot.c on FujiNet Go's session library (built by the setup script in
# build/fngo), with its own settings and SD card in build/shot-config and build/shot-data, so `make
# run`'s CONFIG settings stay as they are. Ports 1985/64001 are fixed: not while `make run` is open.
FNGO_SRC  := $(DEV)/src/fujinet-go-apple2-desktop
FNGO_B    := $(DEV)/build/fngo
FNGO_LIBS := $(addprefix $(FNGO_B)/core/,libapple2session.a applewin-libretro/applewin_libretro.a \
             applewin-common2/libcommon2.a applewin-source/libappleii.a applewin-libyaml/libyaml.a \
             applewin-minizip/libminizip.a applewin-resource/libapple2roms.a applewin-libwindows/libwindows.a)
SDL3_LIBS ?= $(shell pkg-config --libs sdl3 2>/dev/null || echo -lSDL3)
FRAMES    ?= 900
KEYS      ?=
SHOT_DATA := $(DEV)/build/shot-data

build/a2shot: tools/a2shot.c $(FNGO_B)/core/libapple2session.a
	$(CXX) -O2 -x c -std=gnu11 -Wall -I $(FNGO_SRC)/core/include -c $< -o build/a2shot.o
	$(CXX) -o $@ build/a2shot.o $(FNGO_LIBS) -lz $(SDL3_LIBS) -lpthread -ldl

$(FNGO_B)/core/libapple2session.a:
	@echo "FujiNet Go's build is missing in build/fngo - run tools/setup-linux.sh fngo (or setup-macos.sh fngo)"; exit 1

shot: build/a2shot
	mkdir -p "$(SHOT_DATA)/fujinet/SD" build/logs
	$(if $(DISK),cp "$(DISK)" "$(SHOT_DATA)/fujinet/SD/")
	rm -f build/shot.png build/shot.txt build/shot.fujinet.log build/shot.wav
	build/a2shot $(DEV)/build/shot-config $(SHOT_DATA) $(FNGO_SRC)/tools/fujinet/work/out $(FRAMES) \
	  '$(KEYS)' $(DEV)/build/shot $(notdir $(DISK)) > build/logs/shot.log 2>&1 \
	  || { grep -a 'a2shot' build/logs/shot.log; exit 1; }
	@grep -a 'a2shot:' build/logs/shot.log | sed 's/^.*a2shot:/a2shot:/'
	@echo "screenshot: build/shot.png, text: build/shot.txt"

# Apple II client over raw TCP (FujiNet), C with cc65 + fujinet-lib - uses the Apple II development
# kit (../../../development-kit-apple2-and-fujinet: cc65, fujinet-lib >= 5.1.1, cadius, ProDOS).
#
#   make                              RGBTCP.SYSTEM + rgbtcp.po for the server 127.0.0.1 (the emulator)
#   make SERVER=192.168.17.138        for a real Apple II: the server's LAN address (the default;
#                                     the player can type another one at the start)
#
# rgbtcp.po is a ProDOS boot disk: ProDOS starts RGBTCP.SYSTEM by itself.

# the kit: devkits/development-kit-apple2-and-fujinet in this repo once its setup script has run,
# otherwise a checkout of the kit next to this repo
KIT     := development-kit-apple2-and-fujinet
DEV     ?= $(abspath $(if $(wildcard ../../devkits/$(KIT)/opt),../../devkits/$(KIT),../../../$(KIT)))
CL65    := $(DEV)/opt/cc65/bin/cl65
FNLIB   := $(DEV)/opt/fujinet-lib
FNLIBA  := $(wildcard $(FNLIB)/fujinet-apple2-*.lib)
CADIUS  := $(DEV)/opt/cadius/cadius
PRODOS  := $(DEV)/opt/prodos/PRODOS
BOOT    := $(DEV)/opt/prodos/bootblocks.bin
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

SERVER  ?= 127.0.0.1
PORT    ?= 6502
NAME    ?= APPLE2

# apple2enh: an Apple //e enhanced or later (lower case, arrow keys); -system: a ProDOS SYS file
SRC     := ../common/rgb_core.c platform.c

CFLAGS  := -t apple2enh -O -I $(FNLIB) -I . -I ../common \
           -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT='"$(PORT)"' -DPLAYER_NAME='"$(NAME)"'

.PHONY: all clean

all: rgbtcp.po

# the default server is compiled in: when it changes, delete what depends on it. Done while the
# Makefile is read, before make compares file times (macOS's make 3.81 only to the second).
SERVER_ID := $(SERVER) $(PORT) $(NAME)
ifneq ($(shell cat build/server.txt 2>/dev/null),$(SERVER_ID))
$(shell mkdir -p build && echo "$(SERVER_ID)" > build/server.txt && rm -f build/RGBTCP.SYSTEM rgbtcp.po)
endif

build/RGBTCP.SYSTEM: $(SRC) platform.h ../common/rgb_platform.h build/server.txt
	$(CL65) $(CFLAGS) -C apple2enh-system.cfg -m build/rgbtcp.map -o $@ $(SRC) $(FNLIBA)

# ProDOS boot disk: PRODOS first, then the first .SYSTEM file is started by itself.
# cadius takes the file type and load address from the name suffix (#FF2000 = SYS at $2000).
# cadius writes no boot blocks: they come from the ProDOS 2.4.3 disk (blocks 0-1, 1024 bytes).
rgbtcp.po: build/RGBTCP.SYSTEM $(PRODOS) $(BOOT)
	rm -rf $@ build/disk
	mkdir -p build/disk
	cp $(PRODOS) build/disk/PRODOS#FF0000
	cp build/RGBTCP.SYSTEM build/disk/RGBTCP.SYSTEM#FF2000
	$(CADIUS) CREATEVOLUME $@ RGB 140KB --quiet
	$(CADIUS) ADDFILE $@ /RGB build/disk/PRODOS#FF0000 --quiet
	$(CADIUS) ADDFILE $@ /RGB build/disk/RGBTCP.SYSTEM#FF2000 --quiet
	dd if=$(BOOT) of=$@ bs=1024 count=1 conv=notrunc 2>/dev/null

clean:
	rm -rf build rgbtcp.po

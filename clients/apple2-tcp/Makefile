# Apple II client over raw TCP (FujiNet), C with cc65 + fujinet-lib - uses the Apple II development
# kit (../../../development-kit-apple2-and-fujinet: cc65, fujinet-lib >= 5.1.1, cadius, ProDOS).
#
#   make                              RGBTCP.SYSTEM + rgbtcp.po for the server 127.0.0.1 (the emulator)
#   make SERVER=192.168.17.138        for a real Apple II: the server's LAN address (compiled in)
#
# rgbtcp.po is a ProDOS boot disk: ProDOS starts RGBTCP.SYSTEM by itself.

DEV     ?= $(abspath ../../../development-kit-apple2-and-fujinet)
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
CFLAGS  := -t apple2enh -O -I $(FNLIB) \
           -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT='"$(PORT)"' -DPLAYER_NAME='"$(NAME)"'

.PHONY: all clean FORCE

all: rgbtcp.po

# the server address is compiled in: rebuild when it changes
build/server.txt: FORCE
	@mkdir -p build
	@echo "$(SERVER) $(PORT) $(NAME)" | cmp -s - $@ || echo "$(SERVER) $(PORT) $(NAME)" > $@

build/RGBTCP.SYSTEM: rgb_tcp.c build/server.txt
	$(CL65) $(CFLAGS) -C apple2enh-system.cfg -m build/rgbtcp.map -o $@ rgb_tcp.c $(FNLIBA)

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

# Atari XL/XE client over raw TCP (FujiNet), C with cc65 + fujinet-lib - uses the Atari development
# kit (../../../development-kit-atari-xl-and-fujinet: cc65, fujinet-lib, atari800, FujiNet-PC).
#
#   make                              rgbtcp.xex for the server 127.0.0.1 (the emulator on this Mac)
#   make SERVER=192.168.17.138        for a real Atari: the server's LAN address (the default;
#                                     the player can type another one at the start)
#   make run                          the kit's FujiNet-PC boots rgbtcp.xex, atari800 (joystick = cursor keys + Space)
#   make config                       the kit's FujiNet-PC back to its CONFIG menu at boot
#
# The FujiNet boots rgbtcp.xex directly from its SD card (mount it like a disk image).

# the kit: devkits/development-kit-atari-xl-and-fujinet in this repo once its tools/setup.sh has run,
# otherwise a checkout of the kit next to this repo
KIT     := development-kit-atari-xl-and-fujinet
DEV     ?= $(abspath $(if $(wildcard ../../devkits/$(KIT)/opt),../../devkits/$(KIT),../../../$(KIT)))
CL65    := $(DEV)/opt/cc65/bin/cl65
FNLIB   := $(DEV)/opt/fujinet-lib
FNLIBA  := $(wildcard $(FNLIB)/fujinet-atari-*.lib)
FUJINET := $(DEV)/opt/fujinet-pc
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

SERVER  ?= 127.0.0.1
PORT    ?= 6502
NAME    ?= ATARIXL

# -t atari, not atarixl: fujinet-lib calls the OS SIO vector, which the atarixl runtime switches out
CFLAGS  := -t atari -O -I $(FNLIB) \
           -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT='"$(PORT)"' -DPLAYER_NAME='"$(NAME)"'

.PHONY: all run config clean

all: rgbtcp.xex

# the default server is compiled in: when it changes, delete what depends on it. Done while the
# Makefile is read, before make compares file times (macOS's make 3.81 only to the second).
SERVER_ID := $(SERVER) $(PORT) $(NAME)
ifneq ($(shell cat build/server.txt 2>/dev/null),$(SERVER_ID))
$(shell mkdir -p build && echo "$(SERVER_ID)" > build/server.txt && rm -f rgbtcp.xex)
endif

rgbtcp.xex: rgb_tcp.c build/server.txt
	$(CL65) $(CFLAGS) -m build/rgbtcp.map -o $@ rgb_tcp.c $(FNLIBA)

# Like a real Atari: the FujiNet boots the XEX itself (no `atari800 -run`: with a disk mounted in
# D1: the Atari boots that first, and a DOS/N: handler there clashes with the program).
# FujiNet-PC is restarted first, then atari800 - in that order.
run: rgbtcp.xex
	-pkill -f "$(DEV)/opt/atari800/bin/atari800"
	$(MAKE) -C $(DEV) stop
	cp rgbtcp.xex $(FUJINET)/SD/rgbtcp.xex
	python3 -I fn_boot.py $(FUJINET)/fnconfig.ini /rgbtcp.xex
	$(MAKE) -C $(DEV) fujinet
	cd $(DEV) && $(DEV)/opt/atari800/bin/atari800 -config atari800.cfg -kbdjoy0 -no-kbdjoy1 -xl -netsio 9997

config:
	$(MAKE) -C $(DEV) stop
	python3 -I fn_boot.py $(FUJINET)/fnconfig.ini --off

clean:
	rm -rf build rgbtcp.xex

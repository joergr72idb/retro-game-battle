# C64 client over raw TCP (FujiNet), C with cc65 + fujinet-lib - uses the C64 development kit
# (../../../development-kit-c64-and-fujinet: cc65, fujinet-lib >= 5.1.1, VICE with the FujiNet
# bridge, c1541).
#
#   make                              rgbtcp.prg + rgbtcp.d64 for the server 127.0.0.1 (VICE on this Mac)
#   make SERVER=192.168.17.138        for a real C64: the server's LAN address (the default;
#                                     the player can type another one at the start)
#   make run                          VICE + FujiNet-PC of the kit with rgbtcp.prg
#
# On the C64: LOAD"RGBTCP",8 and RUN (or autostart in VICE).

# the kit: devkits/development-kit-c64-and-fujinet in this repo once its setup script has run,
# otherwise a checkout of the kit next to this repo
KIT     := development-kit-c64-and-fujinet
DEV     ?= $(abspath $(if $(wildcard ../../devkits/$(KIT)/opt),../../devkits/$(KIT),../../../$(KIT)))
CL65    := $(DEV)/opt/cc65/bin/cl65
FNLIB   := $(DEV)/opt/fujinet-lib
FNLIBA  := $(wildcard $(FNLIB)/fujinet-c64-*.lib)
C1541   := $(DEV)/opt/vice/bin/c1541
export CC65_HOME := $(DEV)/opt/cc65/share/cc65

SERVER  ?= 127.0.0.1
PORT    ?= 6502
NAME    ?= C64FUJINET

CFLAGS  := -t c64 -O -I $(FNLIB) \
           -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT='"$(PORT)"' -DPLAYER_NAME='"$(NAME)"'

.PHONY: all run clean

all: rgbtcp.prg rgbtcp.d64

# the default server is compiled in: when it changes, delete what depends on it. Done while the
# Makefile is read, before make compares file times (macOS's make 3.81 only to the second).
SERVER_ID := $(SERVER) $(PORT) $(NAME)
ifneq ($(shell cat build/server.txt 2>/dev/null),$(SERVER_ID))
$(shell mkdir -p build && echo "$(SERVER_ID)" > build/server.txt && rm -f rgbtcp.prg rgbtcp.d64)
endif

rgbtcp.prg: rgb_tcp.c build/server.txt
	$(CL65) $(CFLAGS) -m build/rgbtcp.map -o $@ rgb_tcp.c $(FNLIBA)

rgbtcp.d64: rgbtcp.prg
	rm -f $@
	$(C1541) -silent -format "rgb tcp,01" d64 $@ -write rgbtcp.prg rgbtcp > /dev/null 2>&1

run: rgbtcp.prg
	$(MAKE) -C $(DEV) run PRG=$(CURDIR)/rgbtcp.prg

clean:
	rm -rf build rgbtcp.prg rgbtcp.d64 rgb_tcp.o

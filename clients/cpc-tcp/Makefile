# Schneider CPC client over raw TCP (M4 board), C with SDCC - uses the CPC development kit
# (../../../development-kit-cpc-and-m4: SDCC, lib/cpc, lib/m4, tools/cpcbin.py, iDSK).
#
#   make                              RGBTCP.BIN for the server 127.0.0.1 (the emulator on this Mac)
#   make SERVER=192.168.17.138        for a real CPC: the server's LAN address (the default;
#                                     the player can type another one at the start)
#   make sd                           copy RGBTCP.BIN onto the kit's emulated M4 SD card
#
# On the CPC: RUN"RGBTCP"

# the kit: devkits/development-kit-cpc-and-m4 in this repo once its setup script has run,
# otherwise a checkout of the kit next to this repo
KIT     := development-kit-cpc-and-m4
DEV     ?= $(abspath $(if $(wildcard ../../devkits/$(KIT)/opt),../../devkits/$(KIT),../../../$(KIT)))
# SDCC: Homebrew's on macOS, the one on PATH (apt) on Linux - as in the kit
SDCC    := $(if $(filter Darwin,$(shell uname -s)),$(shell brew --prefix sdcc 2>/dev/null)/bin/sdcc,$(shell command -v sdcc 2>/dev/null))
SDASZ80 := $(dir $(SDCC))sdasz80
IDSK    := $(DEV)/opt/idsk/iDSK

SERVER  ?= 127.0.0.1
PORT    ?= 6502
NAME    ?= SCHNEIDERCPC

CFLAGS  := -mz80 --opt-code-size -I $(DEV)/lib/cpc -I $(DEV)/lib/m4 \
           -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT=$(PORT) -DPLAYER_NAME='"$(NAME)"'
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0
OBJ     := build

.PHONY: all sd clean

all: RGBTCP.BIN rgbtcp.dsk

# the default server is compiled in: when it changes, delete what depends on it. Done while the
# Makefile is read, before make compares file times (macOS's make 3.81 only to the second).
SERVER_ID := $(SERVER) $(PORT) $(NAME)
ifneq ($(shell cat $(OBJ)/server.txt 2>/dev/null),$(SERVER_ID))
$(shell mkdir -p $(OBJ) && echo "$(SERVER_ID)" > $(OBJ)/server.txt && rm -f $(OBJ)/rgb_tcp.rel RGBTCP.BIN rgbtcp.dsk)
endif

$(OBJ)/crt0.rel: $(DEV)/lib/cpc/crt0.s | $(OBJ)
	$(SDASZ80) -o $@ $<

$(OBJ)/cpc.rel: $(DEV)/lib/cpc/cpc.c | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/m4.rel: $(DEV)/lib/m4/m4.c | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/rgb_tcp.rel: rgb_tcp.c $(OBJ)/server.txt | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

# crt0 first: its JP at &4000 is the entry
RGBTCP.BIN: $(OBJ)/crt0.rel $(OBJ)/rgb_tcp.rel $(OBJ)/cpc.rel $(OBJ)/m4.rel
	$(SDCC) $(LDFLAGS) -o $(OBJ)/rgb_tcp.ihx $^
	python3 -I $(DEV)/tools/cpcbin.py $(OBJ)/rgb_tcp.ihx $@

rgbtcp.dsk: RGBTCP.BIN
	rm -f $@
	$(IDSK) $@ -n > /dev/null
	$(IDSK) $@ -i RGBTCP.BIN > /dev/null

sd: RGBTCP.BIN
	cp RGBTCP.BIN $(DEV)/m4sd/

$(OBJ):
	mkdir -p $@

clean:
	rm -rf $(OBJ) RGBTCP.BIN rgbtcp.dsk

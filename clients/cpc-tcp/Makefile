# Schneider CPC client over raw TCP (M4 board), C with SDCC - uses the CPC development kit
# (../../../development-kit-cpc-and-m4: SDCC, lib/cpc, lib/m4, tools/cpcbin.py, iDSK) and
# fujinet-network-cpc (../../lib/fujinet-network-cpc, a git subtree: fujinet-lib's network calls on
# the M4; `make FNCPC=/path` for another checkout).
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
# fujinet-network-cpc: the copy in this repo (git subtree)
FNCPC   ?= $(abspath ../../lib/fujinet-network-cpc)
SDCC    := $(if $(filter Darwin,$(shell uname -s)),$(shell brew --prefix sdcc 2>/dev/null)/bin/sdcc,$(shell command -v sdcc 2>/dev/null))
SDASZ80 := $(dir $(SDCC))sdasz80
IDSK    := $(DEV)/opt/idsk/iDSK

SERVER  ?= 127.0.0.1
PORT    ?= 6502
NAME    ?= SCHNEIDERCPC

# -DFN_NO_HTTP: the client only needs TCP - fujinet-network-cpc without HTTP (2.9 KB smaller)
CFLAGS  := -mz80 --opt-code-size -I $(DEV)/lib/cpc -I $(DEV)/lib/m4 -I $(FNCPC)/include \
           -I . -I ../common -DFN_NO_HTTP \
           -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT='"$(PORT)"' -DPLAYER_NAME='"$(NAME)"'
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0
OBJ     := build

.PHONY: all sd clean

all: RGBTCP.BIN rgbtcp.dsk

# the default server is compiled in: when it changes, delete what depends on it. Done while the
# Makefile is read, before make compares file times (macOS's make 3.81 only to the second).
SERVER_ID := $(SERVER) $(PORT) $(NAME)
ifneq ($(shell cat $(OBJ)/server.txt 2>/dev/null),$(SERVER_ID))
$(shell mkdir -p $(OBJ) && echo "$(SERVER_ID)" > $(OBJ)/server.txt && rm -f $(OBJ)/rgb_core.rel RGBTCP.BIN rgbtcp.dsk)
endif

$(OBJ)/crt0.rel: $(DEV)/lib/cpc/crt0.s | $(OBJ)
	$(SDASZ80) -o $@ $<

$(OBJ)/cpc.rel: $(DEV)/lib/cpc/cpc.c | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/m4.rel: $(DEV)/lib/m4/m4.c | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/fujinet-network.rel: $(FNCPC)/src/fujinet-network.c $(FNCPC)/include/fujinet-network.h Makefile | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

HDR     := platform.h ../common/rgb_platform.h $(FNCPC)/include/fujinet-network.h

$(OBJ)/rgb_core.rel: ../common/rgb_core.c $(HDR) $(OBJ)/server.txt | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/platform.rel: platform.c $(HDR) | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

# crt0 first: its JP at &4000 is the entry
RGBTCP.BIN: $(OBJ)/crt0.rel $(OBJ)/rgb_core.rel $(OBJ)/platform.rel $(OBJ)/fujinet-network.rel $(OBJ)/cpc.rel $(OBJ)/m4.rel
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

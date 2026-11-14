# Schneider CPC client over raw TCP (M4 board), C with SDCC - uses the CPC development kit
# (../../../development-kit-cpc-and-m4: SDCC, lib/cpc, lib/m4, tools/cpcbin.py, iDSK).
#
#   make                              RGBTCP.BIN for the server 127.0.0.1 (the emulator on this Mac)
#   make SERVER=192.168.17.138        for a real CPC: the server's LAN address
#   make sd                           copy RGBTCP.BIN onto the kit's emulated M4 SD card
#
# On the CPC: RUN"RGBTCP"

# the kit: devkits/development-kit-cpc-and-m4 in this repo once its tools/setup.sh has run,
# otherwise a checkout of the kit next to this repo
KIT     := development-kit-cpc-and-m4
DEV     ?= $(abspath $(if $(wildcard ../../devkits/$(KIT)/opt),../../devkits/$(KIT),../../../$(KIT)))
SDCC    := $(shell brew --prefix sdcc 2>/dev/null)/bin/sdcc
SDASZ80 := $(dir $(SDCC))sdasz80
IDSK    := $(DEV)/opt/idsk/iDSK

SERVER  ?= 127.0.0.1
PORT    ?= 6502
NAME    ?= SCHNEIDERCPC

comma   := ,
CFLAGS  := -mz80 --opt-code-size -I $(DEV)/lib/cpc -I $(DEV)/lib/m4 \
           -DSERVER_IP='$(subst .,$(comma),$(SERVER))' -DSERVER_PORT=$(PORT) -DPLAYER_NAME='"$(NAME)"'
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0
OBJ     := build

.PHONY: all sd clean

all: RGBTCP.BIN rgbtcp.dsk

# the server address is compiled in: rebuild when it changes
$(OBJ)/server.txt: FORCE | $(OBJ)
	@echo "$(SERVER) $(PORT) $(NAME)" | cmp -s - $@ || echo "$(SERVER) $(PORT) $(NAME)" > $@
FORCE:

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

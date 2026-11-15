# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 joergr72idb - see LICENSE
# fujinet-network for the CPC + M4 (SDCC): the network part of FujiNet's fujinet-lib on the M4.
# Uses the CPC development kit (../development-kit-cpc-and-m4: SDCC, lib/cpc, lib/m4, iDSK,
# CPCSyntaxError); `make DEV=/path/to/kit` for another place.
#
#   make                  build/fujinet-network.rel + build/m4.rel (link both into your program)
#   make test             builds FNTEST.BIN, starts test/testserver.py and runs FNTEST headless
#                         in the kit's emulator; prints PASS or FAIL
#   make fntest           only FNTEST.BIN + fntest.dsk (SERVER=, PORT= for a real CPC)
#   NO_HTTP=1             with any of them: without HTTP (-DFN_NO_HTTP, about 3 KB smaller)
#
# Your program: -I include -I $(DEV)/lib/m4, link build/fujinet-network.rel build/m4.rel.

DEV     ?= $(abspath ../development-kit-cpc-and-m4)
# SDCC: Homebrew's on macOS, the one on PATH (apt) on Linux - as in the kit
SDCC    := $(if $(filter Darwin,$(shell uname -s)),$(shell brew --prefix sdcc 2>/dev/null)/bin/sdcc,$(shell command -v sdcc 2>/dev/null))
SDASZ80 := $(dir $(SDCC))sdasz80
IDSK    := $(DEV)/opt/idsk/iDSK
CPCSE   := $(DEV)/opt/cpcse

SERVER  ?= 127.0.0.1
PORT    ?= 6510
FRAMES  ?= 5000
NO_HTTP ?=

CFLAGS  := -mz80 --opt-code-size -I include -I $(DEV)/lib/cpc -I $(DEV)/lib/m4 $(if $(NO_HTTP),-DFN_NO_HTTP)
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0
OBJ     := build

.PHONY: all lib fntest test clean

all: lib
lib: $(OBJ)/fujinet-network.rel $(OBJ)/m4.rel

$(OBJ)/fujinet-network.rel: src/fujinet-network.c include/fujinet-network.h $(OBJ)/fntest.id | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/m4.rel: $(DEV)/lib/m4/m4.c | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/cpc.rel: $(DEV)/lib/cpc/cpc.c | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/crt0.rel: $(DEV)/lib/cpc/crt0.s | $(OBJ)
	$(SDASZ80) -o $@ $<

# the server and NO_HTTP are compiled in: build/fntest.id changes (and the objects are rebuilt)
# when SERVER/PORT/NO_HTTP do
TEST_ID := $(SERVER) $(PORT) $(NO_HTTP)
ifneq ($(shell cat $(OBJ)/fntest.id 2>/dev/null),$(TEST_ID))
$(shell mkdir -p $(OBJ) && echo "$(TEST_ID)" > $(OBJ)/fntest.id)
endif

$(OBJ)/fntest.rel: test/fntest.c include/fujinet-network.h $(OBJ)/fntest.id | $(OBJ)
	$(SDCC) $(CFLAGS) -DSERVER_HOST='"$(SERVER)"' -DSERVER_PORT=$(PORT) -c -o $@ $<

# crt0 first: its JP at &4000 is the entry
FNTEST.BIN: $(OBJ)/crt0.rel $(OBJ)/fntest.rel $(OBJ)/fujinet-network.rel $(OBJ)/m4.rel $(OBJ)/cpc.rel
	$(SDCC) $(LDFLAGS) -o $(OBJ)/fntest.ihx $^
	python3 -I $(DEV)/tools/cpcbin.py $(OBJ)/fntest.ihx $@

fntest.dsk: FNTEST.BIN
	rm -f $@
	$(IDSK) $@ -n > /dev/null
	$(IDSK) $@ -i FNTEST.BIN > /dev/null

fntest: FNTEST.BIN fntest.dsk

# headless: the test server in the background, FNTEST in the emulator, the server's verdict
test: FNTEST.BIN
	mkdir -p $(DEV)/m4sd && cp FNTEST.BIN $(DEV)/m4sd/
	$(MAKE) -C $(DEV) build/cpcse-home/cpcse.ini
	python3 -I test/testserver.py $(PORT) 180 > $(OBJ)/server.log 2>&1 & \
	  sleep 1; \
	  ( cd $(CPCSE) && CPCSE_HOME=$(DEV)/build/cpcse-home ./cpcse --shot $(CURDIR)/$(OBJ)/shot.bmp \
	    --model cpc6128 --m4 $(DEV)/m4sd --type 'RUN"FNTEST"\n' --frames $(FRAMES) \
	    > $(CURDIR)/$(OBJ)/emu.log 2>&1 ); \
	  wait $$!; rc=$$?; \
	  $(if $(filter Darwin,$(shell uname -s)),sips -s format png $(OBJ)/shot.bmp --out $(OBJ)/shot.png > /dev/null,python3 -I $(DEV)/tools/bmp2png.py $(OBJ)/shot.bmp $(OBJ)/shot.png); \
	  cat $(OBJ)/server.log; echo "screenshot: $(OBJ)/shot.png"; exit $$rc

$(OBJ):
	mkdir -p $@

clean:
	rm -rf $(OBJ) FNTEST.BIN fntest.dsk

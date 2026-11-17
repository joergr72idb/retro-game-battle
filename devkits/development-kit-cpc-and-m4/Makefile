# Development kit for the Amstrad/Schneider CPC + M4 board: SDCC (C), RASM (assembler), iDSK (disc
# images), CPCSyntaxError (emulator with M4 incl. WiFi through the Mac's network) and lib/m4 (our
# own M4 library: HTTP into memory, TCP sockets, DNS). Tools in opt/
# (built by tools/setup-macos.sh or tools/setup-linux.sh).
#
#   make              build the samples: build/HELLOC.BIN, HELLOA.BIN, HELLO.BAS, DEMO.BIN + build/samples.dsk,
#                     and copy them onto the emulated M4's SD card (m4sd/)
#   make www          local test web server for the network samples (127.0.0.1:8765, samples/www)
#   make run          the emulator in a window, M4 on, SD card = m4sd/ (then: RUN"HELLOC")
#   make shot CMD='RUN"HELLOC"' KEYS='wait 100; key SPACE'   run without a visible window in real time,
#                     keys from KEYS (tools/cpcshot.py, the emulator's command API on API_PORT), FRAMES frames
#                     after CMD (default 600 = 12 s), screenshot in build/shot.png (for tests)
#   make fastshot CMD='RUN"HELLOC"'   the emulator's own headless mode: as fast as it can (not real
#                     time - network timing differs), no KEYS, screenshot in build/shot.png, sound in build/shot.wav
#   make stop         stops the test web server
#   make fujinet      optional: build/FNHELLO.BIN with fujinet-network-cpc (FujiNet's fujinet-lib
#                     network API on the M4; fetch it first: tools/setup-linux.sh|setup-macos.sh fujinet)

DEV       := $(CURDIR)
UNAME     := $(shell uname -s)
# SDCC: Homebrew's on macOS, the one on PATH (apt) on Linux
SDCC      := $(if $(filter Darwin,$(UNAME)),$(shell brew --prefix sdcc 2>/dev/null)/bin/sdcc,$(shell command -v sdcc 2>/dev/null))
SDASZ80   := $(dir $(SDCC))sdasz80
RASM      := $(DEV)/opt/rasm/rasm
IDSK      := $(DEV)/opt/idsk/iDSK
CPCSE     := $(DEV)/opt/cpcse
M4SD      := $(DEV)/m4sd
WWW_PORT  ?= 8765
FRAMES    ?= 600
CMD       ?= RUN"HELLOC"
# fujinet-network-cpc (optional, GPL-3.0-or-later): fetched by the setup script's "fujinet" part,
# or point it at your own copy
FNCPC     ?= $(DEV)/src/fujinet-network-cpc

CFLAGS    := -mz80 -I lib/cpc -I lib/m4 --opt-code-size
# crt0 starts at &4000 (a JP to init), the code right after it
LDFLAGS   := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0

# the emulator: settings in build/cpcse-home (never next to the program or in the home folder)
CPCSE_ENV := CPCSE_HOME=$(DEV)/build/cpcse-home

OBJ       := build/obj

.PHONY: all sd www run shot fastshot stop clean fujinet fncheck

all: build/HELLOC.BIN build/HELLOA.BIN build/HELLO.BAS build/DEMO.BIN build/samples.dsk sd

$(OBJ)/crt0.rel: lib/cpc/crt0.s | $(OBJ)
	$(SDASZ80) -o $@ $<

$(OBJ)/%.rel: lib/cpc/%.c lib/cpc/cpc.h | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/%.rel: lib/m4/%.c lib/m4/m4.h | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/hello.rel: samples/hello.c lib/cpc/cpc.h lib/m4/m4.h | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

# crt0 has to come first: its JP is the program's entry at &4000
build/HELLOC.BIN: $(OBJ)/crt0.rel $(OBJ)/hello.rel $(OBJ)/cpc.rel $(OBJ)/m4.rel
	$(SDCC) $(LDFLAGS) -o build/hello.ihx $^
	python3 -I tools/cpcbin.py build/hello.ihx $@

# the demo: C (network, screen), demo_fx.s (interrupts, colours, music) and the raster routine,
# generated with its timing counted in microseconds (RASTER_DELAY: from the interrupt to line 0)
RASTER_DELAY ?= 1010
build/demo_raster.s: samples/demo_raster.py Makefile | $(OBJ)
	python3 -I $< $(RASTER_DELAY) > $@

$(OBJ)/demo_raster.rel: build/demo_raster.s
	$(SDASZ80) -o $@ $<

$(OBJ)/demo_fx.rel: samples/demo_fx.s | $(OBJ)
	$(SDASZ80) -o $@ $<

$(OBJ)/demo.rel: samples/demo.c lib/cpc/cpc.h lib/m4/m4.h | $(OBJ)
	$(SDCC) $(CFLAGS) -c -o $@ $<

build/DEMO.BIN: $(OBJ)/crt0.rel $(OBJ)/demo.rel $(OBJ)/demo_fx.rel $(OBJ)/demo_raster.rel $(OBJ)/cpc.rel $(OBJ)/m4.rel
	$(SDCC) $(LDFLAGS) -o build/demo.ihx $^
	python3 -I tools/cpcbin.py build/demo.ihx $@

# optional: the fujinet-network sample, compiled with the library's sources
FN_CFLAGS := $(CFLAGS) -I $(FNCPC)/include

fncheck:
	@[ -f $(FNCPC)/src/fujinet-network.c ] || { echo "fujinet-network-cpc not found in $(FNCPC):"; \
	  echo "  run tools/setup-linux.sh fujinet (or tools/setup-macos.sh fujinet)"; exit 1; }

$(OBJ)/fujinet-network.rel: $(FNCPC)/src/fujinet-network.c $(FNCPC)/include/fujinet-network.h | $(OBJ)
	$(SDCC) $(FN_CFLAGS) -c -o $@ $<

$(OBJ)/fnhello.rel: samples/fnhello.c $(FNCPC)/include/fujinet-network.h lib/cpc/cpc.h | $(OBJ)
	$(SDCC) $(FN_CFLAGS) -c -o $@ $<

build/FNHELLO.BIN: $(OBJ)/crt0.rel $(OBJ)/fnhello.rel $(OBJ)/fujinet-network.rel $(OBJ)/m4.rel $(OBJ)/cpc.rel
	$(SDCC) $(LDFLAGS) -o build/fnhello.ihx $^
	python3 -I tools/cpcbin.py build/fnhello.ihx $@

fujinet: fncheck build/FNHELLO.BIN
	mkdir -p $(M4SD)
	cp build/FNHELLO.BIN $(M4SD)/

build/HELLOA.BIN: samples/hello.asm | $(OBJ)
	cd build && $(RASM) -o hello ../$< > logs/rasm.log 2>&1 || { cat logs/rasm.log; exit 1; }

# ASCII BASIC: LOAD/RUN on the CPC tokenizes it
build/HELLO.BAS: samples/hello.bas | $(OBJ)
	cp $< $@

build/samples.dsk: build/HELLOC.BIN build/HELLOA.BIN build/HELLO.BAS build/DEMO.BIN
	rm -f $@
	$(IDSK) $@ -n > /dev/null
	$(IDSK) $@ -i build/HELLOC.BIN > /dev/null      # one file per call; the header is kept
	$(IDSK) $@ -i build/HELLOA.BIN > /dev/null
	$(IDSK) $@ -i build/DEMO.BIN > /dev/null
	$(IDSK) $@ -i build/HELLO.BAS -t 0 > /dev/null

$(OBJ):
	mkdir -p $@ build/logs build/cpcse-home

sd: build/HELLOC.BIN build/HELLOA.BIN build/HELLO.BAS build/DEMO.BIN
	mkdir -p $(M4SD)
	cp $^ $(M4SD)/

# local web server for the samples, only on 127.0.0.1
# WWW_PROC: anchored at the first word (python3, or Homebrew's .../Python on macOS), so pgrep/pkill find the
# server and never the recipe shell, whose command line contains the same text
WWW_PROC  = ^[^ ]*[Pp]ython[^ ]* -m http.server $(WWW_PORT) --bind 127.0.0.1 --directory $(DEV)/samples/www
www: | $(OBJ)
	@pgrep -f "$(WWW_PROC)" >/dev/null || \
	  ( nohup python3 -m http.server $(WWW_PORT) --bind 127.0.0.1 --directory $(DEV)/samples/www \
	    >> build/logs/www.log 2>&1 & sleep 1 )

# M4 on, its SD card = m4sd/, PC key layout (each key types its character) - set once, the
# emulator keeps its own settings in build/cpcse-home/cpcse.ini after that
build/cpcse-home/cpcse.ini: | $(OBJ)
	printf 'm4=1\nm4net=1\nm4folder=%s\nkeymapping=pc\n' "$(M4SD)" > $@

run: all www build/cpcse-home/cpcse.ini
	cd $(CPCSE) && $(CPCSE_ENV) ./cpcse --roms $(CPCSE)/roms

# real time with a hidden window, driven through the emulator's command API (tools/cpcshot.py)
KEYS      ?=
API_PORT  ?= 6128
shot: all www build/cpcse-home/cpcse.ini | $(OBJ)
	rm -f build/shot.png
	python3 -I tools/cpcshot.py $(API_PORT) $(FRAMES) '$(CMD)\n' '$(KEYS)' $(DEV)/build/shot.png -- \
	  sh -c 'cd $(CPCSE) && exec env $(CPCSE_ENV) ./cpcse --roms $(CPCSE)/roms "$$@"' cpcse

# headless and as fast as it can: the shot mode finds its ROMs only in the current folder, hence the cd
fastshot: all www | $(OBJ)
	cd $(CPCSE) && $(CPCSE_ENV) ./cpcse --shot $(DEV)/build/shot.bmp --model cpc6128 \
	  --frames $(FRAMES) --m4 $(M4SD) --type '$(CMD)\n' --wav $(DEV)/build/shot.wav > $(DEV)/build/logs/shot.log 2>&1
	$(if $(filter Darwin,$(UNAME)),sips -s format png build/shot.bmp --out build/shot.png > /dev/null,python3 -I tools/bmp2png.py build/shot.bmp build/shot.png)
	@echo "screenshot: build/shot.png"

stop:
	@pkill -f "$(WWW_PROC)" || true

clean:
	rm -rf build/obj build/*.BIN build/*.BAS build/*.dsk build/*.ihx build/*.map build/*.lk build/*.noi build/shot.* build/demo_raster.s

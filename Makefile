# Development kit for the Amstrad/Schneider CPC + M4 board: SDCC (C), RASM (assembler), iDSK (disc
# images), CPCSyntaxError (emulator with M4 incl. WiFi through the Mac's network) and lib/m4 (our
# own M4 library: HTTP into memory, TCP sockets, DNS). Tools in opt/ (built by tools/setup.sh).
#
#   make              build the samples: build/HELLOC.BIN, HELLOA.BIN, HELLO.BAS + build/samples.dsk,
#                     and copy them onto the emulated M4's SD card (m4sd/)
#   make www          local test web server for the network samples (127.0.0.1:8765, samples/www)
#   make run          the emulator in a window, M4 on, SD card = m4sd/ (then: RUN"HELLOC")
#   make shot CMD='RUN"HELLOC"'   headless run, screenshot in build/shot.png (for tests)
#   make stop         stops the test web server

DEV       := $(CURDIR)
SDCC      := $(shell brew --prefix sdcc 2>/dev/null)/bin/sdcc
SDASZ80   := $(dir $(SDCC))sdasz80
RASM      := $(DEV)/opt/rasm/rasm
IDSK      := $(DEV)/opt/idsk/iDSK
CPCSE     := $(DEV)/opt/cpcse
M4SD      := $(DEV)/m4sd
WWW_PORT  ?= 8765
FRAMES    ?= 600
CMD       ?= RUN"HELLOC"

CFLAGS    := -mz80 -I lib/cpc -I lib/m4 --opt-code-size
# crt0 starts at &4000 (a JP to init), the code right after it
LDFLAGS   := -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0

# the emulator: settings in build/cpcse-home (never next to the program or in the home folder)
CPCSE_ENV := CPCSE_HOME=$(DEV)/build/cpcse-home

OBJ       := build/obj

.PHONY: all sd www run shot stop clean

all: build/HELLOC.BIN build/HELLOA.BIN build/HELLO.BAS build/samples.dsk sd

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

build/HELLOA.BIN: samples/hello.asm | $(OBJ)
	cd build && $(RASM) -o hello ../$< > logs/rasm.log 2>&1 || { cat logs/rasm.log; exit 1; }

# ASCII BASIC: LOAD/RUN on the CPC tokenizes it
build/HELLO.BAS: samples/hello.bas | $(OBJ)
	cp $< $@

build/samples.dsk: build/HELLOC.BIN build/HELLOA.BIN build/HELLO.BAS
	rm -f $@
	$(IDSK) $@ -n > /dev/null
	$(IDSK) $@ -i build/HELLOC.BIN > /dev/null      # one file per call; the header is kept
	$(IDSK) $@ -i build/HELLOA.BIN > /dev/null
	$(IDSK) $@ -i build/HELLO.BAS -t 0 > /dev/null

$(OBJ):
	mkdir -p $@ build/logs build/cpcse-home

sd: build/HELLOC.BIN build/HELLOA.BIN build/HELLO.BAS
	mkdir -p $(M4SD)
	cp $^ $(M4SD)/

# local web server for the samples, only on 127.0.0.1
www: | $(OBJ)
	@pgrep -f "http.server $(WWW_PORT) --bind 127.0.0.1 --directory $(DEV)/samples/www" >/dev/null || \
	  ( nohup python3 -m http.server $(WWW_PORT) --bind 127.0.0.1 --directory $(DEV)/samples/www \
	    >> build/logs/www.log 2>&1 & sleep 1 )

# M4 on, its SD card = m4sd/, PC key layout (each key types its character) - set once, the
# emulator keeps its own settings in build/cpcse-home/cpcse.ini after that
build/cpcse-home/cpcse.ini: | $(OBJ)
	printf 'm4=1\nm4net=1\nm4folder=%s\nkeymapping=pc\n' "$(M4SD)" > $@

run: all www build/cpcse-home/cpcse.ini
	cd $(CPCSE) && $(CPCSE_ENV) ./cpcse --roms $(CPCSE)/roms

# headless: the shot mode finds its ROMs only in the current folder, hence the cd
shot: all www | $(OBJ)
	cd $(CPCSE) && $(CPCSE_ENV) ./cpcse --shot $(DEV)/build/shot.bmp --model cpc6128 \
	  --frames $(FRAMES) --m4 $(M4SD) --type '$(CMD)\n' > $(DEV)/build/logs/shot.log 2>&1
	sips -s format png build/shot.bmp --out build/shot.png > /dev/null
	@echo "screenshot: build/shot.png"

stop:
	@pkill -f "http.server $(WWW_PORT) --bind 127.0.0.1 --directory $(DEV)/samples/www" || true

clean:
	rm -rf build/obj build/*.BIN build/*.BAS build/*.dsk build/*.ihx build/*.map build/*.lk build/*.noi build/shot.*

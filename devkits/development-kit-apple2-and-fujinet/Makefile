# Development kit for the Apple II + FujiNet: cc65 (C + ca65), fujinet-lib, cadius (ProDOS disk
# images), ProDOS 2.4.3 (boot disks) and FujiNet Go Apple II (AppleWin core + FujiNet built in).
# All tools live in opt/ (built by tools/setup.sh).
#
#   make run DISK=path/to/disk.po   put a disk image on the emulated FujiNet's SD card, start the emulator
#   make run                        just start the emulator (it boots into FujiNet's CONFIG)
#
# In CONFIG: host SD -> the image -> slot 1 -> ESC boots it.
# Ports: 1985 (emulator <-> FujiNet, SmartPort over SLIP) and 64001 (FujiNet web UI), 127.0.0.1 only.

DEV     := $(CURDIR)
APP     := $(DEV)/opt/fngo/FujiNet Go Apple II.app/Contents/MacOS/FujiNet Go Apple II
# settings and data in build/, never in ~/.config or ~/.local/share
XDG_ENV := XDG_CONFIG_HOME=$(DEV)/build/xdg-config XDG_DATA_HOME=$(DEV)/build/xdg-data
SD      := $(DEV)/build/xdg-data/fujinet-go-apple2/fujinet/SD

.PHONY: run

run:
	mkdir -p "$(SD)"
	$(if $(DISK),cp "$(DISK)" "$(SD)/")
	$(XDG_ENV) "$(APP)"

#!/bin/sh
# Builds the whole toolchain into opt/ (sources in src/). Linux (Debian/Ubuntu with apt, Ubuntu 26.04 x86_64 tested; macOS: setup-macos.sh).
#   tools/setup-linux.sh            build everything that is missing
#   tools/setup-linux.sh cc65 ...   (re)build only the named parts: cc65 fujinet-lib atari800 fastbasic fujinet-pc
set -e

K="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$K/src"; OPT="$K/opt"; LOG="$K/build/logs"
JOBS="$(nproc)"

# pinned versions (as tested 2026-11-14)
CC65_REF=master                # cc65 has no recent releases; V2.19 from git
FNLIB_TAG=v5.1.0
ATARI800_TAG=98cc2dc            # master after 7.2.1 (NetSIO; -record-video for make shot); distribution packages lack NetSIO
FASTBASIC_TAG=v4.8.1
FUJINET_TAG=v1.6.1

mkdir -p "$SRC" "$OPT" "$LOG"

# Windows: WSL2 (not tested yet) - only hints, nothing is changed
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "==> WSL detected (Windows) - not tested yet, please report problems (see README, Windows)"
    case "$K" in /mnt/*)
        echo "    The kit is on a Windows drive ($K): that is slow and breaks file permissions."
        echo "    Clone it into the Linux home (~/) instead. (WSL_MNT_OK=1 to try anyway)"
        [ -n "${WSL_MNT_OK:-}" ] || exit 1 ;;
    esac
    [ -n "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ] || \
        echo "    No WSLg display: emulator windows will not open (wsl --update; make shot works without)."
    echo "    Connections from other machines into WSL need networkingMode=mirrored (see README)."
fi

# check the Debian/Ubuntu packages (no sudo from here - print the apt line and stop)
apt_need() {
    missing=""
    for p in "$@"; do
        dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'ok installed' || missing="$missing $p"
    done
    [ -z "$missing" ] || { echo "missing packages:$missing"; echo "  sudo apt install$missing"; exit 1; }
}
step() { echo "==> $1 (log: build/logs/$2.log)"; }

clone() {  # clone <github repo> <dir> <ref>
    [ -d "$SRC/$2" ] || git clone -q "https://github.com/$1.git" "$SRC/$2"
    git -C "$SRC/$2" fetch -q --tags
    git -C "$SRC/$2" checkout -q "$3"
    [ "$3" != master ] || git -C "$SRC/$2" pull -q
}

prereqs() {
    apt_need build-essential git autoconf automake cmake pkg-config libsdl2-dev libmbedtls-dev python3-venv gh unzip curl
}

build_cc65() {
    step cc65 cc65
    clone cc65/cc65 cc65 "$CC65_REF"
    ( cd "$SRC/cc65" && make -j"$JOBS" && make install PREFIX="$OPT/cc65" ) >"$LOG/cc65.log" 2>&1
    "$OPT/cc65/bin/cl65" --version
}

build_fujinet_lib() {
    step fujinet-lib fujinet-lib
    rm -rf "$OPT/fujinet-lib" && mkdir -p "$OPT/fujinet-lib"
    ( cd "$OPT/fujinet-lib" && gh release download "$FNLIB_TAG" -R FujiNetWIFI/fujinet-lib -p "*atari*" \
        && unzip -oq fujinet-lib-atari-*.zip ) >"$LOG/fujinet-lib.log" 2>&1
    ls "$OPT"/fujinet-lib/fujinet-atari-*.lib
}

build_atari800() {
    step atari800 atari800
    clone atari800/atari800 atari800 "$ATARI800_TAG"
    ( cd "$SRC/atari800" && ./autogen.sh \
        && ./configure --prefix="$OPT/atari800" --target=default --with-video=sdl2 --with-sound=sdl2 --disable-download \
        && make -j"$JOBS" && make install ) >"$LOG/atari800.log" 2>&1
    "$OPT/atari800/bin/atari800" -help | grep -q -- -netsio && echo "atari800 with NetSIO"
}

build_fastbasic() {
    step fastbasic fastbasic
    clone dmsc/fastbasic fastbasic "$FASTBASIC_TAG"
    ( cd "$SRC/fastbasic" && git submodule update --init && make -j"$JOBS" ) >"$LOG/fastbasic.log" 2>&1
    rm -rf "$OPT/fastbasic" && mkdir -p "$OPT/fastbasic"
    cp -R "$SRC/fastbasic/build/compiler/." "$OPT/fastbasic/"   # GNU cp: dir/. = its contents
    cp "$SRC/fastbasic/build/fastbasic.atr" "$OPT/fastbasic/"   # the FastBasic IDE for the Atari itself
    "$OPT/fastbasic/fastbasic" -v
}

build_fujinet_pc() {
    step fujinet-pc fujinet-pc
    clone FujiNetWIFI/fujinet-firmware fujinet-firmware "$FUJINET_TAG"
    git -C "$SRC/fujinet-firmware" submodule update -q --init --recursive
    # mbedTLS 3 = the system libmbedtls-dev; Python venv inside the kit instead of ~/.platformio
    ( cd "$SRC/fujinet-firmware" && ./build.sh -c -y -p ATARI -V "$K/build/venv-fujinet" ) >"$LOG/fujinet-pc.log" 2>&1
    # keep the SD card and settings of an existing install
    if [ -d "$OPT/fujinet-pc" ]; then
        cp "$SRC/fujinet-firmware/build/dist/fujinet" "$OPT/fujinet-pc/"
        cp -R "$SRC/fujinet-firmware/build/dist/data/." "$OPT/fujinet-pc/data/"
    else
        cp -R "$SRC/fujinet-firmware/build/dist" "$OPT/fujinet-pc"
    fi
    echo "FujiNet-PC $FUJINET_TAG"
}

if [ $# -eq 0 ]; then
    prereqs
    [ -x "$OPT/cc65/bin/cl65" ]               || build_cc65
    ls "$OPT"/fujinet-lib/fujinet-atari-*.lib >/dev/null 2>&1 || build_fujinet_lib
    [ -x "$OPT/atari800/bin/atari800" ]       || build_atari800
    [ -x "$OPT/fastbasic/fastbasic" ]         || build_fastbasic
    [ -x "$OPT/fujinet-pc/fujinet" ]          || build_fujinet_pc
else
    for part in "$@"; do
        case "$part" in
            cc65) build_cc65 ;; fujinet-lib) build_fujinet_lib ;; atari800) build_atari800 ;;
            fastbasic) build_fastbasic ;; fujinet-pc) build_fujinet_pc ;;
            *) echo "unknown part: $part"; exit 1 ;;
        esac
    done
fi
echo "Toolchain ready in opt/"

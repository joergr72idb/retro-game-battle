#!/bin/sh
# Builds the whole toolchain into opt/ (sources in src/). Linux (Debian/Ubuntu with apt, Ubuntu 26.04 x86_64 tested; macOS: setup-macos.sh).
#   tools/setup-linux.sh            build everything that is missing
#   tools/setup-linux.sh vice ...   (re)build only the named parts: cc65 fujinet-lib fujinet-pc vice
set -e

K="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$K/src"; OPT="$K/opt"; LOG="$K/build/logs"
JOBS="$(nproc)"

# pinned versions (as tested 2026-11-14)
CC65_REF=master                # cc65 has no recent releases; V2.19 from git
FNLIB_TAG=v5.1.1               # first release with the C64 fixes current firmware needs
# FujiNet-PC for IEC (NetIEC) is only on master so far (PR #2299, 2026-11-06) - pinned to the tested commit
FUJINET_REF=8cc29e8b5
# dillera's VICE fork: real devices 12-30 + the NetIEC bridge (opencbm-netiec)
VICE_REF=7f4e552

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
    git -C "$SRC/$2" fetch -q origin "$3" 2>/dev/null || git -C "$SRC/$2" fetch -q --tags
    git -C "$SRC/$2" checkout -q "$3"
    [ "$3" != master ] || git -C "$SRC/$2" pull -q
}

prereqs() {
    apt_need build-essential git autoconf automake cmake pkg-config libsdl2-dev libsdl2-image-dev libmbedtls-dev python3-venv libssl-dev libcurl4-openssl-dev unzip curl xa65 dos2unix bison flex texinfo
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
    ( cd "$OPT/fujinet-lib" && curl -fsSLO "https://github.com/FujiNetWIFI/fujinet-lib/releases/download/$FNLIB_TAG/fujinet-lib-c64-${FNLIB_TAG#v}.zip" \
        && unzip -oq fujinet-lib-c64-*.zip ) >"$LOG/fujinet-lib.log" 2>&1
    ls "$OPT"/fujinet-lib/fujinet-c64-*.lib
}

build_fujinet_pc() {
    step fujinet-pc fujinet-pc
    clone FujiNetWIFI/fujinet-firmware fujinet-firmware "$FUJINET_REF"
    git -C "$SRC/fujinet-firmware" submodule update -q --init --recursive
    # IEC target = Commodore bus incl. Meatloaf drives; mbedTLS 3 = the system libmbedtls-dev; Python venv inside the kit instead of ~/.platformio
    ( cd "$SRC/fujinet-firmware" && ./build.sh -c -y -p IEC -V "$K/build/venv-fujinet" ) >"$LOG/fujinet-pc.log" 2>&1
    # keep the SD card and settings of an existing install
    if [ -d "$OPT/fujinet-pc" ]; then
        cp "$SRC/fujinet-firmware/build/dist/fujinet" "$OPT/fujinet-pc/"
        cp -R "$SRC/fujinet-firmware/build/dist/data/." "$OPT/fujinet-pc/data/"
    else
        cp -R "$SRC/fujinet-firmware/build/dist" "$OPT/fujinet-pc"
    fi
    grep 'tests passed' "$LOG/fujinet-pc.log"
}

build_vice() {
    step vice vice
    [ -d "$SRC/vice-fn" ] || git clone -q --depth 1 https://github.com/dillera/vice-fn.git "$SRC/vice-fn"
    git -C "$SRC/vice-fn" fetch -q --depth 1 origin "$VICE_REF" 2>/dev/null || true
    git -C "$SRC/vice-fn" checkout -q "$VICE_REF"
    rm -rf "$K/build/vice-sdl2" && mkdir -p "$K/build/vice-sdl2"
    ( cd "$SRC/vice-fn/vice" && ./autogen.sh \
        && cd "$K/build/vice-sdl2" \
        && "$SRC/vice-fn/vice/configure" --prefix="$OPT/vice" --enable-sdl2ui \
             --disable-pdf-docs --disable-html-docs YACC="bison -y" \
        && make -j"$JOBS" && make install ) >"$LOG/vice.log" 2>&1
    # the bridge VICE loads as libopencbm.so: sends the serial bus over TCP to FujiNet-PC
    make -C "$SRC/vice-fn/fujinet/opencbm-netiec" >"$LOG/bridge.log" 2>&1
    mkdir -p "$OPT/vice-bridge"
    cp "$SRC/vice-fn/fujinet/opencbm-netiec/libopencbm.so" "$SRC/vice-fn/fujinet/opencbm-netiec/netiecctl" \
       "$OPT/vice-bridge/"
    ls "$OPT/vice/bin/x64sc" "$OPT/vice-bridge/libopencbm.so"
}

if [ $# -eq 0 ]; then
    prereqs
    [ -x "$OPT/cc65/bin/cl65" ]                        || build_cc65
    ls "$OPT"/fujinet-lib/fujinet-c64-*.lib >/dev/null 2>&1 || build_fujinet_lib
    [ -x "$OPT/fujinet-pc/fujinet" ]                   || build_fujinet_pc
    [ -f "$OPT/vice-bridge/libopencbm.so" ]           || build_vice
else
    for part in "$@"; do
        case "$part" in
            cc65) build_cc65 ;; fujinet-lib) build_fujinet_lib ;; fujinet-pc) build_fujinet_pc ;;
            vice) build_vice ;;
            *) echo "unknown part: $part"; exit 1 ;;
        esac
    done
fi
echo "Toolchain ready in opt/"

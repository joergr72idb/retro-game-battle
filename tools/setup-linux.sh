#!/bin/sh
# Builds the toolchain into opt/ (sources in src/). Linux (Debian/Ubuntu with apt, Ubuntu 26.04 x86_64 tested; macOS: setup-macos.sh).
#   tools/setup-linux.sh              build everything that is missing
#   tools/setup-linux.sh cpcse ...    (re)build only the named parts: rasm idsk cpcse
#   tools/setup-linux.sh fujinet      optional: fetch fujinet-network-cpc (fujinet-lib's network API, GPL-3)
# SDCC comes from apt (sdcc).
set -e

K="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$K/src"; OPT="$K/opt"; LOG="$K/build/logs"
JOBS="$(nproc)"

# pinned versions (as tested 2026-11-14)
RASM_TAG=v3.3.3
IDSK_TAG=v0.20
CPCSE_TAG=v0.16.0               # CPCSyntaxError changes almost daily - update on purpose, then retest
FNCPC_REV=841f7c0               # fujinet-network-cpc (optional part "fujinet", as tested 2026-11-15)

mkdir -p "$SRC" "$OPT" "$LOG"

# check the Debian/Ubuntu packages (no sudo from here - print the apt line and stop)
apt_need() {
    missing=""
    for p in "$@"; do
        dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'ok installed' || missing="$missing $p"
    done
    [ -z "$missing" ] || { echo "missing packages:$missing"; echo "  sudo apt install$missing"; exit 1; }
}
step() { echo "==> $1 (log: build/logs/$2.log)"; }

clone() {  # clone <github repo> <dir> <tag>
    [ -d "$SRC/$2" ] || git clone -q "https://github.com/$1.git" "$SRC/$2"
    git -C "$SRC/$2" fetch -q --tags
    git -C "$SRC/$2" checkout -q "$3"
}

prereqs() {
    apt_need build-essential git cmake ninja-build libsdl2-dev libgl-dev sdcc
}

build_rasm() {
    step rasm rasm
    clone EdouardBERGE/rasm rasm "$RASM_TAG"
    ( cd "$SRC/rasm" && make -j"$JOBS" ) >"$LOG/rasm-build.log" 2>&1
    mkdir -p "$OPT/rasm" && cp "$SRC/rasm/rasm.exe" "$OPT/rasm/rasm"
    "$OPT/rasm/rasm" 2>&1 | head -1
}

build_idsk() {
    step idsk idsk
    clone cpcsdk/idsk idsk "$IDSK_TAG"
    # GCC 15 defaults to C++17, which forbids iDSK's old throw(...) specifications: build it as C++14
    ( cd "$SRC/idsk" && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-std=gnu++14 \
        && cmake --build build -j"$JOBS" ) \
        >"$LOG/idsk.log" 2>&1
    mkdir -p "$OPT/idsk" && cp "$SRC/idsk/build/iDSK" "$OPT/idsk/"
    ls "$OPT/idsk/iDSK"
}

# optional, not part of the default run: a source library (GPL-3.0-or-later), compiled by the
# programs that use it - here by `make fujinet`
get_fujinet() {
    step fujinet-network-cpc fujinet
    clone joergr72idb/fujinet-network-cpc fujinet-network-cpc "$FNCPC_REV" >"$LOG/fujinet.log" 2>&1
    ls "$SRC/fujinet-network-cpc/src/fujinet-network.c"
}

build_cpcse() {
    step CPCSyntaxError cpcse
    clone WacKEDmaN/CPCSyntaxError CPCSyntaxError "$CPCSE_TAG"
    # SDL2 via pkg-config (the project's fallback). Ubuntu's sdl2-config.cmake checks that /usr
    # EXISTS, and CMake 4 counts a directory without read permission (hardened /usr) as missing.
    ( cd "$SRC/CPCSyntaxError" && cmake -S . -B "$K/build/cpcse" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=ON \
        && cmake --build "$K/build/cpcse" ) >"$LOG/cpcse.log" 2>&1
    # the program, its ROMs (CPC firmware, AMSDOS, M4ROM by Duke) and sounds; settings go to build/cpcse-home
    rm -rf "$OPT/cpcse" && mkdir -p "$OPT/cpcse"
    cp "$K/build/cpcse/cpcse" "$K/build/cpcse/cpcse-ctl" "$OPT/cpcse/"
    cp -R "$K/build/cpcse/roms" "$K/build/cpcse/sounds" "$OPT/cpcse/"
    ls "$OPT/cpcse/cpcse"
}

if [ $# -eq 0 ]; then
    prereqs
    [ -x "$OPT/rasm/rasm" ]   || build_rasm
    [ -x "$OPT/idsk/iDSK" ]   || build_idsk
    [ -x "$OPT/cpcse/cpcse" ] || build_cpcse
else
    for part in "$@"; do
        case "$part" in
            rasm) build_rasm ;; idsk) build_idsk ;; cpcse) build_cpcse ;; fujinet) get_fujinet ;;
            *) echo "unknown part: $part"; exit 1 ;;
        esac
    done
fi
echo "Toolchain ready in opt/"

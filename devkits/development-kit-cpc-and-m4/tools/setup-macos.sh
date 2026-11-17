#!/bin/sh
# Builds the toolchain into opt/ (sources in src/). macOS with Homebrew, Apple Silicon tested (Linux: setup-linux.sh).
#   tools/setup-macos.sh              build everything that is missing
#   tools/setup-macos.sh cpcse ...    (re)build only the named parts: rasm idsk cpcse
#   tools/setup-macos.sh fujinet      optional: fetch fujinet-network-cpc (fujinet-lib's network API, GPL-3)
# SDCC comes from Homebrew (brew install sdcc).
set -e

K="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$K/src"; OPT="$K/opt"; LOG="$K/build/logs"
JOBS="$(sysctl -n hw.ncpu)"

# pinned versions (as tested 2026-11-14)
RASM_TAG=v3.3.3
IDSK_TAG=v0.20
CPCSE_TAG=v0.16.0               # CPCSyntaxError changes almost daily - update on purpose, then retest
FNCPC_REV=aef3b77               # fujinet-network-cpc (optional part "fujinet", as tested 2026-11-17)

mkdir -p "$SRC" "$OPT" "$LOG"

step() { echo "==> $1 (log: build/logs/$2.log)"; }

clone() {  # clone <github repo> <dir> <tag>
    [ -d "$SRC/$2" ] || git clone -q "https://github.com/$1.git" "$SRC/$2"
    git -C "$SRC/$2" fetch -q --tags
    git -C "$SRC/$2" checkout -q "$3"
}

prereqs() {
    brew install cmake ninja sdl2 sdcc
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
    ( cd "$SRC/idsk" && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$JOBS" ) \
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
    # make shot: no vsync with the hidden window (macOS never refreshes it - the emulator stood still)
    git -C "$SRC/CPCSyntaxError" apply --reverse --check "$K/tools/patches/cpcse-hidden-window-no-vsync.patch" 2>/dev/null \
        || git -C "$SRC/CPCSyntaxError" apply "$K/tools/patches/cpcse-hidden-window-no-vsync.patch"
    ( cd "$SRC/CPCSyntaxError" && cmake -S . -B "$K/build/cpcse" -G Ninja -DCMAKE_BUILD_TYPE=Release \
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

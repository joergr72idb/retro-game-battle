#!/bin/sh
# Builds the toolchain into opt/ (sources in src/). macOS with Homebrew, Apple Silicon tested (Linux: setup-linux.sh).
#   tools/setup-macos.sh               build everything that is missing
#   tools/setup-macos.sh fngo ...      (re)build only the named parts: cc65 fujinet-lib cadius prodos fngo
set -e

K="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$K/src"; OPT="$K/opt"; LOG="$K/build/logs"
JOBS="$(sysctl -n hw.ncpu)"

# pinned versions (as tested 2026-11-14)
CC65_REF=master
FNLIB_TAG=v5.1.1
CADIUS_TAG=1.4.5
FNGO_TAG=v0.10.0                # FujiNet Go Apple II (AppleWin core + FujiNet built in)
PRODOS_URL=https://raw.githubusercontent.com/ProDOS-8/ProDOS8-Releases/HEAD/ProDOS_2_4_3.po

mkdir -p "$SRC" "$OPT" "$LOG"
step() { echo "==> $1 (log: build/logs/$2.log)"; }

clone() {  # clone <github repo> <dir> <ref>
    [ -d "$SRC/$2" ] || git clone -q "https://github.com/$1.git" "$SRC/$2"
    git -C "$SRC/$2" fetch -q --tags
    git -C "$SRC/$2" checkout -q "$3"
    [ "$3" != master ] || git -C "$SRC/$2" pull -q
}

prereqs() {
    brew install cmake ninja sdl3 boost gh
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
    ( cd "$OPT/fujinet-lib" && gh release download "$FNLIB_TAG" -R FujiNetWIFI/fujinet-lib -p "fujinet-lib-apple2-*.zip" \
        && unzip -oq fujinet-lib-apple2-*.zip ) >"$LOG/fujinet-lib.log" 2>&1
    ls "$OPT"/fujinet-lib/fujinet-apple2-*.lib
}

build_cadius() {
    step cadius cadius
    clone mach-kernel/cadius cadius "$CADIUS_TAG"
    ( cd "$SRC/cadius" && make ) >"$LOG/cadius.log" 2>&1
    mkdir -p "$OPT/cadius" && cp -L "$SRC/cadius/cadius" "$OPT/cadius/"
}

# ProDOS 2.4.3 for boot disks: the PRODOS file and the boot blocks (no licence stated - keep it out of git)
get_prodos() {
    step ProDOS prodos
    rm -rf "$OPT/prodos" && mkdir -p "$OPT/prodos/x"
    ( cd "$OPT/prodos" && curl -s -f -L -o ProDOS_2_4_3.po "$PRODOS_URL" \
        && head -c 1024 ProDOS_2_4_3.po > bootblocks.bin \
        && cd x && "$OPT/cadius/cadius" EXTRACTFILE ../ProDOS_2_4_3.po /PRODOS.2.4.3/PRODOS . --quiet ) \
        >"$LOG/prodos.log" 2>&1
    # cadius adds the type to the name and mirrors absolute paths: take the file, drop the rest
    mv "$(find "$OPT/prodos/x" -type f -name 'PRODOS#*' | head -1)" "$OPT/prodos/PRODOS"
    rm -rf "$OPT/prodos/x"
    touch "$OPT/prodos/PRODOS"          # cadius sets a 1924 date, which confuses make
    ls -l "$OPT/prodos/PRODOS"
}

build_fngo() {
    step "FujiNet Go Apple II" fngo
    [ -d "$SRC/fujinet-go-apple2-desktop" ] || \
        git clone -q https://github.com/FujiNetWIFI/fujinet-go-apple2-desktop.git "$SRC/fujinet-go-apple2-desktop"
    git -C "$SRC/fujinet-go-apple2-desktop" fetch -q --tags
    git -C "$SRC/fujinet-go-apple2-desktop" checkout -q "$FNGO_TAG"
    ( cd "$SRC/fujinet-go-apple2-desktop" && cmake -B "$K/build/fngo" -G Ninja -DFRONTEND=macos \
        -DCMAKE_BUILD_TYPE=Release && cmake --build "$K/build/fngo" ) >"$LOG/fngo.log" 2>&1
    rm -rf "$OPT/fngo" && mkdir -p "$OPT/fngo"
    cp -R "$K/build/fngo/frontends/macos/FujiNet Go Apple II.app" "$OPT/fngo/"
    ls -d "$OPT/fngo/FujiNet Go Apple II.app"
}

if [ $# -eq 0 ]; then
    prereqs
    [ -x "$OPT/cc65/bin/cl65" ]                              || build_cc65
    ls "$OPT"/fujinet-lib/fujinet-apple2-*.lib >/dev/null 2>&1 || build_fujinet_lib
    [ -x "$OPT/cadius/cadius" ]                              || build_cadius
    [ -f "$OPT/prodos/PRODOS" ]                              || get_prodos
    [ -d "$OPT/fngo/FujiNet Go Apple II.app" ]               || build_fngo
else
    for part in "$@"; do
        case "$part" in
            cc65) build_cc65 ;; fujinet-lib) build_fujinet_lib ;; cadius) build_cadius ;;
            prodos) get_prodos ;; fngo) build_fngo ;;
            *) echo "unknown part: $part"; exit 1 ;;
        esac
    done
fi
echo "Toolchain ready in opt/"

#!/bin/sh
# Builds the whole toolchain into opt/ (sources in src/). macOS with Homebrew, Apple Silicon tested.
#   tools/setup.sh            build everything that is missing
#   tools/setup.sh vice ...   (re)build only the named parts: cc65 fujinet-lib fujinet-pc vice
set -e

K="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$K/src"; OPT="$K/opt"; LOG="$K/build/logs"
JOBS="$(sysctl -n hw.ncpu)"

# pinned versions (as tested 2026-11-14)
CC65_REF=master                # cc65 has no recent releases; V2.19 from git
FNLIB_TAG=v5.1.1               # first release with the C64 fixes current firmware needs
# FujiNet-PC for IEC (NetIEC) is only on master so far (PR #2299, 2026-11-06) - pinned to the tested commit
FUJINET_REF=8cc29e8b5
# dillera's VICE fork: real devices 12-30 + the NetIEC bridge (opencbm-netiec)
VICE_REF=7f4e552

mkdir -p "$SRC" "$OPT" "$LOG"

step() { echo "==> $1 (log: build/logs/$2.log)"; }

clone() {  # clone <github repo> <dir> <ref>
    [ -d "$SRC/$2" ] || git clone -q "https://github.com/$1.git" "$SRC/$2"
    git -C "$SRC/$2" fetch -q origin "$3" 2>/dev/null || git -C "$SRC/$2" fetch -q --tags
    git -C "$SRC/$2" checkout -q "$3"
    [ "$3" != master ] || git -C "$SRC/$2" pull -q
}

prereqs() {
    brew install autoconf automake cmake pkgconf sdl2 sdl2_image mbedtls@3 gh xa dos2unix bison
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
    ( cd "$OPT/fujinet-lib" && gh release download "$FNLIB_TAG" -R FujiNetWIFI/fujinet-lib -p "fujinet-lib-c64-*.zip" \
        && unzip -oq fujinet-lib-c64-*.zip ) >"$LOG/fujinet-lib.log" 2>&1
    ls "$OPT"/fujinet-lib/fujinet-c64-*.lib
}

build_fujinet_pc() {
    step fujinet-pc fujinet-pc
    clone FujiNetWIFI/fujinet-firmware fujinet-firmware "$FUJINET_REF"
    git -C "$SRC/fujinet-firmware" submodule update -q --init --recursive
    # IEC target = Commodore bus incl. Meatloaf drives; Python venv inside the kit instead of ~/.platformio
    ( cd "$SRC/fujinet-firmware" && MBEDTLS_ROOT_DIR="$(brew --prefix mbedtls@3)" \
        ./build.sh -c -y -p IEC -V "$K/build/venv-fujinet" ) >"$LOG/fujinet-pc.log" 2>&1
    # keep the SD card and settings of an existing install
    if [ -d "$OPT/fujinet-pc" ]; then
        cp "$SRC/fujinet-firmware/build/dist/fujinet" "$OPT/fujinet-pc/"
        cp -R "$SRC/fujinet-firmware/build/dist/data/" "$OPT/fujinet-pc/data/"
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
             --disable-pdf-docs --disable-html-docs YACC="$(brew --prefix bison)/bin/bison -y" \
        && make -j"$JOBS" && make install ) >"$LOG/vice.log" 2>&1
    # the bridge VICE loads as libopencbm.dylib: sends the serial bus over TCP to FujiNet-PC
    make -C "$SRC/vice-fn/fujinet/opencbm-netiec" >"$LOG/bridge.log" 2>&1
    mkdir -p "$OPT/vice-bridge"
    cp "$SRC/vice-fn/fujinet/opencbm-netiec/libopencbm.dylib" "$SRC/vice-fn/fujinet/opencbm-netiec/netiecctl" \
       "$OPT/vice-bridge/"
    ls "$OPT/vice/bin/x64sc" "$OPT/vice-bridge/libopencbm.dylib"
}

if [ $# -eq 0 ]; then
    prereqs
    [ -x "$OPT/cc65/bin/cl65" ]                        || build_cc65
    ls "$OPT"/fujinet-lib/fujinet-c64-*.lib >/dev/null 2>&1 || build_fujinet_lib
    [ -x "$OPT/fujinet-pc/fujinet" ]                   || build_fujinet_pc
    [ -f "$OPT/vice-bridge/libopencbm.dylib" ]         || build_vice
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

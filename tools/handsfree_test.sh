#!/usr/bin/env bash
# Hands-free test of the four C clients: nobody at the keyboard, no window.
#
#   tools/handsfree_test.sh [atari] [c64] [cpc] [apple2]      (default: all four, in that order)
#
# Starts the server headless on 127.0.0.1:$RGB_PORT (default 6602, HTTP $RGB_HTTP_PORT 8082) with its
# own statistics and log in build/handsfree/ (the event's game_results.csv is not touched). For each
# client: builds it for that server, starts a bot (bots/game_bot.sh) as the opponent and runs the
# client in its development kit's `make shot` with a key script: RETURN (server), RETURN (PIN), 1
# (the first game, Tron), then joystick moves. At the end it checks the server log: did the client
# connect, did the match start and end, did its moves arrive. Exit status 0 = all passed.
#
# Results in build/handsfree/: server.log, <client>.png (+ .txt for C64/Apple II), shot_/make_/bot_ logs.
# The kits come from devkits/<kit> if built there (opt/ exists), else ../<kit> - as in the client
# Makefiles. Needs each kit set up, and a desktop session for the CPC (hidden emulator window).
# The Atari runs on NetSIO $ATARI_NETSIO_PORT / web $ATARI_WEB_PORT (default 9996/8002), so it works
# next to another FujiNet-PC + atari800 on the default ports (e.g. the Blues BBS).
# Afterwards the clients in clients/*-tcp/ are built for the test server: run make there again
# before using them (disk-images/ are not touched).
set -u
R="$(cd "$(dirname "$0")/.." && pwd)"
OUT=$R/build/handsfree
PORT=${RGB_PORT:-6602}
HTTP=${RGB_HTTP_PORT:-8082}
A8_NETSIO=${ATARI_NETSIO_PORT:-9996}
A8_WEB=${ATARI_WEB_PORT:-8002}
CLIENTS=${*:-atari c64 cpc apple2}
cd "$R"
rm -rf "$OUT" && mkdir -p "$OUT"

kit() {   # kit <name>: the development kit's folder
    if [ -d "$R/devkits/$1/opt" ]; then echo "$R/devkits/$1"; else echo "$R/../$1"; fi
}
A8=$(kit development-kit-atari-xl-and-fujinet)
C64=$(kit development-kit-c64-and-fujinet)
CPC=$(kit development-kit-cpc-and-m4)
A2=$(kit development-kit-apple2-and-fujinet)

if python3 -I -c "import socket,sys; s=socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', $PORT))" 2>/dev/null; then :; else
    echo "port $PORT is in use - another server? Use RGB_PORT=..."; exit 1
fi

# the server, with statistics and log in build/handsfree/
python3 -u -c "
import os, runpy, sys
sys.path.insert(0, 'server')
import config
config.RESULTS_CSV_PATH = '$OUT/game_results.csv'
config.LOG_FILE_PATH = '$OUT/game_battle_server.log'
sys.argv = ['game_battle_server.py', '127.0.0.1', '$PORT', '$HTTP', '--headless']
runpy.run_path('server/game_battle_server.py', run_name='__main__')
" > "$OUT/server.log" 2>&1 &
SRV=$!
sleep 2

bot() {   # bot <delay s> <name>: the Tron opponent, started when the client is about to pick its game
    ( sleep "$1"; exec bash bots/game_bot.sh 127.0.0.1 "$PORT" ATARI "$2" ATARI TRON 6 "" 4 ) > "$OUT/bot_$2.log" 2>&1 &
    BOT=$!
}
endbot() { pkill -P "$BOT" 2>/dev/null; kill "$BOT" 2>/dev/null; wait "$BOT" 2>/dev/null; }

moves() {   # moves <n> <frames per move>: a square, over and over
    local m="" i
    for i in $(seq "$1"); do m="$m joy up $2; joy left $2; joy down $2; joy right $2;"; done
    echo "$m"
}

for c in $CLIENTS; do
    echo "=== $c $(date +%T)"
    case $c in
    atari)
        make -C clients/atari-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_atari.log" 2>&1 || { tail "$OUT/make_atari.log"; continue; }
        bot 14 BotAtari
        make -C "$A8" shot NETSIO_PORT="$A8_NETSIO" WEB_PORT="$A8_WEB" XEX="$R/clients/atari-tcp/rgbtcp.xex" FRAMES=3000 \
            KEYS="wait 300; key RETURN; wait 50; key RETURN; wait 200; type 1; wait 150; $(moves 12 12) joy fire 10" \
            > "$OUT/shot_atari.log" 2>&1
        # make shot wrote the test NetSIO port into the kit's fnconfig.ini: back to the default
        sed -i.bak '/^\[BOIP\]/,/^\[/ s/^port=[0-9]*/port=9997/' "$A8/opt/fujinet-pc/fnconfig.ini" && rm -f "$A8/opt/fujinet-pc/fnconfig.ini.bak"
        cp "$A8/build/shot.png" "$OUT/atari.png" 2>/dev/null ;;
    c64)
        make -C clients/c64-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_c64.log" 2>&1 || { tail "$OUT/make_c64.log"; continue; }
        bot 26 BotC64
        make -C "$C64" shot PRG="$R/clients/c64-tcp/rgbtcp.prg" FRAMES=3000 \
            KEYS="wait 900; key RETURN; wait 50; key RETURN; wait 200; type 1; wait 150; $(moves 12 12) joy fire 10" \
            > "$OUT/shot_c64.log" 2>&1
        make -C "$C64" stop > /dev/null 2>&1
        cp "$C64/build/shot.png" "$OUT/c64.png" 2>/dev/null
        cp "$C64/build/shot.txt" "$OUT/c64.txt" 2>/dev/null ;;
    cpc)
        make -C clients/cpc-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_cpc.log" 2>&1 || { tail "$OUT/make_cpc.log"; continue; }
        mkdir -p "$CPC/m4sd" && cp clients/cpc-tcp/RGBTCP.BIN "$CPC/m4sd/"
        bot 16 BotCPC
        make -C "$CPC" shot CMD='RUN"RGBTCP"' FRAMES=2500 \
            KEYS="wait 200; key RETURN; wait 50; key RETURN; wait 300; key 1; wait 250; $(moves 12 16) joy fire 8" \
            > "$OUT/shot_cpc.log" 2>&1
        make -C "$CPC" stop > /dev/null 2>&1
        cp "$CPC/build/shot.png" "$OUT/cpc.png" 2>/dev/null ;;
    apple2)
        make -C clients/apple2-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_apple2.log" 2>&1 || { tail "$OUT/make_apple2.log"; continue; }
        bot 16 BotApple
        make -C "$A2" shot DISK="$R/clients/apple2-tcp/rgbtcp.po" FRAMES=3600 \
            KEYS="wait 450; key RETURN; wait 60; key RETURN; wait 240; type 1; wait 200; $(moves 12 15) button 0 10" \
            > "$OUT/shot_apple2.log" 2>&1
        cp "$A2/build/shot.png" "$OUT/apple2.png" 2>/dev/null
        cp "$A2/build/shot.txt" "$OUT/apple2.txt" 2>/dev/null ;;
    *)
        echo "unknown client: $c (atari c64 cpc apple2)"; continue ;;
    esac
    endbot
    sleep 2
done

kill "$SRV" 2>/dev/null; wait "$SRV" 2>/dev/null

# the verdict from the server log, per client (its name in the HELLO line)
echo "=== result (server log: $OUT/server.log)"
fail=0
for c in $CLIENTS; do
    case $c in
        atari) who=ATARI/ATARIXL ;; c64) who=C64/C64FUJINET ;; cpc) who=CPC/SCHNEIDERCPC ;; apple2) who=APPLE2/APPLE2 ;; *) continue ;;
    esac
    con=$(grep -c "\[+\] $who connected" "$OUT/server.log")
    start=$(grep -c "GAME START.*$who" "$OUT/server.log")
    mv=$(grep -c "\[move\] $who:" "$OUT/server.log")
    end=$(grep -A200 "GAME START.*$who" "$OUT/server.log" | grep -m1 'GAME END' | sed 's/.* - //')
    if [ "$con" -gt 0 ] && [ "$start" -gt 0 ] && [ "$mv" -gt 0 ] && [ -n "$end" ]; then r=PASS; else r=FAIL; fail=1; fi
    printf '%-6s %s  connected %s, match %s, moves %s, end: %s\n' "$c" "$r" "$con" "$start" "$mv" "${end:--}"
done
exit $fail

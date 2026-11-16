#!/usr/bin/env bash
# Hands-free test of the four C clients: nobody at the keyboard, no window.
#
#   tools/handsfree_test.sh [--b2b] [atari] [c64] [cpc] [apple2]      (default: all four, in that order)
#
# Starts the server headless on 127.0.0.1:$RGB_PORT (default 6602, HTTP $RGB_HTTP_PORT 8082) with its
# own statistics and log in build/handsfree/ (the event's game_results.csv is not touched). For each
# client: builds it for that server, starts a bot (bots/game_bot.sh) as the opponent and runs the
# client in its development kit's `make shot` with a key script: RETURN (server), RETURN (PIN), 1
# (the first game, Tron), then joystick moves. At the end it checks the server log: did the client
# connect, did the match start and end, did its moves arrive. Exit status 0 = all passed.
#
# --b2b ("back-to-back"): two matches in a row per client, both joined as the SECOND player - the bot
# is already waiting, so START comes right after GAME 1, without a WAIT in between. The first bot
# connects before the client; after the first match the client plays again (any key, RETURN for
# the PIN, 1) against a second bot, which starts as soon as the server log shows the first match's
# GAME END. PASS needs the connection order bot, client, bot, client, two matches with an end, and
# moves from the client in the second match.
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
B2B=0
if [ "${1:-}" = "--b2b" ]; then B2B=1; shift; fi
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

BOTS=""
bot() {   # bot <delay s> <name>: the Tron opponent, started when the client is about to pick its game
    if [ "$B2B" = 1 ]; then   # waiting before the client comes; a second one for the second match
        ( exec bash bots/game_bot.sh 127.0.0.1 "$PORT" ATARI "$2" ATARI TRON 6 "" 4 ) > "$OUT/bot_$2.log" 2>&1 &
        BOTS="$BOTS $!"
        local ends
        ends=$(grep -c 'GAME END' "$OUT/server.log")
        ( while [ "$(grep -c 'GAME END' "$OUT/server.log")" -le "$ends" ]; do sleep 0.5; done
          exec bash bots/game_bot.sh 127.0.0.1 "$PORT" ATARI "${2}2" ATARI TRON 6 "" 4 ) > "$OUT/bot_${2}2.log" 2>&1 &
        BOTS="$BOTS $!"
        return
    fi
    ( sleep "$1"; exec bash bots/game_bot.sh 127.0.0.1 "$PORT" ATARI "$2" ATARI TRON 6 "" 4 ) > "$OUT/bot_$2.log" 2>&1 &
    BOTS="$BOTS $!"
}
endbot() {
    local b
    for b in $BOTS; do pkill -P "$b" 2>/dev/null; kill "$b" 2>/dev/null; wait "$b" 2>/dev/null; done
    BOTS=""
}

again() {   # again <keys of one round>: with --b2b, play once more (any key, then the round again)
    [ "$B2B" = 1 ] && echo "; wait 50; key RETURN; wait 50; $1"
}
frames() {  # frames <normal> <b2b>
    if [ "$B2B" = 1 ]; then echo "$2"; else echo "$1"; fi
}

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
        ROUND="key RETURN; wait 200; type 1; wait 150; $(moves 12 12) joy fire 10"
        bot 14 BotAtari
        make -C "$A8" shot NETSIO_PORT="$A8_NETSIO" WEB_PORT="$A8_WEB" XEX="$R/clients/atari-tcp/rgbtcp.xex" FRAMES="$(frames 3000 3400)" \
            KEYS="wait 300; key RETURN; wait 50; $ROUND$(again "$ROUND")" \
            > "$OUT/shot_atari.log" 2>&1
        # make shot wrote the test NetSIO port into the kit's fnconfig.ini: back to the default
        sed -i.bak '/^\[BOIP\]/,/^\[/ s/^port=[0-9]*/port=9997/' "$A8/opt/fujinet-pc/fnconfig.ini" && rm -f "$A8/opt/fujinet-pc/fnconfig.ini.bak"
        cp "$A8/build/shot.png" "$OUT/atari.png" 2>/dev/null ;;
    c64)
        make -C clients/c64-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_c64.log" 2>&1 || { tail "$OUT/make_c64.log"; continue; }
        ROUND="key RETURN; wait 200; type 1; wait 150; $(moves 12 12) joy fire 10"
        bot 26 BotC64
        make -C "$C64" shot PRG="$R/clients/c64-tcp/rgbtcp.prg" FRAMES="$(frames 3000 4000)" \
            KEYS="wait 900; key RETURN; wait 50; $ROUND$(again "$ROUND")" \
            > "$OUT/shot_c64.log" 2>&1
        make -C "$C64" stop > /dev/null 2>&1
        cp "$C64/build/shot.png" "$OUT/c64.png" 2>/dev/null
        cp "$C64/build/shot.txt" "$OUT/c64.txt" 2>/dev/null ;;
    cpc)
        make -C clients/cpc-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_cpc.log" 2>&1 || { tail "$OUT/make_cpc.log"; continue; }
        mkdir -p "$CPC/m4sd" && cp clients/cpc-tcp/RGBTCP.BIN "$CPC/m4sd/"
        ROUND="key RETURN; wait 300; key 1; wait 250; $(moves 12 16) joy fire 8"
        bot 16 BotCPC
        make -C "$CPC" shot CMD='RUN"RGBTCP"' FRAMES="$(frames 2500 3600)" \
            KEYS="wait 200; key RETURN; wait 50; $ROUND$(again "$ROUND")" \
            > "$OUT/shot_cpc.log" 2>&1
        make -C "$CPC" stop > /dev/null 2>&1
        cp "$CPC/build/shot.png" "$OUT/cpc.png" 2>/dev/null ;;
    apple2)
        make -C clients/apple2-tcp SERVER=127.0.0.1 PORT="$PORT" > "$OUT/make_apple2.log" 2>&1 || { tail "$OUT/make_apple2.log"; continue; }
        ROUND="key RETURN; wait 240; type 1; wait 200; $(moves 12 15) button 0 10"
        bot 16 BotApple
        make -C "$A2" shot DISK="$R/clients/apple2-tcp/rgbtcp.po" FRAMES="$(frames 3600 4800)" \
            KEYS="wait 450; key RETURN; wait 60; $ROUND$(again "$ROUND")" \
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
    if [ "$B2B" = 1 ]; then
        case $c in atari) b=BotAtari ;; c64) b=BotC64 ;; cpc) b=BotCPC ;; apple2) b=BotApple ;; esac
        # B = one of this client's bots connected, C = the client; matches, ends, client moves in match 2
        set -- $(awk -v who="$who" -v bot="ATARI/$b" '
            index($0, who " connected") { seq = seq "C" }
            index($0, bot) && / connected / { seq = seq "B" }
            /GAME START/ && index($0, who) { s++; inm = 1 }
            /GAME END/ && inm { e++; inm = 0 }
            /\[move\]/ && index($0, who ":") && s == 2 { m2++ }
            END { print (seq == "" ? "-" : seq), s + 0, e + 0, m2 + 0 }' "$OUT/server.log")
        if [ "$1" = BCBC ] && [ "$2" = 2 ] && [ "$3" = 2 ] && [ "$4" -gt 0 ]; then r=PASS; else r=FAIL; fail=1; fi
        printf '%-6s %s  b2b: connections %s (B=bot C=client), matches %s, ends %s, moves in match 2 %s\n' "$c" "$r" "$1" "$2" "$3" "$4"
        continue
    fi
    printf '%-6s %s  connected %s, match %s, moves %s, end: %s\n' "$c" "$r" "$con" "$start" "$mv" "${end:--}"
done
exit $fail

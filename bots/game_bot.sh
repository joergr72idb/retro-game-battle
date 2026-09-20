#!/usr/bin/env bash
#
# game_bot.sh - simulates a retro Game Battle client over TCP.
#
# Extends retro-tron-battle/bots/tron_bot.sh with the new game-selection
# step: on "WAIT GAMES <n> <id1>:<label1> ..." (first connection) or
# "ERR INCOMPATIBLE GAMES <n> ..." (pairing-time rejection), picks a game
# by id (not menu position, so it survives reordering) and sends
# "GAME <n>". Everything from START onward is identical to tron_bot.sh.
#
# Usage:
#   ./game_bot.sh [HOST] [PORT] [PLATFORM] [NAME] [VARIANT] [WANT_GAME] [TURN_CHANCE] [PIN] [FIRE_CHANCE]
#
# WANT_GAME: a game id (e.g. TRON, SNAKE, PONG, ARTILLERY) to select
# whenever offered a choice - if empty, picks whatever's offered first. If
# WANT_GAME is set but never appears in an offered list, the bot gives up
# (useful for testing compatibility filtering: e.g. pass
# VARIANT=C64-WIC64 WANT_GAME=SNAKE to confirm Snake is never even offered
# to a WiC64-variant client, or WANT_GAME=ARTILLERY to confirm it IS).
#
# FIRE_CHANCE: for ARTILLERY - roughly 1-in-N idle polls sends "FIRE" (in
# addition to the existing random MOVE turns, which double as angle/power
# adjustments for this game - see games/artillery.py). Harmless no-op for
# every other game, and harmless when it's not this bot's turn either
# (the server just discards it) - so it's safe to always pass a value even
# when playing Tron/Snake/Pong.
#
# Examples:
#   ./game_bot.sh 127.0.0.1 6502 ATARI BotA ATARI TRON
#   ./game_bot.sh 127.0.0.1 6502 C64 BotB C64-WIC64 SNAKE      # should be refused SNAKE
#   ./game_bot.sh 127.0.0.1 6502 C64 BotC C64-WIC64 ARTILLERY 12 "" 8  # should work fine
#
# NOTE: the server pairs the first compatible connections it sees for a
# given game. Start only ONE bot if you want it to wait for a real client.

set -uo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-6502}"
PLATFORM="${3:-C64}"
NAME="${4:-Bot}"
VARIANT="${5:-$PLATFORM}"
WANT_GAME="${6:-}"
TURN_CHANCE="${7:-12}"
PIN="${8:-}"
FIRE_CHANCE="${9:-0}"   # 0 = never fire (fine for every game except Artillery)
POLL_INTERVAL="0.2"

if ! command -v nc >/dev/null 2>&1; then
    echo "This script needs netcat (nc) installed." >&2
    exit 1
fi

echo "[$PLATFORM/$NAME] connecting to $HOST:$PORT (variant=$VARIANT, want=${WANT_GAME:-any}) ..."

coproc NC { nc "$HOST" "$PORT"; }

send() {
    printf '%s\n' "$1" >&"${NC[1]}" 2>/dev/null
}

if [ -n "$PIN" ]; then
    send "HELLO $PLATFORM $NAME $VARIANT $PIN"
else
    send "HELLO $PLATFORM $NAME $VARIANT"
fi

CURDIR="R"
STARTED=0
IDLE_COUNT=0
# 600 * 0.2s = 120s of total silence before giving up. Needs to be generous:
# this protocol deliberately sends the client NOTHING during play (the
# server-side display is the only spectator view - see docs/protocol.md) -
# a long game (e.g. a 5-point Pong match with two non-blocking bots
# measured at ~68s/~274 ticks in testing) is genuinely silent the whole
# time, not stalled.
MAX_IDLE=600

# Parses "n id1:label1 id2:label2 ..." (the tail of a WAIT GAMES / ERR
# INCOMPATIBLE GAMES line) and sends a GAME <n> selection. Returns 1 (and
# prints why) if it decides to give up instead.
handle_game_list() {
    local body="$1"
    local count rest
    read -r count rest <<< "$body"
    local -a ids=()
    local tok
    for tok in $rest; do
        ids+=("${tok%%:*}")
    done

    if [ "${#ids[@]}" -eq 0 ]; then
        echo "[$PLATFORM/$NAME] no games offered at all - giving up"
        return 1
    fi

    local pick_idx=-1
    if [ -n "$WANT_GAME" ]; then
        local i
        for i in "${!ids[@]}"; do
            if [ "${ids[$i]}" = "$WANT_GAME" ]; then
                pick_idx=$i
                break
            fi
        done
        if [ "$pick_idx" -lt 0 ]; then
            echo "[$PLATFORM/$NAME] wanted game $WANT_GAME not offered (${ids[*]}) - giving up"
            return 1
        fi
    else
        pick_idx=0
    fi

    local n=$(( pick_idx + 1 ))
    echo "[$PLATFORM/$NAME] selecting game ${ids[$pick_idx]} (option $n of ${#ids[@]}: ${ids[*]})"
    send "GAME $n"
    return 0
}

while true; do
    if read -r -u "${NC[0]}" -t "$POLL_INTERVAL" line; then
        line="${line%$'\r'}"
        IDLE_COUNT=0
        case "$line" in
            "WAIT GAMES"*)
                body="${line#WAIT GAMES }"
                if ! handle_game_list "$body"; then break; fi
                ;;
            "ERR INCOMPATIBLE GAMES"*)
                body="${line#ERR INCOMPATIBLE GAMES }"
                echo "[$PLATFORM/$NAME] incompatible with the waiting game - picking another"
                if ! handle_game_list "$body"; then break; fi
                ;;
            "ERR TOO MANY INVALID SELECTIONS"|"ERR UNKNOWN SESSION"|"ERR UNKNOWN REQUEST")
                echo "[$PLATFORM/$NAME] $line - giving up"
                break
                ;;
            WAIT)
                echo "[$PLATFORM/$NAME] chosen - waiting for an opponent..."
                ;;
            START*)
                # START <game-id> <w> <h> <x1> <y1> <x2> <y2> <yourplayernum> <p1plat> <p2plat>
                read -r _ GID W H X1 Y1 X2 Y2 PNUM P1PLAT P2PLAT <<< "$line"
                if [ "$PNUM" = "1" ]; then CURDIR="R"; else CURDIR="L"; fi
                STARTED=1
                echo "[$PLATFORM/$NAME] game start! playing $GID as player $PNUM"
                ;;
            "END WIN"*)
                echo "[$PLATFORM/$NAME] $line"
                STARTED=0
                ;;
            "END DRAW")
                echo "[$PLATFORM/$NAME] draw game."
                STARTED=0
                ;;
            STATS*)
                echo "[$PLATFORM/$NAME] $line"
                break
                ;;
            *)
                echo "[$PLATFORM/$NAME] < $line"
                ;;
        esac
    else
        IDLE_COUNT=$((IDLE_COUNT + 1))
        if [ "$IDLE_COUNT" -ge "$MAX_IDLE" ]; then
            echo "[$PLATFORM/$NAME] no response from server for a while, giving up."
            break
        fi
        if [ "$STARTED" -eq 1 ] && [ $(( RANDOM % TURN_CHANCE )) -eq 0 ]; then
            case "$CURDIR" in
                U) OPTS=(D L R) ;;
                D) OPTS=(U L R) ;;
                L) OPTS=(U D R) ;;
                R) OPTS=(U D L) ;;
                *) OPTS=(U D L R) ;;
            esac
            NEWDIR="${OPTS[$(( RANDOM % ${#OPTS[@]} ))]}"
            CURDIR="$NEWDIR"
            send "MOVE $NEWDIR"
        fi
        if [ "$STARTED" -eq 1 ] && [ "$FIRE_CHANCE" -gt 0 ] && [ $(( RANDOM % FIRE_CHANCE )) -eq 0 ]; then
            send "FIRE"
        fi
    fi
done

if [ -n "${NC[1]:-}" ]; then
    send "BYE" 2>/dev/null || true
    exec {NC[1]}>&- 2>/dev/null || true
fi
if [ -n "${NC_PID:-}" ]; then
    wait "$NC_PID" 2>/dev/null || true
fi
echo "[$PLATFORM/$NAME] disconnected."

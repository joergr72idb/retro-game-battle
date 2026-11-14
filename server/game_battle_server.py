#!/usr/bin/env python3
"""
Retro Game Battle Server
=========================

Multi-game successor to retro-tron-battle/server/tron_server.py: same
retro-computer-steers-over-the-network design, same two participation
methods (raw TCP - used by every client in this repo - and HTTP polling,
kept as a bridge for slow add-ons like the WiC64), but the first connecting client now picks which
game to play from a server-provided list (see docs/protocol.md), and new
games can be added as plugins under games/ without touching this file or
the transport/pairing code under engine/.

Run with:
    python3 game_battle_server.py [host] [tcp_port] [http_port]
Defaults: 0.0.0.0 6502 8080
"""

import asyncio
import sys
import threading

import config
from engine.model import load_stats_from_csv
from engine.net_tcp import handle_client
from engine.net_http import handle_http_connection
from games import GAME_ORDER


async def server_main(host: str, port: int, http_port: int):
    tcp_server = await asyncio.start_server(handle_client, host, port)
    http_server = await asyncio.start_server(handle_http_connection, host, http_port)
    addrs = ", ".join(str(sock.getsockname()) for sock in tcp_server.sockets)
    http_addrs = ", ".join(str(sock.getsockname()) for sock in http_server.sockets)
    print(f"Retro Game Battle server (build {config.SERVER_BUILD}) listening on {addrs}")
    print(f"HTTP-polling bridge listening on {http_addrs}")
    print(f"Games available: {', '.join(GAME_ORDER)}")
    print("Waiting for the first challenger to connect and pick a game...")
    async with tcp_server, http_server:
        await asyncio.gather(tcp_server.serve_forever(), http_server.serve_forever())


def network_thread(host: str, port: int, http_port: int):
    try:
        asyncio.run(server_main(host, port, http_port))
    except Exception as e:
        print(f"[server thread] fatal error: {e}")


def main(host: str, port: int, http_port: int, headless: bool = False):
    load_stats_from_csv()
    t = threading.Thread(target=network_thread, args=(host, port, http_port), daemon=True)
    t.start()
    if headless:
        # No display - just keep the process alive; used for automated/bot testing.
        try:
            t.join()
        except KeyboardInterrupt:
            pass
    else:
        from display.pygame_loop import pygame_loop
        pygame_loop()  # blocks on the main thread until the window is closed


if __name__ == "__main__":
    h = sys.argv[1] if len(sys.argv) > 1 else "0.0.0.0"
    p = int(sys.argv[2]) if len(sys.argv) > 2 else 6502
    hp = int(sys.argv[3]) if len(sys.argv) > 3 else 8080
    headless = "--headless" in sys.argv
    try:
        main(h, p, hp, headless=headless)
    except KeyboardInterrupt:
        print("\nServer stopped.")

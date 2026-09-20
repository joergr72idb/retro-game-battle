"""
Raw-TCP transport (Atari/FujiNet). Ported from
retro-tron-battle/server/tron_server.py's handle_client(), extended to
parse the new VARIANT field: `HELLO <PLATFORM> <NAME> <VARIANT> [PIN]`.
"""

from engine.model import read_line, display_name_for, LineBuffer
from engine.pairing import join


async def handle_client(reader, writer):
    peer = writer.get_extra_info("peername")
    # A persistent buffer, passed through to join()/the new Player's inbuf -
    # any bytes beyond the HELLO line that arrived in the same read (e.g. a
    # fast client that sends HELLO and its first GAME selection back to
    # back) must survive past this call, see read_line()'s docstring.
    inbuf = LineBuffer()
    hello = await read_line(reader, timeout=20, buf=inbuf)
    if not hello or not hello.upper().startswith("HELLO"):
        writer.close()
        return

    parts = hello.split()
    platform = parts[1].upper() if len(parts) > 1 else "UNKNOWN"
    name = parts[2] if len(parts) > 2 else platform
    variant = parts[3] if len(parts) > 3 else ""
    pin = parts[4] if len(parts) > 4 else ""
    name = display_name_for(name, pin)

    await join(platform, name, variant, pin, reader, writer, peer_desc=str(peer), inbuf=inbuf)

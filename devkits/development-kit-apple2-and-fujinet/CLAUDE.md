# CLAUDE.md - Development kit for the Apple II and FujiNet

Toolchain only - real programs live in their own project folders and point `DEV` here.

- `tools/setup-macos.sh` / `tools/setup-linux.sh` build everything into `opt/` (keep their pinned versions in step). ProDOS comes from GitHub raw
  (ProDOS-8/ProDOS8-Releases): `releases.prodos8.com` has a certificate for *.github.io only -
  don't switch certificate checks off for it. ProDOS has no licence statement: never commit it or a
  disk image containing it.
- Start the emulator with `XDG_CONFIG_HOME`/`XDG_DATA_HOME` in `build/` (`make run`), else it writes
  to ~/.config and ~/.local/share.
- cadius: `EXTRACTFILE` mirrors absolute output paths under the current folder and appends
  `#type+aux` to names; extracted files get a 1924 date (touch them). `ADDFILE` takes type/aux from a
  `#FF2000` name suffix. Boot blocks are not written by `CREATEVOLUME` - dd them into block 0-1.
- `make run` (the GNOME app) can't be driven from outside. For checks use `make shot DISK= KEYS= FRAMES=`
  (tools/a2shot.c on FujiNet Go's libapple2session.a from build/fngo): headless, boots the disk without CONFIG
  ([Mount1] + configenabled=0 written into build/shot-data/fujinet/fnconfig.ini), then read build/shot.txt /
  build/shot.png / build/shot.fujinet.log. Tested 2026-11-15 with a key-echo disk: text, CTRL, joystick, buttons.
- a2shot details: the session library chdir()s - pass absolute paths. Its emulator starts with Caps Lock on
  (unshifted letters come out upper case, Shift gives lower case): `type` sends lower case with Shift. The text dump
  shows MouseText line characters as letters (e.g. "S"). Ports 1985/64001 are fixed (refuses while make run is open).
  60 frames = 1 s; ProDOS + a SYS file from the SD card are up after ~400 frames.
- `make shot` also writes build/shot.wav (a2shot pulls apple2session_render_audio in a thread; with
  enable_audio=0 the core still fills its buffer). `PEEK=addr:len` [+ `PEEK_EVERY=N`] prints memory at the
  end / every N frames (apple2debug_read) - addresses from the map file; recompute after every change of BSS.
- samples/demo (2026-11-17): ProDOS boot disk build/demo.po (contains ProDOS: build/ only, never commit).
  60 fps checked by its frame counter (_fx_mask + 885). Mixed-mode text gets colour fringes in this emulator:
  inverse text is unreadable there. `$C019` bit 7 = 0 during the vertical blank (//e) works in the emulator.

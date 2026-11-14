# CLAUDE.md - Development kit for the Apple II and FujiNet

Toolchain only - real programs live in their own project folders and point `DEV` here.

- `tools/setup.sh` builds everything into `opt/`. ProDOS comes from GitHub raw
  (ProDOS-8/ProDOS8-Releases): `releases.prodos8.com` has a certificate for *.github.io only -
  don't switch certificate checks off for it. ProDOS has no licence statement: never commit it or a
  disk image containing it.
- Start the emulator with `XDG_CONFIG_HOME`/`XDG_DATA_HOME` in `build/` (`make run`), else it writes
  to ~/.config and ~/.local/share.
- cadius: `EXTRACTFILE` mirrors absolute output paths under the current folder and appends
  `#type+aux` to names; extracted files get a 1924 date (touch them). `ADDFILE` takes type/aux from a
  `#FF2000` name suffix. Boot blocks are not written by `CREATEVOLUME` - dd them into block 0-1.
- The emulator can't be driven from outside: the user boots disks in CONFIG. Check network code via
  the game server's log or `build/xdg-data/fujinet-go-apple2/fujinet/fujinet-console.log`.

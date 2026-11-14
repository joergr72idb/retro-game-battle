"""Lets the kit's FujiNet-PC boot a file from its SD card instead of CONFIG, like a real FujiNet
with that file mounted in drive slot 1.

  python3 fn_boot.py <fnconfig.ini> /rgbtcp.xex    boot /rgbtcp.xex (SD card = host slot 1)
  python3 fn_boot.py <fnconfig.ini> --off          back to FujiNet's CONFIG

fnconfig.ini uses CR LF line endings, which are kept.
"""
import sys


def main():
    path, target = sys.argv[1], sys.argv[2]
    lines = open(path, newline="").read().replace("\r\n", "\n").split("\n")

    # drop an existing [Mount1] section
    out, skip = [], False
    for line in lines:
        if line.startswith("["):
            skip = line == "[Mount1]"
        if not skip:
            out.append(line)
    while out and out[-1] == "":
        out.pop()

    boot = target != "--off"
    out = ["configenabled=%d" % (0 if boot else 1) if l.startswith("configenabled=") else l for l in out]
    if boot:
        out += ["", "[Mount1]", "hostslot=1", "path=" + target, "mode=r"]
    open(path, "w", newline="").write("\r\n".join(out) + "\r\n")


main()

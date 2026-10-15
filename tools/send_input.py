#!/usr/bin/env python3
"""Send mouse and keyboard input to the game, from inside its Wine prefix (send_input.exe).

send_input.py STEP...
  move X Y        the cursor to (X, Y) in the game window's client pixels
  click | rclick  the left or right mouse button, down and up
  key NAME        a key down and up: Return, Escape, space, Tab, Up, Down, Left, Right, a letter or digit
  wait S          sleep S seconds (a float)
  where           print the cursor's position in the window, and whether the window is in front

Under Xwayland, X11's XTest does not reach the game; send_input.exe (proxy/send_input.c, built by
build_proxy.py) calls SendInput inside the GOG prefix, run with Proton's own wine binary, which
joins the running game's wineserver. Only the GOG prefix (section 0 of the doc) is used.
"""
import os
import subprocess
import sys

PROTON_BIN = os.path.expanduser("~/.steam/debian-installation/steamapps/common/Proton - Experimental/files/bin")
PREFIX = os.path.expanduser("~/torchlight-gog/compat/pfx")
EXE = os.path.expanduser("~/torchlight-gog/build/proxy/send_input.exe")
TITLE = "Torchlight"
KEYS = {"Return": 13, "Escape": 27, "space": 32, "Tab": 9, "BackSpace": 8,
        "Left": 37, "Up": 38, "Right": 39, "Down": 40}


def vk(name):
    if name in KEYS:
        return KEYS[name]
    if len(name) == 1 and name.isalnum():
        return ord(name.upper())
    raise ValueError(f"unknown key {name!r}")


def translate(steps):
    """send_input.exe's arguments from this script's steps; raises ValueError on a bad one."""
    out, i = [], 0
    while i < len(steps):
        verb = steps[i]
        if verb == "move" and i + 2 < len(steps):
            out += ["move", str(int(steps[i + 1])), str(int(steps[i + 2]))]
            i += 3
        elif verb in ("click", "rclick", "where"):
            out.append(verb)
            i += 1
        elif verb == "key" and i + 1 < len(steps):
            out += ["key", str(vk(steps[i + 1]))]
            i += 2
        elif verb == "wait" and i + 1 < len(steps):
            out += ["wait", str(int(float(steps[i + 1]) * 1000))]
            i += 2
        else:
            raise ValueError(f"bad step at {verb!r}")
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if "compatdata/41500" in PREFIX:
        print("refusing: that is Steam's Torchlight prefix")
        return 1
    args = translate(sys.argv[1:])
    env = dict(os.environ, WINEPREFIX=PREFIX, WINEDEBUG="-all", WINEDLLOVERRIDES="winemenubuilder.exe=d")
    r = subprocess.run([os.path.join(PROTON_BIN, "wine"), EXE, TITLE] + args, env=env)
    if r.returncode:
        print(f"send_input.exe exited with {r.returncode} (1: no window titled {TITLE!r}; 3: a bad step)")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())

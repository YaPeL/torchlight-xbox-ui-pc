#!/usr/bin/env python3
"""Build and install the proxy CEGUIBase.dll (proxy/cegui_proxy.c) for a copy of the PC game.

build_proxy.py build REAL_DLL BUILD_DIR   the .def from REAL_DLL's exports, then clang-cl and lld-link
                                          into BUILD_DIR/CEGUIBase.dll (BUILD_DIR outside this repo)
build_proxy.py install GAME_DIR PROXY     CEGUIBase.dll -> CEGUIBase_orig.dll (once), PROXY copied in
build_proxy.py remove GAME_DIR            the real CEGUIBase.dll put back
build_proxy.py status GAME_DIR            which CEGUIBase.dll the folder holds

The export table is read from the user's own DLL at build time and is not kept in the repository.
Every export is forwarded by name, with its own ordinal, to CEGUIBase_orig.dll, except the members
in HOOKS, which go to the proxy's thunks. Only use GAME_DIR on a copy of the game (GOG's, in its own
prefix), never Steam's install.
"""
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_info import PE  # noqa: E402

REAL = "CEGUIBase_orig"
LLVM = "/usr/lib/llvm-21/bin"
# Mangled member -> the thunk's __fastcall symbol (@name@bytes of arguments, ecx and edx included).
HOOKS = {
    "?loadScheme@SchemeManager@CEGUI@@QAEPAVScheme@2@ABVString@2@0@Z": "@hook_load_scheme@16",
    "?loadWindowLayout@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@_N@Z": "@hook_load_layout2@16",
    "?loadWindowLayout@WindowManager@CEGUI@@QAEPAVWindow@2@ABVString@2@00P6A_NPAV32@AAV42@2PAX@Z3@Z":
        "@hook_load_layout5@28",
}
# The kernel32 functions the proxy imports, with their stdcall argument bytes.
KERNEL32 = {"LoadLibraryA": 4, "GetProcAddress": 8, "GetModuleFileNameA": 12, "CreateFileA": 28,
            "WriteFile": 20, "GetTickCount": 0, "DisableThreadLibraryCalls": 4}


def proxy_def(exports):
    """The .def text for the proxy, from [(name, ordinal)] of the real DLL."""
    names = {n for n, _ in exports}
    missing = [h for h in HOOKS if h not in names]
    if missing:
        raise ValueError(f"the real DLL does not export {missing}")
    lines = ["LIBRARY CEGUIBase", "EXPORTS"]
    for name, ordinal in exports:
        target = HOOKS.get(name, f"{REAL}.{name}")
        lines.append(f"    {name}={target} @{ordinal}")
    return "\n".join(lines) + "\n"


def import_def(dll, funcs):
    return f"LIBRARY {dll}\nEXPORTS\n" + "".join(f"{f}@{n}\n" for f, n in funcs.items())


def is_proxy(path):
    """True if the DLL at path forwards to CEGUIBase_orig (it is this proxy)."""
    with open(path, "rb") as f:
        return (REAL + ".").encode() in f.read()


def inside(path, root):
    path, root = os.path.realpath(path), os.path.realpath(root)
    return path == root or path.startswith(root + os.sep)


def build(real_dll, out):
    repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    if inside(out, repo):
        raise SystemExit(f"{out} is inside this repository; build elsewhere.")
    with open(real_dll, "rb") as f:
        pe = PE(f.read())
    if is_proxy(real_dll):
        raise SystemExit(f"{real_dll} is the proxy, not the real DLL")
    os.makedirs(out, exist_ok=True)
    src = os.path.join(repo, "proxy", "cegui_proxy.c")
    with open(os.path.join(out, "proxy.def"), "w") as f:
        f.write(proxy_def(pe.exports()))
    with open(os.path.join(out, "kernel32.def"), "w") as f:
        f.write(import_def("kernel32.dll", KERNEL32))
    steps = [
        [f"{LLVM}/llvm-dlltool", "-m", "i386", "-k", "-d", "kernel32.def", "-l", "kernel32.lib"],
        ["clang-cl", "--target=i686-pc-windows-msvc", "/c", "/O2", "/GS-", "/W3", "/clang:-ffreestanding", "/Focegui_proxy.obj", src],
        ["lld-link", "/dll", "/machine:x86", "/nodefaultlib", "/entry:DllMain", "/safeseh:no",
         "/def:proxy.def", "cegui_proxy.obj", "kernel32.lib", "/out:CEGUIBase.dll"],
    ]
    for step in steps:
        subprocess.run(step, cwd=out, check=True)
    print(f"built {os.path.join(out, 'CEGUIBase.dll')}: {len(pe.exports())} exports, {len(HOOKS)} hooked")


def install(game, proxy):
    dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, REAL + ".dll")
    if not os.path.exists(orig):
        if is_proxy(dll):
            raise SystemExit(f"{dll} is a proxy and {orig} is missing: restore the real DLL first")
        os.rename(dll, orig)
    elif not is_proxy(dll):
        raise SystemExit(f"both {dll} and {orig} are real DLLs; look before overwriting")
    shutil.copyfile(proxy, dll)
    print(f"installed the proxy in {game}")


def remove(game):
    dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, REAL + ".dll")
    if not os.path.exists(orig):
        print("no proxy installed")
        return
    if not is_proxy(dll):
        raise SystemExit(f"{dll} is not the proxy; not touching it")
    os.replace(orig, dll)
    print(f"restored the real CEGUIBase.dll in {game}")


def status(game):
    dll = os.path.join(game, "CEGUIBase.dll")
    print("proxy" if is_proxy(dll) else "real")


def main():
    cmds = {"build": (build, 2), "install": (install, 2), "remove": (remove, 1), "status": (status, 1)}
    if len(sys.argv) < 2 or sys.argv[1] not in cmds or len(sys.argv) != 2 + cmds[sys.argv[1]][1]:
        print(__doc__)
        return 2
    cmds[sys.argv[1]][0](*sys.argv[2:])
    return 0


if __name__ == "__main__":
    sys.exit(main())

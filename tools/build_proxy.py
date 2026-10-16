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
import re
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
    # Traced (trace=1 in x360_ui.ini), forwarded to the real ones otherwise.
    "?getChild@Window@CEGUI@@QBEPAV12@ABVString@2@@Z": "@hook_get_child@12",
    "?recursiveChildSearch@Window@CEGUI@@QBEPAV12@ABVString@2@@Z": "@hook_child_search@12",
    "?getProperty@PropertySet@CEGUI@@QBE?AVString@2@ABV32@@Z": "@hook_get_property@16",
    "?setText@Window@CEGUI@@QAEXABVString@2@@Z": "@hook_set_text@12",
    "?injectKeyDown@System@CEGUI@@QAE_NI@Z": "@hook_key_down@12",
    "?injectKeyUp@System@CEGUI@@QAE_NI@Z": "@hook_key_up@12",
    "?injectChar@System@CEGUI@@QAE_NI@Z": "@hook_char@12",
}
# The kernel32 functions the proxy imports, with their stdcall argument bytes.
KERNEL32 = {"LoadLibraryA": 4, "GetProcAddress": 8, "GetModuleFileNameA": 12, "CreateFileA": 28,
            "WriteFile": 20, "GetTickCount": 0, "DisableThreadLibraryCalls": 4, "GetModuleHandleA": 4,
            "ReadFile": 20, "CloseHandle": 4, "VirtualProtect": 16,
            "QueryPerformanceCounter": 4, "QueryPerformanceFrequency": 4}
# send_input.exe's imports (proxy/send_input.c, a test helper that runs in the game's prefix).
KERNEL32_EXE = {"GetCommandLineA": 0, "Sleep": 4, "ExitProcess": 4, "GetStdHandle": 4}
USER32 = {"EnumWindows": 8, "GetWindowTextA": 12, "IsWindowVisible": 4, "IsIconic": 4, "GetClientRect": 8, "ShowWindow": 8, "SetForegroundWindow": 4, "ClientToScreen": 8, "SetCursorPos": 8, "SendInput": 12,
          "MapVirtualKeyA": 8, "GetCursorPos": 4, "ScreenToClient": 8, "GetForegroundWindow": 0,
          "GetActiveWindow": 0, "PostMessageW": 16}
# msvcr90's SEH handler, which clang-cl's __try/__except on x86 registers (cdecl: no @bytes).
MSVCR90 = ["_except_handler3"]


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


def referenced_names(src):
    """[(dll, mangled name)] the proxy's source resolves at run time: sym("dll", ...) and need(...)
    (CEGUIBase_orig.dll), with the STD_STRING macro expanded."""
    std = re.search(r'#define STD_STRING "([^"]*)"', src)
    out = []
    for m in re.finditer(r'(?:\bsym\("([^"]+)",|\bneed\()\s*((?:"[^"]*"\s*|STD_STRING\s*)+)\)', src):
        parts = re.findall(r'"[^"]*"|STD_STRING', m.group(2))
        name = "".join(std.group(1) if p == "STD_STRING" else p[1:-1] for p in parts)
        out.append((m.group(1) or REAL + ".dll", name))
    return out


def check_names(src, game_dir, real_dll):
    """The names of referenced_names() that the game folder's DLLs do not export."""
    missing = []
    for dll, name in referenced_names(src):
        path = real_dll if dll == REAL + ".dll" else os.path.join(game_dir, dll)
        with open(path, "rb") as f:
            if name not in {n for n, _ in PE(f.read()).exports()}:
                missing.append(f"{dll}: {name}")
    return missing


def import_def(dll, funcs):
    """An import library's .def: {stdcall name: argument bytes}, or [cdecl names]."""
    if isinstance(funcs, dict):
        return f"LIBRARY {dll}\nEXPORTS\n" + "".join(f"{f}@{n}\n" for f, n in funcs.items())
    return f"LIBRARY {dll}\nEXPORTS\n" + "".join(f"{f}\n" for f in funcs)


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
    with open(src) as f:
        missing = check_names(f.read(), os.path.dirname(os.path.abspath(real_dll)), real_dll)
    if missing:
        raise SystemExit("the proxy names functions these DLLs do not export:\n  " + "\n  ".join(missing))
    with open(os.path.join(out, "proxy.def"), "w") as f:
        f.write(proxy_def(pe.exports()))
    with open(os.path.join(out, "kernel32.def"), "w") as f:
        f.write(import_def("kernel32.dll", {**KERNEL32, **KERNEL32_EXE}))
    with open(os.path.join(out, "user32.def"), "w") as f:
        f.write(import_def("user32.dll", USER32))
    with open(os.path.join(out, "msvcr90.def"), "w") as f:
        f.write(import_def("msvcr90.dll", MSVCR90))
    steps = [
        [f"{LLVM}/llvm-dlltool", "-m", "i386", "-k", "-d", "kernel32.def", "-l", "kernel32.lib"],
        [f"{LLVM}/llvm-dlltool", "-m", "i386", "-d", "msvcr90.def", "-l", "msvcr90.lib"],
        [f"{LLVM}/llvm-dlltool", "-m", "i386", "-k", "-d", "user32.def", "-l", "user32.lib"],
        ["clang-cl", "--target=i686-pc-windows-msvc", "/c", "/O2", "/GS-", "/W3", "/clang:-ffreestanding", "/Focegui_proxy.obj", src],
        ["clang-cl", "--target=i686-pc-windows-msvc", "/c", "/O2", "/GS-", "/W3", "/clang:-ffreestanding", "/Fopad.obj",
         os.path.join(repo, "proxy", "pad.c")],
        ["lld-link", "/dll", "/machine:x86", "/nodefaultlib", "/entry:DllMain", "/safeseh:no",
         "/def:proxy.def", "cegui_proxy.obj", "pad.obj", "kernel32.lib", "user32.lib", "msvcr90.lib", "/out:CEGUIBase.dll"],
    ]
    helper = os.path.join(repo, "proxy", "send_input.c")
    steps += [
        ["clang-cl", "--target=i686-pc-windows-msvc", "/c", "/O2", "/GS-", "/W3", "/clang:-ffreestanding",
         "/Fosend_input.obj", helper],
        ["lld-link", "/subsystem:console", "/machine:x86", "/nodefaultlib", "/entry:start", "/safeseh:no",
         "send_input.obj", "kernel32.lib", "user32.lib", "/out:send_input.exe"],
    ]
    for step in steps:
        subprocess.run(step, cwd=out, check=True)
    print(f"built {os.path.join(out, 'CEGUIBase.dll')}: {len(pe.exports())} exports, {len(HOOKS)} hooked; "
          f"and send_input.exe")


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

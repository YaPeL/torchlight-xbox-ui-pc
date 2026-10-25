#!/usr/bin/env python3
"""Build and install the proxy CEGUIBase.dll (proxy/cegui_proxy.c) for a copy of the PC game.

build_proxy.py build [REAL_DLL] BUILD_DIR [--target gog|steam]
                                          the .def from proxy/cegui_exports.txt, then clang-cl and
                                          lld-link into BUILD_DIR/CEGUIBase.dll (BUILD_DIR outside this
                                          repo); REAL_DLL, when given, is checked first to export
                                          exactly that list, and its folder to hold the target's build
build_proxy.py install GAME_DIR PROXY     CEGUIBase.dll -> CEGUIBase_orig.dll (once), PROXY copied in
build_proxy.py remove GAME_DIR            the real CEGUIBase.dll put back
build_proxy.py status GAME_DIR            which CEGUIBase.dll the folder holds

The export table is proxy/cegui_exports.txt (names and ordinals only, tools/cegui_exports.py).
Every export is forwarded by name, with its own ordinal, to CEGUIBase_orig.dll, except the members
in HOOKS, which go to the proxy's thunks. Only use GAME_DIR on a copy of the game (GOG's, in its own
prefix), never Steam's install.

--target says which Torchlight.exe the proxy is for: gog (build 17870, the default: proxy/*.c as
written) or steam (build 17685: every GOG address in proxy/*.c's code, outside comments and
literals, replaced by its Steam address from proxy/addresses_steam.txt into copies under
BUILD_DIR/src, compiled with X360_TARGET_STEAM defined). The build stops if an address has no
accepted Steam address, or if a Torchlight.exe next to REAL_DLL is another build than the target.
"""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_info import PE  # noqa: E402
import cegui_exports  # noqa: E402
import port_addresses  # noqa: E402

REAL = "CEGUIBase_orig"
LLVM = "/usr/lib/llvm-21/bin"


def llvm(tool):
    """An LLVM tool: version 21's on PATH (Debian and Ubuntu name it with the suffix), else the one on
    PATH, else LLVM 21's folder."""
    for name in [f"{tool}-21", tool]:   # 21: the version the proxy is built and compared with
        if shutil.which(name):
            return shutil.which(name)
    return f"{LLVM}/{tool}"

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
            "ReadFile": 20, "CloseHandle": 4, "VirtualProtect": 16, "VirtualAlloc": 16,
            "QueryPerformanceCounter": 4, "QueryPerformanceFrequency": 4, "DeleteFileA": 4,
            "GetFileSize": 8, "GetProcessHeap": 0, "HeapAlloc": 12, "HeapFree": 12, "GetLocalTime": 4,
            "GetFileAttributesA": 4, "CreateThread": 24, "GetCurrentProcess": 0, "TerminateProcess": 8}
# send_input.exe's imports (proxy/send_input.c, a test helper that runs in the game's prefix).
KERNEL32_EXE = {"GetCommandLineA": 0, "Sleep": 4, "ExitProcess": 4, "GetStdHandle": 4}
USER32 = {"EnumWindows": 8, "GetWindowTextA": 12, "IsWindowVisible": 4, "IsIconic": 4, "GetClientRect": 8, "ShowWindow": 8, "SetForegroundWindow": 4, "ClientToScreen": 8, "SetCursorPos": 8, "SendInput": 12,
          "MapVirtualKeyA": 8, "GetCursorPos": 4, "ScreenToClient": 8, "GetForegroundWindow": 0, "SetWindowLongW": 12, "CallWindowProcW": 20,
          "SetWindowLongA": 12, "CallWindowProcA": 20, "IsWindowUnicode": 4, "GetWindowLongW": 8, "GetWindowLongA": 8,
          "GetActiveWindow": 0, "PostMessageW": 16, "GetAsyncKeyState": 4, "SetCursor": 4,
          "MessageBoxA": 16}
# msvcr90's SEH handler, which clang-cl's __try/__except on x86 registers (cdecl: no @bytes).
MSVCR90 = ["_except_handler3"]


# The builds the proxy can be made for: Torchlight.exe's PE timestamp and the address table.
TARGETS = {"gog": (0x4FFC6815, None), "steam": (0x4B5F7519, "addresses_steam.txt")}
# A table line's status that a build may use: found by two independent ways (port_addresses.py),
# or checked by hand with the evidence written in docs section 16.
ACCEPTED = ("verified", "checked")


def read_table(path):
    """{GOG address: other address} of a table's accepted lines ("GOG OTHER status ..."; # comments)."""
    out = {}
    with open(path) as f:
        for n, line in enumerate(f, 1):
            parts = line.split("#", 1)[0].split()
            if not parts:
                continue
            if len(parts) < 3:
                raise ValueError(f"{path}:{n}: expected GOG OTHER STATUS")
            gog = int(parts[0], 16)
            if gog in out:
                raise ValueError(f"{path}:{n}: {gog:#x} twice")
            if parts[2] in ACCEPTED and parts[1] != "-":
                out[gog] = int(parts[1], 16)
    return out


def substitute(text, table):
    """The C source with each address in its code replaced from table; and the addresses missing."""
    out, last, missing = [], 0, set()
    for start, end, value in port_addresses.addresses_in(text, code_only=True):
        while end < len(text) and text[end] in "uUlL":   # a suffix goes inside: (0x... + x360_reloc)u is no C
            end += 1
        if value not in table:
            missing.add(value)
            continue
        out.append(text[last:start])
        # Plus x360_reloc: Steam's executable has relocations and Windows loads it away from its
        # base (proxy/cegui_proxy.c sets it at start).
        out.append(f"({table[value]:#010x} + x360_reloc)")
        last = end
    out.append(text[last:])
    return "".join(out), missing


def exe_timestamp(folder):
    path = os.path.join(folder, "Torchlight.exe")
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return PE(f.read(4096)).timestamp


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


def check_names(src, game_dir, real_names):
    """The names of referenced_names() not exported: CEGUIBase_orig's by real_names (a set), the
    other DLLs' by the game folder's (not checked without one)."""
    missing = []
    for dll, name in referenced_names(src):
        if dll == REAL + ".dll":
            known = real_names
        elif game_dir:
            with open(os.path.join(game_dir, dll), "rb") as f:
                known = {n for n, _ in PE(f.read()).exports()}
        else:
            continue
        if name not in known:
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


def target_sources(repo, out, target):
    """{name: path} of the proxy's .c files for the target: proxy/ itself for gog; for another
    build, copies under out/src with the addresses substituted (SystemExit if one is missing)."""
    folder = os.path.join(repo, "proxy")
    names = sorted(n for n in os.listdir(folder) if n.endswith(".c"))
    headers = [n for n in os.listdir(folder) if n.endswith(".h")]
    if headers and TARGETS[target][1]:
        # Only the .c files are copied with their addresses replaced (the review: an address in a
        # header would stay GOG's without a word).
        raise SystemExit(f"proxy/ has headers ({', '.join(headers)}): substitute them too before building {target}")
    table_name = TARGETS[target][1]
    if not table_name:
        return {n: os.path.join(folder, n) for n in names}
    table = read_table(os.path.join(folder, table_name))
    src = os.path.join(out, "src")
    os.makedirs(src, exist_ok=True)
    paths, missing = {}, {}
    for n in names:
        with open(os.path.join(folder, n), encoding="utf-8") as f:
            text, lacking = substitute(f.read(), table)
        for v in lacking:
            missing.setdefault(v, []).append(n)
        paths[n] = os.path.join(src, n)
        with open(paths[n], "w", encoding="utf-8") as f:
            f.write("extern unsigned long x360_reloc;   /* tools/build_proxy.py: the executable's move */\n" + text)
    if missing:
        raise SystemExit(f"no accepted {target} address in proxy/{table_name} for:\n  "
                         + "\n  ".join(f"{v:#010x} ({', '.join(fs)})" for v, fs in sorted(missing.items())))
    return paths


def build(real_dll, out, target="gog", exports_list=cegui_exports.LIST):
    repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    if target not in TARGETS:
        raise SystemExit(f"unknown target {target!r}: one of {', '.join(TARGETS)}")
    if inside(out, repo):
        raise SystemExit(f"{out} is inside this repository; build elsewhere.")
    exports = cegui_exports.exports(exports_list)
    if real_dll and not os.path.isfile(real_dll):
        raise SystemExit(f"{real_dll}: no such file")
    game_dir = os.path.dirname(os.path.abspath(real_dll)) if real_dll else None
    if real_dll and is_proxy(real_dll):
        raise SystemExit(f"{real_dll} is the proxy, not the real DLL")
    stamp = exe_timestamp(game_dir) if game_dir else None
    if stamp is not None and stamp != TARGETS[target][0]:
        known = [t for t, (ts, _) in TARGETS.items() if ts == stamp]
        raise SystemExit(f"the Torchlight.exe next to {real_dll} is build {stamp:#x}"
                         + (f" ({known[0]}): pass --target {known[0]}" if known else ", not one the proxy knows"))
    if real_dll:
        diff = cegui_exports.differences(real_dll, exports_list)
        if diff:
            raise SystemExit(f"{real_dll} is not the CEGUIBase.dll the proxy is made for "
                             f"({len(diff)} exports differ from {exports_list}):\n  " + "\n  ".join(diff[:10]))
    os.makedirs(out, exist_ok=True)
    sources = target_sources(repo, out, target)
    src = sources["cegui_proxy.c"]
    menu = sources["x360_menu.c"]
    ui_dump = sources["ui_dump.c"]
    setup = sources["setup.c"]
    missing = []
    for path in (src, menu, ui_dump, setup):
        with open(path) as f:
            missing += check_names(f.read(), game_dir, {n for n, _ in exports})
    if missing:
        raise SystemExit("the proxy names functions these DLLs do not export:\n  " + "\n  ".join(missing))
    with open(os.path.join(out, "proxy.def"), "w") as f:
        f.write(proxy_def(exports))
    with open(os.path.join(out, "kernel32.def"), "w") as f:
        f.write(import_def("kernel32.dll", {**KERNEL32, **KERNEL32_EXE}))
    with open(os.path.join(out, "user32.def"), "w") as f:
        f.write(import_def("user32.dll", USER32))
    with open(os.path.join(out, "msvcr90.def"), "w") as f:
        f.write(import_def("msvcr90.dll", MSVCR90))
    cc = [llvm("clang-cl"), "--target=i686-pc-windows-msvc", "/c", "/O2", "/GS-", "/W3", "/clang:-ffreestanding"]
    if target != "gog":
        cc.append(f"/DX360_TARGET_{target.upper()}")
    steps = [
        [llvm("llvm-dlltool"), "-m", "i386", "-k", "-d", "kernel32.def", "-l", "kernel32.lib"],
        [llvm("llvm-dlltool"), "-m", "i386", "-d", "msvcr90.def", "-l", "msvcr90.lib"],
        [llvm("llvm-dlltool"), "-m", "i386", "-k", "-d", "user32.def", "-l", "user32.lib"],
        cc + ["/Focegui_proxy.obj", src],
        cc + ["/Fopad.obj", sources["pad.c"]],
        cc + ["/Fox360_menu.obj", menu],
        cc + ["/Foui_dump.obj", ui_dump],
        cc + ["/Fosetup.obj", setup],
        [llvm("lld-link"), "/timestamp:0", "/dll", "/machine:x86", "/nodefaultlib", "/entry:DllMain", "/safeseh:no",
         "/def:proxy.def", "cegui_proxy.obj", "pad.obj", "x360_menu.obj", "ui_dump.obj", "setup.obj", "kernel32.lib", "user32.lib", "msvcr90.lib", "/out:CEGUIBase.dll"],
    ]
    helper = sources["send_input.c"]
    steps += [
        cc + ["/Fosend_input.obj", helper],
        [llvm("lld-link"), "/timestamp:0", "/subsystem:console", "/machine:x86", "/nodefaultlib", "/entry:start", "/safeseh:no",
         "send_input.obj", "kernel32.lib", "user32.lib", "/out:send_input.exe"],
    ]
    for step in steps:
        subprocess.run(step, cwd=out, check=True)
    print(f"built {os.path.join(out, 'CEGUIBase.dll')} for {target}: {len(exports)} exports, "
          f"{len(HOOKS)} hooked; and send_input.exe")


def same_file(a, b):
    with open(a, "rb") as fa, open(b, "rb") as fb:
        return fa.read() == fb.read()


def install(game, proxy):
    dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, REAL + ".dll")
    if not is_proxy(proxy):
        raise SystemExit(f"{proxy} is not the proxy; not installing it")
    if not os.path.exists(dll) and os.path.exists(orig):
        shutil.copyfile(orig, dll)              # the proxy's place empty: the real one put back first
    if os.path.exists(orig) and not is_proxy(dll) and same_file(dll, orig):
        os.remove(orig)                         # a store's file check put the real DLL back: one copy
    if not os.path.exists(orig):
        if is_proxy(dll):
            raise SystemExit(f"{dll} is a proxy and {orig} is missing: restore the real DLL first")
        os.rename(dll, orig)
    elif not is_proxy(dll):
        raise SystemExit(f"both {dll} and {orig} are real DLLs; look before overwriting")
    shutil.copyfile(proxy, dll)
    print(f"installed the proxy in {game}")


def remove(game):
    """True once the real DLL is in place (put back, or there all along); False if CEGUIBase.dll
    is something else than the proxy with the real one beside it (then nothing is touched)."""
    dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, REAL + ".dll")
    if os.path.exists(orig) and (not os.path.exists(dll) or same_file(dll, orig)):
        os.replace(orig, dll)                   # missing or put back by a file check: the real one kept
        print(f"restored the real CEGUIBase.dll in {game}")
        return True
    if not os.path.exists(orig):
        print("no proxy installed")
        return os.path.exists(dll) and not is_proxy(dll)
    if not is_proxy(dll):
        raise SystemExit(f"{dll} is not the proxy; not touching it")
    os.replace(orig, dll)
    print(f"restored the real CEGUIBase.dll in {game}")
    return True


def status(game):
    dll = os.path.join(game, "CEGUIBase.dll")
    print("proxy" if is_proxy(dll) else "real")


def main():
    cmds = {"build": (build, (1, 2)), "install": (install, 2), "remove": (remove, 1), "status": (status, 1)}
    args, target = sys.argv[1:], None
    if "--target" in args:
        i = args.index("--target")
        if i + 1 >= len(args):
            print(__doc__)
            return 2
        target = args[i + 1]
        del args[i:i + 2]
    counts = cmds[args[0]][1] if args and args[0] in cmds else ()
    if not args or len(args) - 1 not in (counts if isinstance(counts, tuple) else (counts,)) \
            or (target and args[0] != "build"):
        print(__doc__)
        return 2
    if args[0] == "build":
        build(*([None] if len(args) == 2 else []), *args[1:], target=target or "gog")
    else:
        cmds[args[0]][0](*args[1:])
    return 0


if __name__ == "__main__":
    sys.exit(main())

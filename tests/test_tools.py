"""Tests of the tools, on synthetic data only (no game file)."""
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import unittest.mock
import zipfile
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import build_proxy  # noqa: E402
import cegui_exports  # noqa: E402
import check_no_game_files  # noqa: E402
import save_items  # noqa: E402
import compare_installs  # noqa: E402
import compare_ui  # noqa: E402
import convert_x360_ui  # noqa: E402
import install  # noqa: E402
import stfs  # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "xex"))
import aes128  # noqa: E402
import layout_windows  # noqa: E402
import port_addresses  # noqa: E402
import send_input  # noqa: E402
import steam_guard  # noqa: E402
from pe_info import PE  # noqa: E402


def synthetic_pe(imports, exports, timestamp=1):
    """A minimal PE32 with one section at RVA 0x1000 holding an import and an export table."""
    sec_rva, sec_raw = 0x1000, 0x200
    body = bytearray(0x800)

    def put(off, data):
        body[off:off + len(data)] = data
        return sec_rva + off

    # Strings and hint/name entries from 0x400 on.
    cursor = [0x400]

    def string(s, hint=False):
        data = (b"\0\0" if hint else b"") + s.encode() + b"\0"
        rva = put(cursor[0], data)
        cursor[0] += len(data) + (len(data) & 1)
        return rva

    # Imports: descriptors at 0, thunk arrays at 0x100.
    thunk = 0x100
    desc = 0
    for dll, funcs in imports.items():
        first = sec_rva + thunk
        for f in funcs:
            struct.pack_into("<I", body, thunk, string(f, hint=True))
            thunk += 4
        thunk += 4
        struct.pack_into("<IIIII", body, desc, first, 0, 0, string(dll), first)
        desc += 20
    # Exports: directory at 0x300, arrays after it.
    names = [string(n) for n in exports]
    n = len(exports)
    funcs_rva, names_rva, ords_rva = sec_rva + 0x340, sec_rva + 0x360, sec_rva + 0x380
    for i in range(n):
        struct.pack_into("<I", body, 0x340 + 4 * i, 0x1000)
        struct.pack_into("<I", body, 0x360 + 4 * i, names[i])
        struct.pack_into("<H", body, 0x380 + 2 * i, i)
    struct.pack_into("<IIHHIIIIII", body, 0x300, 0, 0, 0, 0, string("x.dll"), 1, n, n, funcs_rva, names_rva)
    struct.pack_into("<I", body, 0x300 + 36, ords_rva)

    head = bytearray(sec_raw)
    head[0:2] = b"MZ"
    struct.pack_into("<I", head, 0x3C, 0x40)
    head[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHI", head, 0x44, 0x14C, 1, timestamp)
    struct.pack_into("<H", head, 0x40 + 20, 224)
    opt = 0x40 + 24
    struct.pack_into("<H", head, opt, 0x10B)
    struct.pack_into("<I", head, opt + 28, 0x400000)
    struct.pack_into("<I", head, opt + 92, 16)
    struct.pack_into("<II", head, opt + 96, sec_rva + 0x300 if exports else 0, 0x40)
    struct.pack_into("<II", head, opt + 104, sec_rva if imports else 0, 20 * (len(imports) + 1))
    sec = opt + 224
    head[sec:sec + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", head, sec + 8, len(body), sec_rva, len(body), sec_raw)
    return bytes(head + body)


def write(path, data):
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data) if isinstance(data, bytes) else p.write_text(data)


class PETest(unittest.TestCase):
    def test_imports_and_exports(self):
        pe = PE(synthetic_pe({"A.dll": ["f", "g"], "B.dll": ["h"]}, ["one", "two"], timestamp=7))
        self.assertEqual(pe.timestamp, 7)
        self.assertEqual(pe.imports(), {"A.dll": ["f", "g"], "B.dll": ["h"]})
        self.assertEqual(pe.exports(), [("one", 1), ("two", 2)])

    def test_not_pe(self):
        with self.assertRaises(ValueError):
            PE(b"not a pe at all" * 10)


class BuildProxyTest(unittest.TestCase):
    def test_def(self):
        hooked = list(build_proxy.HOOKS)
        exports = [("??0Thing@CEGUI@@QAE@XZ", 1)] + [(h, i + 2) for i, h in enumerate(hooked)]
        text = build_proxy.proxy_def(exports)
        self.assertEqual(text.splitlines(), [
            "LIBRARY CEGUIBase", "EXPORTS",
            "    ??0Thing@CEGUI@@QAE@XZ=CEGUIBase_orig.??0Thing@CEGUI@@QAE@XZ @1"]
            + [f"    {h}={build_proxy.HOOKS[h]} @{i + 2}" for i, h in enumerate(hooked)])

    def test_def_needs_the_hooked_members(self):
        with self.assertRaises(ValueError):
            build_proxy.proxy_def([("??0Thing@CEGUI@@QAE@XZ", 1)])

    def test_referenced_names(self):
        src = ('#define STD_STRING "?$s@std@@"\n'
               'f = sym("OgreMain.dll", "?get@RGM@@QBEABV" STD_STRING "XZ");\n'
               'g = need("??0String@CEGUI@@QAE@PBD@Z");\n')
        self.assertEqual(build_proxy.referenced_names(src), [
            ("OgreMain.dll", "?get@RGM@@QBEABV?$s@std@@XZ"), ("CEGUIBase_orig.dll", "??0String@CEGUI@@QAE@PBD@Z")])

    def test_check_names(self):
        with tempfile.TemporaryDirectory() as game:
            write(os.path.join(game, "Ogre.dll"), synthetic_pe({}, ["?a@@XZ"]))
            src = 'sym("Ogre.dll", "?a@@XZ"); sym("Ogre.dll", "?typo@@XZ"); need("?b@@XZ"); need("?c@@XZ");'
            self.assertEqual(build_proxy.check_names(src, game, {"?b@@XZ"}),
                             ["Ogre.dll: ?typo@@XZ", "CEGUIBase_orig.dll: ?c@@XZ"])
            # Without the game's folder only CEGUIBase_orig's names (the list's) are checked.
            self.assertEqual(build_proxy.check_names(src, None, {"?b@@XZ"}), ["CEGUIBase_orig.dll: ?c@@XZ"])

    def test_the_proxy_source_parses(self):
        src = Path(os.path.dirname(os.path.abspath(__file__)), "..", "proxy", "cegui_proxy.c").read_text()
        names = build_proxy.referenced_names(src)
        self.assertIn(("OgreMain.dll", "?getWorldResourceGroupName@ResourceGroupManager@Ogre@@QBEABV"
                       "?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ"), names)
        self.assertGreaterEqual(len(names), 13)

    def test_import_def(self):
        self.assertEqual(build_proxy.import_def("k.dll", {"F": 4, "G": 0}), "LIBRARY k.dll\nEXPORTS\nF@4\nG@0\n")

    def test_install_and_remove(self):
        with tempfile.TemporaryDirectory() as game, tempfile.TemporaryDirectory() as built:
            dll = os.path.join(game, "CEGUIBase.dll")
            write(dll, b"MZ synthetic real")
            proxy = os.path.join(built, "CEGUIBase.dll")
            write(proxy, b"MZ synthetic proxy CEGUIBase_orig.x")
            build_proxy.install(game, proxy)
            self.assertTrue(build_proxy.is_proxy(dll))
            self.assertEqual(Path(game, "CEGUIBase_orig.dll").read_bytes(), b"MZ synthetic real")
            build_proxy.install(game, proxy)  # again: the real DLL is kept
            self.assertEqual(Path(game, "CEGUIBase_orig.dll").read_bytes(), b"MZ synthetic real")
            build_proxy.remove(game)
            self.assertEqual(Path(dll).read_bytes(), b"MZ synthetic real")
            self.assertFalse(os.path.exists(os.path.join(game, "CEGUIBase_orig.dll")))

    def test_install_after_a_file_check_and_with_the_dll_missing(self):
        with tempfile.TemporaryDirectory() as game, tempfile.TemporaryDirectory() as built:
            dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, "CEGUIBase_orig.dll")
            proxy = os.path.join(built, "CEGUIBase.dll")
            write(proxy, b"MZ synthetic proxy CEGUIBase_orig.x")
            # A store's file check put the real DLL back beside the kept one: install again works.
            write(dll, b"MZ synthetic real")
            write(orig, b"MZ synthetic real")
            build_proxy.install(game, proxy)
            self.assertTrue(build_proxy.is_proxy(dll))
            self.assertEqual(Path(orig).read_bytes(), b"MZ synthetic real")
            # The proxy's place empty: uninstall puts the real one back.
            os.remove(dll)
            self.assertTrue(build_proxy.remove(game))
            self.assertEqual(Path(dll).read_bytes(), b"MZ synthetic real")
            self.assertFalse(os.path.exists(orig))
            # Not a proxy: refused.
            other = os.path.join(built, "other.dll")
            write(other, b"MZ something else")
            with self.assertRaises(SystemExit):
                build_proxy.install(game, other)
            self.assertEqual(Path(dll).read_bytes(), b"MZ synthetic real")

    def test_install_refuses_two_real_dlls(self):
        with tempfile.TemporaryDirectory() as game:
            write(os.path.join(game, "CEGUIBase.dll"), b"MZ synthetic real")
            write(os.path.join(game, "CEGUIBase_orig.dll"), b"MZ synthetic real too")
            with self.assertRaises(SystemExit):
                build_proxy.install(game, os.path.join(game, "CEGUIBase.dll"))
            with self.assertRaises(SystemExit):
                build_proxy.remove(game)


class CompareTest(unittest.TestCase):
    def test_verdicts(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            same = synthetic_pe({"A.dll": ["f"]}, ["e"])
            for root in (a, b):
                write(os.path.join(root, "Same.dll"), same)
            write(os.path.join(a, "game.exe"), synthetic_pe({"A.dll": ["f"]}, [], timestamp=1))
            write(os.path.join(b, "GAME.EXE"), synthetic_pe({"A.dll": ["f", "steam"]}, [], timestamp=2))
            write(os.path.join(a, "onlya.dll"), same)
            write(os.path.join(b, "notes.txt"), "not a binary")
            report = {k: (v, lines) for k, v, lines in compare_installs.compare(a, b)}
            self.assertEqual(set(report), {"same.dll", "game.exe", "onlya.dll"})
            self.assertEqual(report["same.dll"][0], "identical")
            self.assertEqual(report["onlya.dll"][0], "only in A")
            verdict, lines = report["game.exe"]
            self.assertEqual(verdict, "different")
            self.assertTrue(any("link timestamp 1 vs 2" in line for line in lines))
            self.assertTrue(any("only B ['steam']" in line for line in lines))


class LayoutTest(unittest.TestCase):
    XML = (
        '<GUILayout><Window Type="Look/FrameWindow" Name="Root">'
        '<Property Name="Text" Value="x" />'
        '<Window Type="Look/Button" Name="Root/Ok" />'
        '<Window Type="DefaultWindow"><Window Type="Look/StaticText" Name="Deep" /></Window>'
        "</Window></GUILayout>"
    )

    def test_windows(self):
        w = layout_windows.windows(self.XML)
        self.assertEqual(list(w), ["Root", "Root/Ok", "Deep"])
        self.assertEqual(w["Root/Ok"], ("Look/Button", "Root"))
        self.assertEqual(w["Deep"][1], "Root", "an unnamed window's child: the nearest named parent")

    def test_utf16(self):
        self.assertIn("Root", layout_windows.windows(layout_windows.text_of(self.XML.encode("utf-16"))))

    def test_overlap(self):
        have, missing = layout_windows.overlap(layout_windows.windows(self.XML), {"Root": 0, "Deep": 0})
        self.assertEqual((have, missing), (["Root", "Deep"], ["Root/Ok"]))


class ConvertX360Test(unittest.TestCase):
    """A synthetic 360 pak: one scheme with two imagesets (one of them PC's name, `Loading`), one
    font named twice, a looknfeel and two layouts, one naming an imageset the scheme lacks."""
    FILES = {
        "media/UI/hud/schemes/ceguiWidgets.scheme":
            '<?xml version="1.0" ?>\n<GUIScheme Name="newHud">\n'
            '<Imageset Name="widgets1" Filename="media/ui/icons2/widgets1.imageset" />\n'
            '<Imageset Name="Loading" Filename="media/ui/loading.imageset" />\n'
            '<Font Name="Serif" Filename="media/ui/Serif.font" />\n'
            '<Font Name="Serif" Filename="media/ui/Serif.font" />\n'
            '<LookNFeel Filename="media/ui/hud/schemes/ceguiWidgets.looknfeel" />\n'
            '<FalagardMapping WindowType="ceguiWidget/Button" TargetType="CEGUI/PushButton" />\n'
            "</GUIScheme>\n",
        "media/UI/icons2/widgets1.imageset":
            '<Imageset Name= "widgets1" Imagefile="media/ui/icons2/widgets1.dds"><Image Name="a" /></Imageset>',
        "media/UI/icons2/widgets1.dds": b"DDS synthetic 1",
        "media/UI/loading.imageset": '<Imageset Name="Loading" Imagefile="media/ui/loading.dds" />',
        "media/UI/loading.dds": b"DDS synthetic 2",
        "media/UI/Serif.font": '<Font Name="Serif" Filename="media/ui/BRLNSDB.TTF" Type="FreeType" />',
        "media/UI/brlnsdb.ttf": b"synthetic font",
        "media/UI/hud/schemes/ceguiWidgets.looknfeel":
            '<Falagard><Image imageset="widgets1" image="a" />'
            '<Property Name="Image" Value="set:Loading image:loading" />'
            '<Property name="Font" value="Serif" /></Falagard>',
        "media/UI/modalmenu.uilayout":
            '<GUILayout><Window Type="ceguiWidget/StaticImage" Name="Root">'
            '<Property Name="Image" Value="set:widgets1 image:a" /><Property Name="Font" Value="Serif" />'
            '<Property Name="Font" Value="FrizQuadrataSmall" /></Window></GUILayout>',
        "media/ui/skills.uilayout":
            '<GUILayout><Window Type="DefaultWindow" Name="R"><Window Type="DefaultWindow" Name="S">'
            '<Property Name="Image" Value="set:widgets6 image:b" /></Window></Window></GUILayout>',
        "media/UI/hud.layout": "<GUILayout />",
        "media/UI/interactivemenu.layout":
            '<GUILayout><Window Type="DefaultWindow" Name="Root"><Window Type="ceguiWidget/StaticImage" '
            'Name="TopBar"><Property Name="Image" Value="set:Loading image:background" /></Window>'
            '</Window></GUILayout>',
        "media/models/other.dds": b"not UI",
    }

    def pak(self, root):
        path = os.path.join(root, "pak.zip")
        with zipfile.ZipFile(path, "w") as z:
            for name, data in self.FILES.items():
                z.writestr(name, data)
        return path

    def test_convert(self):
        with tempfile.TemporaryDirectory() as root:
            out, report = convert_x360_ui.convert(convert_x360_ui.read_pak(self.pak(root)))
        text = {k: v.decode() for k, v in out.items() if not k.endswith((".dds", ".TTF"))}
        d = convert_x360_ui.DEST
        self.assertEqual(sorted(out), sorted(d + n for n in [
            "BRLNSDB.TTF", "Serif.font", "ceguiWidgets.looknfeel", "ceguiWidgets.scheme", "loading.dds",
            "loading.imageset", "interactivemenu.layout", "modalmenu.layout", "skills.layout", "widgets1.dds", "widgets1.imageset"]))
        scheme = text[d + "ceguiWidgets.scheme"]
        self.assertIn('<GUIScheme Name="X360_newHud">', scheme)
        self.assertIn('<Imageset Name="X360_Loading" Filename="media/ui/x360/loading.imageset" />', scheme)
        self.assertEqual(scheme.count('Filename="media/ui/x360/Serif.font"'), 2)
        self.assertIn('<LookNFeel Filename="media/ui/x360/ceguiWidgets.looknfeel" />', scheme)
        self.assertIn('<Imageset Name= "X360_widgets1" Imagefile="media/ui/x360/widgets1.dds">',
                      text[d + "widgets1.imageset"])
        self.assertIn('Filename="media/ui/x360/BRLNSDB.TTF"', text[d + "Serif.font"])
        self.assertIn('Name="X360_Serif"', text[d + "Serif.font"], "fonts are renamed")
        self.assertEqual(scheme.count('<Font Name="X360_Serif"'), 2)
        self.assertIn('<Property Name="Font" Value="X360_Serif" />', text[d + "modalmenu.layout"])
        self.assertIn('<Property name="Font" value="X360_Serif" />', text[d + "ceguiWidgets.looknfeel"])
        self.assertIn('<Property Name="Font" Value="FrizQuadrataSmall" />', text[d + "modalmenu.layout"],
                      "a font the scheme does not name is left")
        self.assertEqual(report["font_names"], {"Serif": "X360_Serif"})
        self.assertEqual(text[d + "ceguiWidgets.looknfeel"],
                         '<Falagard><Image imageset="X360_widgets1" image="a" />'
                         '<Property Name="Image" Value="set:X360_Loading image:loading" />'
                         '<Property name="Font" value="X360_Serif" /></Falagard>')
        self.assertIn('Value="set:X360_widgets1 image:a"', text[d + "modalmenu.layout"])
        self.assertIn('Value="set:widgets6 image:b"', text[d + "skills.layout"], "unknown sets are left")
        self.assertIn('Value="set:X360_Loading image:background"', text[d + "interactivemenu.layout"],
                      "a .layout of EXTRA_LAYOUTS is converted; hud.layout is not")
        self.assertEqual(out[d + "loading.dds"], b"DDS synthetic 2")
        self.assertEqual(report["layouts"]["skills.layout"], {"windows": 2, "unknown_imagesets": ["widgets6"]})
        self.assertEqual(report["imagesets"], {"widgets1": "X360_widgets1", "Loading": "X360_Loading"})
        self.assertEqual(report["missing_files"], [])

    def test_missing_file_and_utf16(self):
        files = {k.lower(): v.encode() if isinstance(v, str) else v for k, v in self.FILES.items()}
        del files["media/ui/loading.dds"]
        files["media/ui/modalmenu.uilayout"] = self.FILES["media/UI/modalmenu.uilayout"].encode("utf-16")
        out, report = convert_x360_ui.convert(files)
        self.assertEqual(report["missing_files"], ["media/ui/loading.dds"])
        layout = out[convert_x360_ui.DEST + "modalmenu.layout"]
        self.assertEqual(layout[:2], b"\xff\xfe", "a UTF-16 layout stays UTF-16")
        self.assertIn("set:X360_widgets1", layout.decode("utf-16"))

    def test_not_the_360_pak(self):
        with self.assertRaises(ValueError):
            convert_x360_ui.convert({"media/ui/hud.layout": b"<GUILayout />"})

    def test_main_refuses_the_repository(self):
        with tempfile.TemporaryDirectory() as root:
            pak = self.pak(root)
            tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "convert_x360_ui.py")
            repo = os.path.join(os.path.dirname(tool), "..")
            refused = subprocess.run([sys.executable, tool, pak, os.path.join(repo, "out")], capture_output=True)
            self.assertEqual(refused.returncode, 2)
            self.assertFalse(os.path.exists(os.path.join(repo, "out")))
            done = subprocess.run([sys.executable, tool, pak, os.path.join(root, "o")], capture_output=True, text=True)
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertIn("widgets6", done.stdout)
            self.assertFalse(os.path.exists(os.path.join(root, "o", "media", "ui", "x360", "hud.layout")),
                             "the 360 pak's .layout files are not taken")
            self.assertTrue(os.path.isfile(os.path.join(root, "o", "media", "ui", "x360", "modalmenu.layout")))
            self.assertTrue(os.path.isfile(os.path.join(root, "o", "x360_ui_report.json")))


class SendInputTest(unittest.TestCase):
    def test_translate(self):
        self.assertEqual(send_input.translate(["move", "10", "20", "click", "key", "Return", "key", "a", "wait", "0.5",
                                               "where", "rclick"]),
                         ["move", "10", "20", "click", "key", "13", "key", "65", "wait", "500", "where", "rclick"])

    def test_bad_steps(self):
        for steps in (["move", "1"], ["key", "NoSuchKey"], ["jump"], ["wait"]):
            with self.assertRaises(ValueError):
                send_input.translate(steps)


class SteamGuardTest(unittest.TestCase):
    def test_snapshot_and_differences(self):
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, "pfx", "save"))
            write(os.path.join(root, "pfx", "save", "0.svt"), b"synthetic save")
            write(os.path.join(root, "pfx", "settings.txt"), "a")
            os.symlink("save", os.path.join(root, "pfx", "link"))
            before = steam_guard.snapshot([os.path.join(root, "pfx")])
            self.assertEqual(steam_guard.differences(before, steam_guard.snapshot(before.keys())), [])
            write(os.path.join(root, "pfx", "settings.txt"), "b")
            write(os.path.join(root, "pfx", "new.txt"), "x")
            os.remove(os.path.join(root, "pfx", "save", "0.svt"))
            lines = steam_guard.differences(before, steam_guard.snapshot(before.keys()))
            self.assertEqual(len(lines), 3)
            self.assertTrue(any(line.startswith("changed:") and line.endswith("settings.txt") for line in lines))
            self.assertTrue(any(line.startswith("new:") and line.endswith("new.txt") for line in lines))
            self.assertTrue(any(line.startswith("gone:") and line.endswith("0.svt") for line in lines))


class NoGameFilesTest(unittest.TestCase):
    def test_forbidden(self):
        paths = ["docs/a.md", "x/Torchlight.EXE", "m/ui.layout", "m/a.imageset", "t.dds", "data.pak",
                 "c.dll", "media/Pak.zip", "tools/zip.py", "a.uilayout"]
        self.assertEqual(check_no_game_files.forbidden(paths),
                         ["x/Torchlight.EXE", "m/ui.layout", "m/a.imageset", "t.dds", "data.pak", "c.dll",
                          "media/Pak.zip", "a.uilayout"])

    def test_export_list(self):
        """Only proxy/cegui_exports.txt may hold an export table, and only CEGUI's names in its format."""
        name = "?f@Font@" + "CEGUI@@QAEXXZ"          # split: this file must not look like a table
        good = "# comment\n" + f"1 cegui ??0Thing@CEGUI@@QAE@XZ\n2 runic {name}\n"
        table = lambda form: "".join(form.format(i=i, n=name.replace("XZ", f"{i}XZ")) + "\n" for i in range(8))
        with tempfile.TemporaryDirectory() as root:
            write(os.path.join(root, "proxy", "cegui_exports.txt"), good)
            copies = {"copy.md": table("{i} cegui {n}"), "indented.md": table("    {i} cegui {n}"),
                      "tabs.txt": table("{i}\tcegui\t{n}"), "def.txt": table("    {n}=CEGUIBase_orig.{n} @{i}"),
                      "dump.txt": table("{n} @{i}"), "numbered.txt": table("{i} {n}"),
                      "bulk.txt": table("{n}") * 60}
            for f, text in copies.items():
                write(os.path.join(root, "docs", f), text)
            write(os.path.join(root, "docs", "fine.md"), table("a call of {n} in a sentence"))
            paths = ["proxy/cegui_exports.txt", "docs/fine.md"] + [f"docs/{f}" for f in copies]
            self.assertEqual(check_no_game_files.export_problems(root, paths),
                             [f"docs/{f}: export table lines outside proxy/cegui_exports.txt" for f in copies])
            for bad in ("3 cegui ?other@Ogre@@QAEXXZ\n", "3 cegui ?f@Ogre@@QAEXPAVWindow@CEGUI@@@Z\n",
                        "3 game ?x@CEGUI@@QAEXXZ\n", "free text\n", "# a comment after the entries\n",
                        "\n", "1 cegui ?dup@CEGUI@@QAEXXZ\n"):
                write(os.path.join(root, "proxy", "cegui_exports.txt"), good + bad)
                self.assertEqual(len(check_no_game_files.export_problems(root, paths[:1])), 1, bad)
            write(os.path.join(root, "proxy", "cegui_exports.txt"), "#\n" * 11 + good)
            self.assertEqual(len(check_no_game_files.export_problems(root, paths[:1])), 1)
        self.assertTrue(cegui_exports.in_cegui("??8CEGUI@@YA_NABVString@0@0@Z"))
        self.assertTrue(cegui_exports.in_cegui("??0?$RefCounted@VBoundSlot@CEGUI@@@CEGUI@@QAE@XZ"))
        self.assertEqual(check_no_game_files.forbidden(["proxy/proxy.def", "x/kernel32.lib"]),
                         ["proxy/proxy.def", "x/kernel32.lib"])

    def test_committed_export_list(self):
        repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
        entries = cegui_exports.read(os.path.join(repo, "proxy", "cegui_exports.txt"))
        names = {n for n, _, _ in entries}
        self.assertTrue(set(build_proxy.HOOKS) <= names)
        self.assertEqual(sorted(o for _, o, _ in entries), list(range(1, len(entries) + 1)))
        self.assertEqual(check_no_game_files.export_problems(repo, ["proxy/cegui_exports.txt"]), [])

    def test_tracked_repo(self):
        with tempfile.TemporaryDirectory() as root:
            subprocess.run(["git", "init", "-q", root], check=True)
            write(os.path.join(root, "ok.txt"), "x")
            write(os.path.join(root, "bad.dds"), b"synthetic")
            subprocess.run(["git", "-C", root, "add", "-f", "ok.txt", "bad.dds"], check=True)
            self.assertEqual(check_no_game_files.forbidden(check_no_game_files.tracked(root)), ["bad.dds"])


if __name__ == "__main__":
    unittest.main()


import pad_prompts  # noqa: E402


def _png(width, height, rgba, filters=None):
    """A synthetic RGBA PNG; filters: per row (0 none, 1 sub), the data given unfiltered."""
    raw = b""
    for y in range(height):
        f = (filters or [0] * height)[y]
        row = bytearray(rgba[y * width * 4:(y + 1) * width * 4])
        if f == 1:
            row = bytearray(row[i] - (row[i - 4] if i >= 4 else 0) & 255 for i in range(len(row)))
        raw += bytes([f]) + bytes(row)

    def chunk(kind, body):
        import zlib
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    import zlib
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class PadPromptsTest(unittest.TestCase):
    def test_table(self):
        self.assertEqual(pad_prompts.prompt_icons("xbox"), [])
        ps = {i: f for i, f, _ in pad_prompts.prompt_icons("playstation4")}
        self.assertEqual((ps["a_button"], ps["b_button"], ps["start_button"], ps["back_button"]),
                         ("cross", "circle", "start_ps4", "back_ps4"))
        self.assertEqual((ps["controller"], ps["dpad_up"], ps["analog_button_left_down"]),
                         ("controller_ps4", "dpad_up", "stick_r_press"))
        self.assertEqual(len(ps), 24)
        self.assertEqual({i: f for i, f, _ in pad_prompts.prompt_icons("playstation")}["start_button"], "start_ps5")
        self.assertEqual({i: s for i, _, s in pad_prompts.prompt_icons("nintendo")}["controller"], 0.8)
        n = {i: f for i, f, _ in pad_prompts.prompt_icons("nintendo")}
        self.assertEqual((n["a_button"], n["b_button"]), ("b", "a"), "by position")
        n = {i: f for i, f, _ in pad_prompts.prompt_icons("nintendo", True)}
        self.assertEqual((n["a_button"], n["b_button"]), ("a", "b"), "swapped")

    def test_png(self):
        px = bytes([255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 10, 20, 30, 40])
        img = pad_prompts.read_png(_png(2, 2, px, filters=[0, 1]))
        self.assertEqual((img.width, img.height, bytes(img.rgba)), (2, 2, px))

    def test_dds_argb_round_trip(self):
        img = pad_prompts.Image(3, 2, bytes(range(24)))
        back = pad_prompts.decode_dds(pad_prompts.encode_dds_argb(img))
        self.assertEqual(bytes(back.rgba), bytes(range(24)))

    def test_dxt3(self):
        header = bytearray(128)
        header[0:4] = b"DDS "
        struct.pack_into("<II", header, 12, 4, 4)
        struct.pack_into("<I4sI", header, 80, 0x4, b"DXT3", 0)
        alpha = (0xF).to_bytes(1, "little") + bytes(7)          # texel 0 opaque, the rest clear
        colour = struct.pack("<HHI", 0xF800, 0x001F, 0)          # every texel colour 0: red
        img = pad_prompts.decode_dds(bytes(header) + alpha + colour)
        self.assertEqual(bytes(img.rgba[0:4]), bytes([255, 0, 0, 255]))
        self.assertEqual(img.rgba[7], 0)

    def test_fit_and_patch(self):
        icon = pad_prompts.Image(4, 2, bytes([255, 0, 0, 255]) * 8)
        fitted = pad_prompts.fit_icon(icon, 4, 4)
        # In the patch, the 4x4 region keeps a 1-texel border: the icon fits 2x1, at (3, 3).
        self.assertEqual(fitted.rgba[3], 0, "transparent above")
        self.assertEqual(bytes(fitted.rgba[16:20]), bytes([255, 0, 0, 255]), "centred: rows 1 and 2")
        texture = pad_prompts.encode_dds_argb(pad_prompts.Image(8, 8, bytes([0, 0, 255, 255]) * 64))
        xml = ('<Imageset Name="X360_widgets1" Imagefile="media/ui/x360/w.dds">'
               '<Image Name="a_button" XPos="2" YPos="2" Width="4" Height="4" />'
               '<Image Name="frame" XPos="0" YPos="0" Width="2" Height="2" /></Imageset>')
        data, count = pad_prompts.patch_texture(xml, texture, {"a_button": icon})
        img = pad_prompts.decode_dds(data)
        self.assertEqual(count, 1)
        self.assertEqual(bytes(img.rgba[(3 * 8 + 3) * 4:(3 * 8 + 3) * 4 + 4]), bytes([255, 0, 0, 255]))
        self.assertEqual(img.rgba[(3 * 8 + 2) * 4 + 3], 0, "the border cleared")
        self.assertEqual(img.rgba[(2 * 8 + 2) * 4 + 3], 0, "the region cleared around the icon")
        self.assertEqual(bytes(img.rgba[0:4]), bytes([0, 0, 255, 255]), "the rest as it was")
        with self.assertRaises(ValueError):
            pad_prompts.patch_texture(xml.replace('XPos="2"', 'XPos="7"'), texture, {"a_button": icon})

    def test_converter_applies_them(self):
        texture = pad_prompts.encode_dds_argb(pad_prompts.Image(8, 8))
        out = {"media/ui/x360/widgets1.imageset":
               b'<Imageset Imagefile="media/ui/x360/widgets1.dds"><Image Name="b_button" XPos="0" YPos="0" Width="8" Height="8" /></Imageset>',
               "media/ui/x360/widgets1.dds": texture}
        report = {}
        icon = pad_prompts.Image(2, 2, bytes([1, 2, 3, 255]) * 4)
        out = convert_x360_ui.apply_prompts(out, report, {"b_button": icon})
        self.assertEqual(report["prompts"], {"media/ui/x360/widgets1.imageset": 1})
        self.assertNotEqual(out["media/ui/x360/widgets1.dds"], texture)

    def test_the_icons_in_the_repository(self):
        root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "prompts")
        self.assertIn("Kenney", open(os.path.join(root, "LICENSE.txt")).read())
        for style in ("playstation3", "playstation4", "playstation5", "nintendo"):
            icons = pad_prompts.load_icons(root, style)
            self.assertEqual(len(icons), 24)
            for img, _ in icons.values():
                self.assertTrue(img.width and img.height and any(img.rgba[3::4]))


class SendInputTypeTest(unittest.TestCase):
    def test_type_and_hold(self):
        import send_input
        self.assertEqual(send_input.translate(["type", "a_"]),
                         ["key", "65", "wait", "40", "down", "16", "wait", "60", "key", "189", "wait", "60",
                          "up", "16", "wait", "40"])
        self.assertEqual(send_input.translate(["down", "Shift", "key", "grave", "up", "Shift"]),
                         ["down", "16", "key", "192", "up", "16"])
        with self.assertRaises(ValueError):
            send_input.translate(["type", "a;b"])


def ui_window(base, rect, children=(), shown=True, prefix="7_", **fields):
    """A window of a synthetic UI dump: rect is (left, top, right, bottom) at 1080p."""
    left, top, right, bottom = rect
    w = {"name": prefix + base, "base_name": base, "shown": shown, "text": "", "font": "Serif",
         "images": {}, "effective_alpha": 1,
         "rect_1080p": {"left": left, "top": top, "right": right, "bottom": bottom}, "children": list(children)}
    w.update(fields)
    return w


class CompareUITest(unittest.TestCase):
    def dumps(self, pc_label, pc_extra=()):
        ref = {"windows": [ui_window("Root", (0, 0, 1920, 1080), [
            ui_window("Label", (10, 10, 110, 40), text="Level 3", font="Fancy"),
            ui_window("Icon", (0, 0, 32, 32), images={"Image": {"imageset": "widgets", "image": "a"}}),
            ui_window("Hidden", (0, 0, 5, 5), shown=False)])]}
        pc = {"windows": [ui_window("Root", (0, 0, 1920, 1080), [
            pc_label,
            ui_window("Icon", (0, 0, 32, 32), prefix="x360hud_",
                      images={"Image": {"imageset": "X360_widgets", "image": "a"}}), *pc_extra])]}
        return ref, pc

    def test_same_screen_has_no_differences(self):
        ref, pc = self.dumps(ui_window("Label", (10.5, 10, 110, 40), text="Level 3", font="X360_Fancy"))
        self.assertEqual(compare_ui.compare(ref, pc), [])

    def test_rect_text_font_and_missing_windows(self):
        ref, pc = self.dumps(ui_window("Label", (14, 10, 110, 40), text="Level 2", font="X360_Serif"))
        lines = compare_ui.compare(ref, pc)
        self.assertEqual(len(lines), 3)
        self.assertIn("Root/Label: rect left 10 vs 14", lines)
        self.assertTrue(any("text 'Level 3' vs 'Level 2'" in line for line in lines))
        ref, pc = self.dumps(ui_window("Other", (10, 10, 110, 40)), [ui_window("Extra", (0, 0, 1, 1))])
        self.assertEqual(compare_ui.compare(ref, pc), ["Root/Label: missing on PC"])
        self.assertIn("Root/Other: only on PC", compare_ui.compare(ref, pc, show_extra=True))

    def test_shown_on_pc_where_the_360_hides_it(self):
        ref, pc = self.dumps(ui_window("Label", (10, 10, 110, 40), text="Level 3", font="Fancy"),
                             [ui_window("Hidden", (0, 0, 5, 5))])
        self.assertEqual(compare_ui.compare(ref, pc), ["Root/Hidden: shown on PC, hidden on the 360"])

    def test_windows_at_alpha_0_are_not_shown(self):
        ref, pc = self.dumps(ui_window("Label", (10, 10, 110, 40), text="Level 3", font="Fancy"),
                             [ui_window("Unseen", (0, 0, 5, 5), effective_alpha=0)])
        self.assertEqual(compare_ui.compare(ref, pc, show_extra=True), [])

    def test_under_limits_the_subtree(self):
        ref, pc = self.dumps(ui_window("Label", (99, 10, 110, 40)))
        self.assertEqual(compare_ui.compare(ref, pc, under="Icon"), [])


class LabelLookTest(unittest.TestCase):
    def test_the_label_button_is_made_relative_to_the_height(self):
        look = ('<WidgetLook name="ceguiWidget/ItemTextButton"><ImagerySection name="Button"><ImageryComponent><Area>'
                '<Dim type="LeftEdge" ><AbsoluteDim value="-20" /></Dim><Dim type="Width" ><AbsoluteDim value="21" /></Dim>'
                '</Area></ImageryComponent></ImagerySection></WidgetLook>')
        out = convert_x360_ui.label_button_relative(look)
        self.assertIn('<Dim type="LeftEdge" ><UnifiedDim scale="-1.1581" type="Height" /></Dim>', out)
        self.assertIn('<Dim type="Width" ><UnifiedDim scale="1.2160" type="Height" /></Dim>', out)
        self.assertEqual(convert_x360_ui.label_button_relative("<WidgetLook name=\"x\"></WidgetLook>"),
                         "<WidgetLook name=\"x\"></WidgetLook>")


class LabelFrameTest(unittest.TestCase):
    def test_label_frame_made_relative_in_the_label_look_only(self):
        text = ('<WidgetLook name="ceguiWidget/StaticShared"><ImagerySection name="frame"><FrameComponent>'
                '<Area><Dim type="LeftEdge" ><AbsoluteDim value="-24" /></Dim>'
                '<Dim type="TopEdge" ><AbsoluteDim value="-9" /></Dim>'
                '<Dim type="Width" ><UnifiedDim scale="1" type="Width" ><DimOperator op="Add">'
                '<AbsoluteDim value="48" /></DimOperator></UnifiedDim></Dim>'
                '<Dim type="Height" ><UnifiedDim scale="1" type="Height" ><DimOperator op="Add">'
                '<AbsoluteDim value="18" /></DimOperator></UnifiedDim></Dim></Area>'
                '</FrameComponent></ImagerySection></WidgetLook>'
                '<WidgetLook name="ceguiWidget/ItemTextButton"><ImagerySection name="label" />'
                '<StateImagery name="Enabled"><Layer><Section look="ceguiWidget/StaticShared" section="frame" />'
                '</Layer></StateImagery></WidgetLook>')
        out = convert_x360_ui.label_frame_relative(text)
        shared = out[:out.find('<WidgetLook name="ceguiWidget/ItemTextButton">')]
        label = out[out.find('<WidgetLook name="ceguiWidget/ItemTextButton">'):]
        self.assertIn('<AbsoluteDim value="-24" />', shared)
        self.assertIn('<ImagerySection name="labelframe">', label)
        self.assertIn('<Section section="labelframe" />', label)
        self.assertIn('<UnifiedDim scale="-1.3897" type="Height" />', label)
        self.assertIn('<UnifiedDim scale="2.7794" type="Height" />', label)
        self.assertLess(label.find("labelframe"), label.find("<StateImagery"))


class ItemDescriptionsTest(unittest.TestCase):
    def unit(self, path, name, desc):
        strings = {1: "UNIT", 2: "NAME", 3: name, 4: "DESCRIPTION"}
        props = [(2, 5, 3)]
        if desc is not None:
            strings[5] = desc
            props.append((4, 8, 5))
        return path, synthetic_adm(strings, (1, props, []))

    def pak(self, d, name, units):
        p = os.path.join(d, name)
        with zipfile.ZipFile(p, "w") as z:
            for path, raw in units:
                z.writestr(path, bytes(raw))
        return p

    def test_only_differing_descriptions_with_the_360s_text(self):
        with tempfile.TemporaryDirectory() as d:
            x3 = self.pak(d, "x3.zip", [self.unit("media/units/items/a.dat.adm", "Scroll A", None),
                                        self.unit("media/units/items/b.dat.adm", "Fish B", "Feed it"),
                                        self.unit("media/units/items/c.dat.adm", "Same C", "Same text")])
            pc = self.pak(d, "pc.zip", [self.unit("media/units/items/A.DAT.adm", "Scroll A", "Click to use"),
                                        self.unit("media/units/items/b.dat.adm", "Fish B", "Click to feed it"),
                                        self.unit("media/units/items/c.dat.adm", "Same C", "Same text")])
            self.assertEqual(convert_x360_ui.item_descriptions(x3, pc),
                             {"desc.Scroll A": "~", "desc.Fish B": "~Feed it"})


def fake_exe(path, timestamp):
    """A Torchlight.exe of nothing but a PE header with that link timestamp."""
    head = bytearray(512)
    head[:2] = b"MZ"
    struct.pack_into("<I", head, 0x3C, 0x80)
    head[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<I", head, 0x88, timestamp)
    with open(path, "wb") as f:
        f.write(bytes(head))


def fake_game(path, timestamp=install.GOG_TIMESTAMP):
    os.makedirs(path, exist_ok=True)
    fake_exe(os.path.join(path, "Torchlight.exe"), timestamp)
    with open(os.path.join(path, "CEGUIBase.dll"), "wb") as f:
        f.write(b"MZ")


class InstallTest(unittest.TestCase):
    def test_finds_the_stores_installs_and_tells_the_builds(self):
        with tempfile.TemporaryDirectory() as home:
            gog = os.path.join(home, "Games", "Heroic", "Torchlight")
            fake_game(gog)
            wine = os.path.join(home, "prefix", "drive_c", "GOG Games", "Torchlight")
            fake_game(wine)
            steam = os.path.join(home, ".steam", "steam", "steamapps")
            fake_game(os.path.join(steam, "common", "Torchlight"), 0x4B000000)
            open(os.path.join(steam, "appmanifest_41500.acf"), "w").close()
            epic = os.path.join(home, "Epic", "Torchlight")
            fake_game(epic, 0x4C000000)
            os.makedirs(os.path.join(home, ".config", "legendary"))
            with open(os.path.join(home, ".config", "legendary", "installed.json"), "w") as f:
                json.dump({"Abc": {"title": "Torchlight", "install_path": epic}}, f)
            found = install.find_games(home)
            stores = {s for s, _ in found}
            self.assertEqual(stores, {"gog", "steam", "epic"})
            builds = {p: install.build_of(p) for _, p in found}
            self.assertEqual(builds[os.path.realpath(gog)], "gog")
            self.assertEqual(builds[os.path.realpath(wine)], "gog")
            self.assertEqual(builds[os.path.realpath(epic)], "unknown")
            steam_build = os.path.join(home, "steambuild")
            fake_game(steam_build, 0x4B5F7519)
            self.assertEqual(install.build_of(steam_build), "steam")

    def test_writes_the_ini_with_the_bag_and_keeps_other_lines(self):
        with tempfile.TemporaryDirectory() as home:
            game = os.path.join(home, "game")
            os.makedirs(game)
            with open(os.path.join(game, "x360_ui.ini"), "w") as f:
                f.write("out=old\nfont_diff=1\nupdate_check=0\n")
            install.write_ini(game, os.path.join(game, "x360ui"), "360")
            with open(os.path.join(game, "x360_ui.ini")) as f:
                text = f.read()
            self.assertIn("prompts=auto", text)
            self.assertIn("bag=360", text)
            self.assertIn("font_diff=1", text)
            self.assertNotIn("out=old", text)
            self.assertNotIn("update_check", text)
            self.assertEqual(install.read_ini(game)["bag"], "360")

    def test_reads_the_360_copy_from_an_extracted_folder(self):
        with tempfile.TemporaryDirectory() as home:
            x = os.path.join(home, "x360", "extracted")
            os.makedirs(x)
            with zipfile.ZipFile(os.path.join(x, "pak.zip"), "w") as z:
                z.writestr("media/UI/mastermenu_xenon.layout", "<GUILayout/>")
            with open(os.path.join(x, "default.bin"), "wb") as f:
                f.write(b"MZ")
            work = os.path.join(home, "work")
            os.makedirs(work)
            saved = install.is_x360_image
            try:
                install.is_x360_image = lambda data: data[:2] == b"MZ"
                pak, image = install.read_360_copy(os.path.join(home, "x360"), work)
            finally:
                install.is_x360_image = saved
            self.assertTrue(install.is_x360_pak(pak))
            self.assertEqual(image, os.path.join(x, "default.bin"))
            other = os.path.join(home, "other")
            os.makedirs(other)
            with zipfile.ZipFile(os.path.join(other, "pak.zip"), "w") as z:
                z.writestr("media/UI/hud.layout", "<GUILayout/>")
            open(os.path.join(other, "default.bin"), "wb").close()
            with self.assertRaises(SystemExit):
                install.read_360_copy(other, work)

    def test_refuses_another_games_package(self):
        with tempfile.TemporaryDirectory() as home:
            path = os.path.join(home, "pkg")
            with open(path, "wb") as f:
                f.write(stfs_package({"pak.zip": b"x"}, title=0x12345678))
            with self.assertRaises(SystemExit) as e:
                install.read_360_copy(path, home)
            self.assertIn("another game", str(e.exception))

    def test_backs_up_the_saves_of_the_games_prefix(self):
        def save_in(prefix):
            path = os.path.join(prefix, "drive_c", "users", "me", "AppData", "Roaming", "runic games", "torchlight",
                                "save")
            os.makedirs(path)
            return os.path.realpath(path)
        with tempfile.TemporaryDirectory() as home:
            # The install's prefix and a Proton run's own beside it, as ~/torchlight-gog/{prefix,compat/pfx}.
            game = os.path.join(home, "tl", "prefix", "drive_c", "GOG Games", "Torchlight")
            fake_game(game)
            own, beside = save_in(os.path.join(home, "tl", "prefix")), save_in(os.path.join(home, "tl", "compat", "pfx"))
            save_in(os.path.join(home, "other", "pfx"))
            with open(os.path.join(own, "0.svt"), "w") as f:
                f.write("hero")
            folders = install.save_folders(game, home)
            self.assertEqual(sorted(folders), sorted([own, beside]))
            copies = install.back_up_saves(folders)
            with open(os.path.join(copies[folders.index(own)], "0.svt")) as f:
                self.assertEqual(f.read(), "hero")
            with open(os.path.join(own, "0.svt")) as f:
                self.assertEqual(f.read(), "hero")
            # A prefix right in the home (~/.wine): its own saves only, not every prefix of the home.
            wine_game = os.path.join(home, ".wine", "drive_c", "GOG Games", "Torchlight")
            fake_game(wine_game)
            wine = save_in(os.path.join(home, ".wine"))
            self.assertEqual(install.save_folders(wine_game, home), [wine])
            # Called as do_install calls it, without a home: the user's (it raised a TypeError).
            with unittest.mock.patch.dict(os.environ, {"HOME": home}):
                self.assertEqual(sorted(install.save_folders(game)), sorted([own, beside]))

    def test_uninstall_removes_what_the_install_wrote(self):
        with tempfile.TemporaryDirectory() as home:
            game = os.path.join(home, "game")
            fake_game(game)
            for folder in install.converted_folders(game):
                os.makedirs(os.path.join(folder, "media"))
            for name in ("x360_ui.ini", "x360_ui.log", "x360_dump_a.json"):
                open(os.path.join(game, name), "w").close()
            removed = []
            saved = install.build_proxy.remove
            try:
                install.build_proxy.remove = lambda g: removed.append(g) or True
                self.assertEqual(install.main(["uninstall", "--game", game, "--yes"]), 0)
            finally:
                install.build_proxy.remove = saved
            self.assertEqual(removed, [os.path.realpath(game)])
            self.assertEqual(sorted(os.listdir(game)), ["CEGUIBase.dll", "Torchlight.exe"])


def stfs_package(files, title=0x58410A7E, content_type=0x000D0000, contiguous=True):
    """A LIVE package of the files (name: bytes): the file table at data block 0, each file's
    blocks after it, chained through the level 0 hash table when not contiguous."""
    header_size = 0xAD0E                       # rounded up to 0xB000: one copy of each table
    out = bytearray(0xB000)
    out[:4] = b"LIVE"
    struct.pack_into(">I", out, 0x340, header_size)
    struct.pack_into(">I", out, 0x344, content_type)
    struct.pack_into(">I", out, 0x360, title)
    v = 0x379
    out[v] = 0x24
    out[v + 2] = 1
    struct.pack_into("<H", out, v + 3, 1)       # the file table: one block, data block 0
    blocks, entries, nxt = [b""], [], {}
    for name, data in files.items():
        first = len(blocks)
        chunks = [data[i:i + 0x1000] for i in range(0, max(len(data), 1), 0x1000)]
        blocks += chunks
        for k in range(len(chunks) - 1):
            nxt[first + k] = first + k + 1
        e = bytearray(0x40)
        e[:len(name)] = name.encode()
        e[0x28] = len(name) | (0x40 if contiguous else 0)
        e[0x29:0x2C] = len(chunks).to_bytes(3, "little")
        e[0x2F:0x32] = first.to_bytes(3, "little")
        struct.pack_into(">h", e, 0x32, -1)
        struct.pack_into(">I", e, 0x34, len(data))
        entries.append(bytes(e))
    blocks[0] = b"".join(entries)
    hash_table = bytearray(0x1000)
    for n, m in nxt.items():
        hash_table[n * 0x18 + 0x15:n * 0x18 + 0x18] = m.to_bytes(3, "big")
    out += hash_table                          # block 0: the level 0 hash table
    for b in blocks:                           # data block n at block n + 1
        out += b.ljust(0x1000, b"\0")
    return bytes(out)


class CheckRealDllTest(unittest.TestCase):
    def test_the_real_dll_against_the_list(self):
        with tempfile.TemporaryDirectory() as d:
            game, listed = os.path.join(d, "game"), os.path.join(d, "exports.txt")
            dll, orig = os.path.join(game, "CEGUIBase.dll"), os.path.join(game, "CEGUIBase_orig.dll")
            real = synthetic_pe({}, ["?a@CEGUI@@XZ"])
            write(listed, "1 cegui ?a@CEGUI@@XZ\n")
            write(dll, real)
            install.check_real_dll(game, listed)                 # not installed yet
            os.rename(dll, orig)
            write(dll, b"MZ forwards to CEGUIBase_orig.x")
            install.check_real_dll(game, listed)                 # the proxy installed: the real one beside it
            write(dll, real)
            install.check_real_dll(game, listed)                 # a store's file check put the real one back
            os.remove(dll)
            install.check_real_dll(game, listed)                 # only the real one, renamed
            write(dll, b"MZ forwards to CEGUIBase_orig.x")
            write(listed, "1 cegui ?a@CEGUI@@XZ\n" + "2 cegui ?b@CEGUI@@XZ\n")
            with self.assertRaises(SystemExit) as e:
                install.check_real_dll(game, listed)
            self.assertIn("is not the CEGUIBase.dll this mod is made for", str(e.exception))
            os.remove(orig)
            with self.assertRaises(SystemExit) as e:
                install.check_real_dll(game, listed)
            self.assertIn("the real CEGUIBase.dll is missing", str(e.exception))


class StfsTest(unittest.TestCase):
    def test_lists_and_reads_files_contiguous_or_chained(self):
        big = bytes(range(256)) * 20            # 5120 bytes: two blocks
        for contiguous in (True, False):
            with tempfile.TemporaryDirectory() as home:
                path = os.path.join(home, "pkg")
                with open(path, "wb") as f:
                    f.write(stfs_package({"pak.zip": b"zip", "default.xex": big}, contiguous=contiguous))
                with stfs.Package(path) as pkg:
                    self.assertEqual(pkg.title_id, 0x58410A7E)
                    self.assertEqual(sorted(e["path"] for e in pkg.files), ["default.xex", "pak.zip"])
                    self.assertEqual(pkg.read(pkg.find("DEFAULT.XEX")), big)
                    self.assertEqual(pkg.read(pkg.find("pak.zip")), b"zip")

    def test_refuses_what_is_not_a_package(self):
        with tempfile.TemporaryDirectory() as home:
            path = os.path.join(home, "x")
            with open(path, "wb") as f:
                f.write(b"MZ" + bytes(0x400))
            with self.assertRaises(ValueError):
                stfs.Package(path)


class Aes128Test(unittest.TestCase):
    def test_fips_197_and_cbc_chaining(self):
        key = bytes(range(16))
        self.assertEqual(aes128.cbc_decrypt(key, bytes(16), bytes.fromhex("69c4e0d86a7b0430d8cdb78070b4c55a")),
                         bytes.fromhex("00112233445566778899aabbccddeeff"))
        # NIST SP 800-38A F.2.2, the first two blocks.
        key = bytes.fromhex("2b7e151628aed2a6abf7158809cf4f3c")
        iv = bytes.fromhex("000102030405060708090a0b0c0d0e0f")
        ct = bytes.fromhex("7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2")
        self.assertEqual(aes128.cbc_decrypt(key, iv, ct),
                         bytes.fromhex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"))


class ImageStringsTest(unittest.TestCase):
    def test_reads_the_texts_at_their_addresses(self):
        image = bytearray(0x200)
        base = convert_x360_ui.IMAGE_BASE
        saved = dict(convert_x360_ui.STRINGS)
        saved8 = dict(convert_x360_ui.STRINGS_UTF8)
        try:
            convert_x360_ui.STRINGS.clear()
            convert_x360_ui.STRINGS.update({"a": base + 0x10, "b": base + 0x40, "c": base + 0x1000})
            convert_x360_ui.STRINGS_UTF8.clear()
            convert_x360_ui.STRINGS_UTF8.update({"d": base + 0x80, "e": base + 0x2000})
            image[0x10:0x10 + 8] = "Tab1".encode("utf-16-be")
            image[0x40:0x42] = b"\x00\x01"   # not printable
            image[0x80:0x80 + 6] = b"Spell\0"
            self.assertEqual(convert_x360_ui.image_strings(bytes(image)), {"a": "Tab1", "d": "Spell"})
        finally:
            convert_x360_ui.STRINGS.clear()
            convert_x360_ui.STRINGS.update(saved)
            convert_x360_ui.STRINGS_UTF8.clear()
            convert_x360_ui.STRINGS_UTF8.update(saved8)


def synthetic_adm(strings, root):
    """A Runic .adm: strings {id: text}; root (name id, [(key id, type, value)], children)."""
    out = bytearray(struct.pack("<II", 1, len(strings)))
    for i, text in strings.items():
        out += struct.pack("<II", i, len(text)) + text.encode("utf-16-le")

    def node(n):
        name, props, children = n
        b = bytearray(struct.pack("<II", name, len(props)))
        for key, kind, value in props:
            b += struct.pack("<II", key, kind) + struct.pack("<Q" if kind == 6 else "<I", value)
        b += struct.pack("<I", len(children))
        for c in children:
            b += node(c)
        return b
    return bytes(out + node(root))


class ContextTipsTest(unittest.TestCase):
    def test_tips(self):
        strings = {1: "GLOBALS", 2: "CONTEXTTIPS", 3: "TIP_A", 4: "Press A", 5: "OTHER", 6: "TIP_B", 7: "Push B",
                   8: "BIG"}
        root = (1, [(5, 1, 7), (8, 6, 1 << 40)], [(5, [(3, 8, 4)], []), (2, [(3, 8, 4), (6, 8, 7), (5, 1, 3)], [])])
        raw = synthetic_adm(strings, root)
        self.assertEqual(convert_x360_ui.context_tips(raw), {"TIP_A": "Press A", "TIP_B": "Push B"})


def two_section_pe(code, data, code_rva=0x1000, data_rva=0x9000):
    """A minimal PE32 with a .text and a .data section (no imports or exports)."""
    head = bytearray(0x200)
    head[0:2] = b"MZ"
    struct.pack_into("<I", head, 0x3C, 0x40)
    head[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHI", head, 0x44, 0x14C, 2, 1)
    struct.pack_into("<H", head, 0x40 + 20, 224)
    opt = 0x40 + 24
    struct.pack_into("<H", head, opt, 0x10B)
    struct.pack_into("<I", head, opt + 28, 0x400000)
    struct.pack_into("<I", head, opt + 92, 16)
    sec = opt + 224
    raw = 0x200
    for i, (name, rva, body) in enumerate(((b".text", code_rva, code), (b".data", data_rva, data))):
        o = sec + 40 * i
        head[o:o + 8] = name.ljust(8, b"\0")
        struct.pack_into("<IIII", head, o + 8, len(body), rva, len(body), raw)
        raw += len(body)
    return bytes(head) + bytes(code) + bytes(data)


class PortAddressesTest(unittest.TestCase):
    def builds(self):
        import random
        rng = random.Random(3)

        def filler(n):
            return bytes(rng.randrange(256) for _ in range(n))

        def function(callee_rel, data_va):
            # push ebp; mov ebp,esp; mov eax,[data]; call rel32; add eax,7; pop ebp; ret, then a body.
            return (b"\x55\x8b\xec\xa1" + struct.pack("<I", data_va) + b"\xe8" + struct.pack("<i", callee_rel)
                    + b"\x83\xc0\x07\x5d\xc3" + bytes(range(40, 80)))
        gog_code = filler(0x300) + function(0x1234, 0x409010) + filler(0x300)
        other_code = filler(0x500) + function(-0x88, 0x40a020) + filler(0x200)
        gog = port_addresses.Image(two_section_pe(gog_code, bytes(0x40)))
        other = port_addresses.Image(two_section_pe(other_code, bytes(0x40), data_rva=0xa000))
        return gog, other

    def test_code_address(self):
        gog, other = self.builds()
        there, how = port_addresses.port(gog, other, 0x401300)
        self.assertEqual(there, 0x401500, how)
        there, _ = port_addresses.port(gog, other, 0x401308)  # the call, inside the function
        self.assertEqual(there, 0x401508)

    def test_data_address(self):
        gog, other = self.builds()
        there, how = port_addresses.port(gog, other, 0x409010)
        self.assertEqual(there, 0x40a020, how)

    def test_missing(self):
        gog, other = self.builds()
        there, _ = port_addresses.port(gog, other, 0x401000)  # filler only GOG has
        self.assertIsNone(there)

    def test_source_addresses(self):
        with tempfile.TemporaryDirectory() as d:
            write(os.path.join(d, "a.c"), "f(0x40c8f0); g(0x2d8); h(0x82381D18); k(0x5dc320);")
            write(os.path.join(d, "b.h"), "0x40aaaa")
            self.assertEqual(port_addresses.source_addresses(d), [0x40c8f0, 0x5dc320])



def sections_pe(sections, timestamp=1, import_dir=(0, 0)):
    """A minimal PE32 of [(name, rva, bytes)] sections, with an optional import directory."""
    head = bytearray(0x400)
    head[0:2] = b"MZ"
    struct.pack_into("<I", head, 0x3C, 0x40)
    head[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHI", head, 0x44, 0x14C, len(sections), timestamp)
    struct.pack_into("<H", head, 0x40 + 20, 224)
    opt = 0x40 + 24
    struct.pack_into("<H", head, opt, 0x10B)
    struct.pack_into("<I", head, opt + 28, 0x400000)
    struct.pack_into("<I", head, opt + 92, 16)
    struct.pack_into("<II", head, opt + 104, *import_dir)
    sec = opt + 224
    raw = len(head)
    body = b""
    for i, (name, rva, data) in enumerate(sections):
        o = sec + 40 * i
        head[o:o + 8] = name.ljust(8, b"\0")
        struct.pack_into("<IIII", head, o + 8, len(data), rva, len(data), raw)
        raw += len(data)
        body += data
    return bytes(head) + body


class PortEvidenceTest(unittest.TestCase):
    """The ways port_addresses.py finds an address when a window at it does not."""

    def build(self, gog):
        """One synthetic build: .text with a function F (its prologue the build's own), two callers
        that read a global, padding between them; .rdata with F's vtable, its RTTI locator and a wide
        string; .data with the type descriptor and the global. Returns (image, addresses)."""
        import random
        rng = random.Random(7)                 # the code both builds share
        fill = random.Random(1 if gog else 2)  # what is each build's own
        shift = 0 if gog else 0x230   # the other build's code and data sit elsewhere
        text_rva, rdata_rva, data_rva = 0x1000, 0x4000, 0x6000
        a = {}
        code = bytearray(fill.randrange(256) for _ in range(0x100 + shift))
        code += b"\xcc" * (-len(code) % 16)

        def at(name):
            a[name] = 0x400000 + text_rva + len(code)

        body = bytes(rng.randrange(1, 0xe0) for _ in range(96))   # no E8/E9/0F: no displacements
        at("F")
        # GOG: push -1; push handler; mov eax, fs:[0]  /  other: mov eax, fs:[0]; push -1; push handler
        handler = struct.pack("<I", 0x400000 + text_rva + 0x10)
        prologue = (b"\x6a\xff\x68" + handler + b"\x64\xa1\0\0\0\0" if gog
                    else b"\x64\xa1\0\0\0\0\x6a\xff\x68" + handler)
        code += prologue + body + b"\xc3"
        code += b"\xcc" * (-len(code) % 16)
        data_global = 0x400000 + data_rva + 0x40 + (0x18 if not gog else 0)
        callers = []
        for n in range(2):
            at(f"G{n}")
            lead = bytes(rng.randrange(1, 0xe0) for _ in range(24))
            code += lead + b"\xa1" + struct.pack("<I", data_global) + b"\x8b\xc8"
            callers.append(len(code))
            code += b"\xe8\0\0\0\0" + bytes(rng.randrange(1, 0xe0) for _ in range(24)) + b"\xc3"
            code += b"\xcc" * (-len(code) % 16)
        for site in callers:
            struct.pack_into("<i", code, site + 1, (a["F"] - 0x400000 - text_rva) - (site + 5))
        code += bytes(fill.randrange(256) for _ in range(0x100))
        rdata = bytearray(0x200 + shift)
        data = bytearray(0x100 + shift)
        td = 0x400000 + data_rva + 0x80 + shift
        data[0x88 + shift:0x88 + shift + 13] = b".?AVCThing@@\0"
        col = 0x400000 + rdata_rva + 0x10 + shift
        struct.pack_into("<IIIII", rdata, 0x10 + shift, 0, 0, 0, td, 0)
        struct.pack_into("<III", rdata, 0x40 + shift, col, a["F"], a["G0"])
        a["vtable"] = 0x400000 + rdata_rva + 0x44 + shift
        text = "QUEST COMPLETE".encode("utf-16-le") + b"\0\0"
        rdata[0x100 + shift:0x100 + shift + len(text)] = text
        a["string"] = 0x400000 + rdata_rva + 0x100 + shift
        a["global"] = data_global
        image = port_addresses.Image(sections_pe([(b".text", text_rva, bytes(code)), (b".rdata", rdata_rva, bytes(rdata)),
                                                  (b".data", data_rva, bytes(data))]))
        return image, a

    def setUp(self):
        self.gog, self.ga = self.build(True)
        self.other, self.oa = self.build(False)
        self.porter = port_addresses.Porter(self.gog, self.other)

    def test_function_by_callers_and_body(self):
        value, status, how = self.porter.port(self.ga["F"])
        self.assertEqual(value, self.oa["F"], how)
        self.assertEqual(status, "verified", how)
        self.assertIn("callers", how)
        self.assertNotIn("window", how)   # the prologues differ

    def test_vtable_by_rtti(self):
        value, status, how = self.porter.port(self.ga["vtable"])
        self.assertEqual(value, self.oa["vtable"], how)
        self.assertIn("rtti .?AVCThing@@", how)

    def test_global_by_references(self):
        value, status, how = self.porter.port(self.ga["global"])
        self.assertEqual((value, status), (self.oa["global"], "verified"), how)

    def test_wide_string(self):
        value, status, how = self.porter.port(self.ga["string"])
        self.assertEqual(value, self.oa["string"], how)
        self.assertIn("content 30 bytes equal", how)

    def test_import_slot(self):
        gog = port_addresses.Image(synthetic_pe({"msvcp90.dll": ["a", "b"]}, []))
        other = port_addresses.Image(synthetic_pe({"kernel32.dll": ["x"], "msvcp90.dll": ["b", "a"]}, []))
        slot = next(s for s, imp in gog.iat().items() if imp == ("msvcp90.dll", "b"))
        there = next(s for s, imp in other.iat().items() if imp == ("msvcp90.dll", "b"))
        self.assertNotEqual(slot, there)
        value, status, how = port_addresses.Porter(gog, other).port(slot)
        self.assertEqual((value, status), (there, "single"), how)

    def test_verdict(self):
        ev = port_addresses.Evidence()
        self.assertEqual(ev.verdict()[1], "unresolved")
        ev.add(0x10, "window 16")
        self.assertEqual(ev.verdict()[:2], (0x10, "single"))
        ev.add(0x10, "local 0+16 in f")
        self.assertEqual(ev.verdict()[:2], (0x10, "verified"))
        ev.add(0x20, "refs-global 0x401000")   # weak: does not overrule
        self.assertEqual(ev.verdict()[:2], (0x10, "verified"))
        ev.add(0x30, "callers 0x401000")       # strong and different: unresolved
        self.assertEqual(ev.verdict()[:2], (None, "unresolved"))
        many = port_addresses.Evidence()
        for site in (1, 2, 3):
            many.add(0x10, f"callers {site:#x}")
        many.add(0x40, "callers 0x9")         # one small caller matched elsewhere: outvoted
        self.assertEqual(many.verdict()[:2], (0x10, "verified"))
        many.add(0x40, "refs 0x8")
        self.assertEqual(many.verdict()[:2], (None, "unresolved"))
        weak = port_addresses.Evidence()
        weak.add(0x20, "refs-global 0x401000")
        self.assertEqual(weak.verdict()[:2], (0x20, "single"))

    def test_code_addresses_in_source(self):
        text = ('a(0x4e9a30u); /* 0x500000 */ s("0x600000"); c(\'"\'); // 0x650000\n'
                'd(0x00700000); e(0x2d8); f(0x82381D18);')
        self.assertEqual([v for _, _, v in port_addresses.addresses_in(text, code_only=True)], [0x4e9a30, 0x700000])
        self.assertEqual([v for _, _, v in port_addresses.addresses_in(text)],
                         [0x4e9a30, 0x500000, 0x600000, 0x650000, 0x700000])


class BuildTargetTest(unittest.TestCase):
    def test_table_and_substitution(self):
        with tempfile.TemporaryDirectory() as d:
            table = os.path.join(d, "t.txt")
            write(table, "# comment\n0x0054cad0 0x005482f0 verified callers\n"
                         "0x00547ca0 0x00543660 checked by hand  # trailing\n"
                         "0x00525579 - unresolved no way\n0x00410000 0x00420000 single window\n")
            t = build_proxy.read_table(table)
            self.assertEqual(t, {0x54cad0: 0x5482f0, 0x547ca0: 0x543660})
            src = ("#define GOG_MODAL 0x0054cad0   /* GOG 0x54cad0, 0x525579 */\n"
                   "f((void *)0x547ca0u); g(\"0x525579\"); h(0x2cc);\n")
            out, missing = build_proxy.substitute(src, t)
            self.assertEqual(out, "#define GOG_MODAL 0x005482f0   /* GOG 0x54cad0, 0x525579 */\n"
                                  "f((void *)0x00543660u); g(\"0x525579\"); h(0x2cc);\n")
            self.assertEqual(missing, set())
            _, missing = build_proxy.substitute("x(0x00525579); y(0x410000);", t)
            self.assertEqual(missing, {0x525579, 0x410000})
            write(table, "0x1 0x2 verified\n0x1 0x3 verified\n")
            with self.assertRaises(ValueError):
                build_proxy.read_table(table)

    def test_build_refuses_another_exe(self):
        with tempfile.TemporaryDirectory() as d:
            game = os.path.join(d, "game")
            fake_game(game, timestamp=build_proxy.TARGETS["gog"][0])
            dll = os.path.join(game, "CEGUIBase.dll")
            write(dll, synthetic_pe({}, ["?a@CEGUI@@XZ"]))
            listed = os.path.join(d, "exports.txt")
            write(listed, "1 cegui ?a@CEGUI@@XZ\n")
            with self.assertRaises(SystemExit) as e:
                build_proxy.build(dll, os.path.join(d, "out"), target="steam", exports_list=listed)
            self.assertIn("--target gog", str(e.exception))
            with self.assertRaises(SystemExit):
                build_proxy.build(dll, os.path.join(d, "out"), target="epic", exports_list=listed)

    def test_build_refuses_another_dll(self):
        with tempfile.TemporaryDirectory() as d:
            dll = os.path.join(d, "game", "CEGUIBase.dll")
            write(dll, synthetic_pe({}, ["?a@CEGUI@@XZ"]))
            listed = os.path.join(d, "exports.txt")
            write(listed, "1 cegui ?a@CEGUI@@XZ\n" + "2 cegui ?b@CEGUI@@XZ\n")
            with self.assertRaises(SystemExit) as e:
                build_proxy.build(dll, os.path.join(d, "out"), exports_list=listed)
            self.assertIn("not the CEGUIBase.dll the proxy is made for", str(e.exception))

    def test_committed_steam_table(self):
        """proxy/addresses_steam.txt covers every address in the proxy's code, numbers only."""
        repo = os.path.join(os.path.dirname(__file__), "..")
        table = build_proxy.read_table(os.path.join(repo, "proxy", "addresses_steam.txt"))
        need = port_addresses.source_addresses(os.path.join(repo, "proxy"), code_only=True)
        self.assertEqual([hex(v) for v in need if v not in table], [])
        with open(os.path.join(repo, "proxy", "addresses_steam.txt")) as f:
            for line in f:
                body = line.split("#", 1)[0].split()
                if body:
                    self.assertRegex(body[0], r"^0x[0-9a-f]{8}$")
                    self.assertRegex(body[1], r"^(0x[0-9a-f]{8}|-)$")


class CompareOffsetsTest(unittest.TestCase):
    DUMP = """
  54cad0:\t6a ff                \tpush   0xffffffff
  54caf3:\t83 be 04 03 00 00 00 \tcmp    DWORD PTR [esi+0x304],0x0
  54cafa:\tc7 44 24 10 01 00 00 00 \tmov    DWORD PTR [esp+0x10],0x1
  54cb04:\t80 be 3d 17 00 00 00 \tcmp    BYTE PTR [esi+0x173d],0x0
  54cb10:\t8b 4c 81 08          \tmov    ecx,DWORD PTR [ecx+eax*4+0x8]
  54cb14:\t8b 45 f0             \tmov    eax,DWORD PTR [ebp-0x10]
"""

    def test_field_offsets(self):
        import compare_offsets
        instrs = compare_offsets.instructions(self.DUMP)
        self.assertEqual(len(instrs), 6)
        self.assertEqual(compare_offsets.field_offsets(instrs), [("cmp", 0x304), ("cmp", 0x173d), ("mov", 0x8)])

    def test_compare(self):
        import compare_offsets
        a = [("cmp", 0x304), ("mov", 0x2bc), ("mov", 0x338)]
        self.assertEqual(compare_offsets.compare(a, list(a)), [])
        diffs = compare_offsets.compare(a, [("cmp", 0x304), ("mov", 0x2c0), ("mov", 0x338)])
        self.assertEqual(diffs, [("replace", [("mov", 0x2bc)], [("mov", 0x2c0)])])


class SaveItemsTest(unittest.TestCase):
    @staticmethod
    def record(name, slot, suffix="", instance=b"\x01" * 6, tail=b"tail"):
        n, m = name.encode("utf-16-le"), suffix.encode("utf-16-le")
        return (b"\xde\x11" + b"\x22" * 6 + struct.pack("<H", len(name)) + n + b"\0\0" + struct.pack("<H", len(suffix))
                + m + b"\xf1\x11" + instance + b"\x00" * 33 + struct.pack("<i", slot) + b"\x00\x01\x01\x01" + tail)

    def test_reads_and_compares_the_items(self):
        a = b"head" + self.record("Sword", 20, "[ITEM] of the Bear") + self.record("Potion", 70) + self.record("Barrel", -1)
        b = b"head" + self.record("Potion", 19, instance=b"\x02" * 6) + self.record("Sword", 33, "[ITEM] of the Bear") \
            + self.record("Barrel", -1)
        with tempfile.TemporaryDirectory() as d:
            pa, pb = os.path.join(d, "a.svt"), os.path.join(d, "b.svt")
            write(pa, a)
            write(pb, b)
            _, recs = save_items.items(pa)
            self.assertEqual([(r[2], r[3], r[5]) for r in recs],
                             [("Sword", "[ITEM] of the Bear", 20), ("Potion", "", 70), ("Barrel", "", -1)])
            with unittest.mock.patch("sys.stdout"):
                self.assertEqual(save_items.main(["compare", pa, pb]), 0)   # slots and instance ids aside, the same
            write(pb, b"head" + self.record("Sword", 33, "[ITEM] of the Bear") + self.record("Barrel", -1))
            with unittest.mock.patch("sys.stdout"):
                self.assertEqual(save_items.main(["compare", pa, pb]), 1)   # the potion lost


if __name__ == "__main__":
    unittest.main()

"""Tests of the tools, on synthetic data only (no game file)."""
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import build_proxy  # noqa: E402
import check_no_game_files  # noqa: E402
import compare_installs  # noqa: E402
import compare_ui  # noqa: E402
import convert_x360_ui  # noqa: E402
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
            write(os.path.join(game, "real.dll"), synthetic_pe({}, ["?b@@XZ"]))
            src = 'sym("Ogre.dll", "?a@@XZ"); sym("Ogre.dll", "?typo@@XZ"); need("?b@@XZ");'
            self.assertEqual(build_proxy.check_names(src, game, os.path.join(game, "real.dll")),
                             ["Ogre.dll: ?typo@@XZ"])

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


if __name__ == "__main__":
    unittest.main()

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
import check_no_game_files  # noqa: E402
import compare_installs  # noqa: E402
import convert_x360_ui  # noqa: E402
import layout_windows  # noqa: E402
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
            '<Property Name="Image" Value="set:Loading image:loading" /></Falagard>',
        "media/UI/modalmenu.uilayout":
            '<GUILayout><Window Type="ceguiWidget/StaticImage" Name="Root">'
            '<Property Name="Image" Value="set:widgets1 image:a" /></Window></GUILayout>',
        "media/ui/skills.uilayout":
            '<GUILayout><Window Type="DefaultWindow" Name="R"><Window Type="DefaultWindow" Name="S">'
            '<Property Name="Image" Value="set:widgets6 image:b" /></Window></Window></GUILayout>',
        "media/UI/hud.layout": "<GUILayout />",
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
            "loading.imageset", "modalmenu.layout", "skills.layout", "widgets1.dds", "widgets1.imageset"]))
        scheme = text[d + "ceguiWidgets.scheme"]
        self.assertIn('<GUIScheme Name="X360_newHud">', scheme)
        self.assertIn('<Imageset Name="X360_Loading" Filename="media/ui/x360/loading.imageset" />', scheme)
        self.assertEqual(scheme.count('Filename="media/ui/x360/Serif.font"'), 2)
        self.assertIn('<LookNFeel Filename="media/ui/x360/ceguiWidgets.looknfeel" />', scheme)
        self.assertIn('<Imageset Name= "X360_widgets1" Imagefile="media/ui/x360/widgets1.dds">',
                      text[d + "widgets1.imageset"])
        self.assertIn('Filename="media/ui/x360/BRLNSDB.TTF"', text[d + "Serif.font"])
        self.assertIn('Name="Serif"', text[d + "Serif.font"], "font names are kept")
        self.assertEqual(text[d + "ceguiWidgets.looknfeel"],
                         '<Falagard><Image imageset="X360_widgets1" image="a" />'
                         '<Property Name="Image" Value="set:X360_Loading image:loading" /></Falagard>')
        self.assertIn('Value="set:X360_widgets1 image:a"', text[d + "modalmenu.layout"])
        self.assertIn('Value="set:widgets6 image:b"', text[d + "skills.layout"], "unknown sets are left")
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

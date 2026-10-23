"""The C++ installer's readers against the Python tools, on synthetic data (tests/test_tools.py's
builders). Skipped unless X360UI_INSTALLER names the built x360ui-install (CI builds it)."""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "xex"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stfs  # noqa: E402
from test_tools import stfs_package  # noqa: E402

BINARY = os.environ.get("X360UI_INSTALLER")
TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools")


def run(*args):
    return subprocess.run(list(args), check=True, capture_output=True).stdout


@unittest.skipUnless(BINARY and os.path.isfile(BINARY), "X360UI_INSTALLER not set")
class InstallerCppTest(unittest.TestCase):
    def test_stfs_list_and_extract_as_python(self):
        files = {"pak.zip": b"zip" * 100, "default.xex": bytes(range(256)) * 37, "a.png": b""}
        for contiguous in (True, False):
            with tempfile.TemporaryDirectory() as d:
                pkg = os.path.join(d, "pkg")
                with open(pkg, "wb") as f:
                    f.write(stfs_package(files, contiguous=contiguous))
                self.assertEqual(run(BINARY, "dev", "stfs", pkg), run(sys.executable, os.path.join(TOOLS, "stfs.py"), pkg))
                run(BINARY, "dev", "stfs", pkg, os.path.join(d, "cpp"))
                run(sys.executable, os.path.join(TOOLS, "stfs.py"), pkg, os.path.join(d, "py"))
                for name in files:
                    with open(os.path.join(d, "cpp", name), "rb") as a, open(os.path.join(d, "py", name), "rb") as b:
                        self.assertEqual(a.read(), b.read(), name)

    def test_aes_as_python(self):
        import aes128
        key, iv = bytes(range(16)), bytes(range(16, 32))
        data = bytes((i * 31) % 256 for i in range(16 * 40))
        with tempfile.TemporaryDirectory() as d:
            src, out = os.path.join(d, "in"), os.path.join(d, "out")
            with open(src, "wb") as f:
                f.write(data)
            run(BINARY, "dev", "aes-cbc", key.hex(), iv.hex(), src, out)
            with open(out, "rb") as f:
                self.assertEqual(f.read(), aes128.cbc_decrypt(key, iv, data))

    def test_lzx_as_python_on_random_streams(self):
        """Seeded random streams, biased to valid block headers (verbatim, aligned, uncompressed),
        through both decoders: the same bytes, or both an error."""
        import random
        from lzx_decompress import LZXDecoder
        rng = random.Random(1234)
        same = 0
        with tempfile.TemporaryDirectory() as d:
            kinds = {1: 0, 2: 0, 3: 0}
            for k in range(600):
                bits, size = rng.choice((15, 16, 17)), rng.randint(1, 20000)
                # The first 16-bit word is little endian: its bit 15 (data[1]'s bit 7) is the E8 flag,
                # kept 0, then the block type in data[1]'s bits 6..4: verbatim, aligned, uncompressed.
                kind = rng.choice((1, 2, 3))
                data = bytes([rng.randrange(256), (kind << 4) | rng.randrange(16)]) + bytes(rng.randrange(256) for _ in range(rng.randint(8, 3000)))
                src, out = os.path.join(d, "in"), os.path.join(d, "out")
                with open(src, "wb") as f:
                    f.write(data)
                try:
                    want = LZXDecoder(bits).decompress(data, size)
                except Exception:
                    want = None
                r = subprocess.run([BINARY, "dev", "lzx", str(bits), str(size), src, out], capture_output=True)
                if want is None:
                    self.assertNotEqual(r.returncode, 0, f"stream {k}: Python failed, C++ did not")
                else:
                    self.assertEqual(r.returncode, 0, f"stream {k}: {r.stderr!r}")
                    with open(out, "rb") as f:
                        self.assertEqual(f.read(), want, f"stream {k}")
                    same += 1
                    kinds[kind] += 1
        self.assertGreater(same, 250)   # most streams decode, so the test means something
        self.assertTrue(all(n > 50 for n in kinds.values()), kinds)   # each block type decoded

    def test_stfs_names_with_a_nul_refused(self):
        with tempfile.TemporaryDirectory() as d:
            pkg = os.path.join(d, "pkg")
            with open(pkg, "wb") as f:
                f.write(stfs_package({"pak.zip": b"good", "pak.zip\0": b"bad"}))   # the good one first: the bad would overwrite it
            r = subprocess.run([BINARY, "dev", "stfs", pkg, os.path.join(d, "out")], capture_output=True)
            self.assertNotEqual(r.returncode, 0)
            with open(os.path.join(d, "out", "pak.zip"), "rb") as f:
                self.assertEqual(f.read(), b"good")

    def test_refuses_what_is_not_a_package(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "x")
            with open(path, "wb") as f:
                f.write(b"MZ" + bytes(0x400))
            r = subprocess.run([BINARY, "dev", "stfs", path], capture_output=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn(b"not an Xbox 360 package", r.stderr)



def _dds_argb(w, h, seed):
    import random
    rng = random.Random(seed)
    head = bytearray(128)
    head[0:4] = b"DDS "
    struct.pack_into("<7I", head, 4, 124, 0x100F, h, w, w * 4, 0, 0)
    struct.pack_into("<8I", head, 76, 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    return bytes(head) + bytes(rng.randrange(256) for _ in range(w * h * 4))


def _dds_dxt(w, h, fourcc, seed):
    import random
    rng = random.Random(seed)
    head = bytearray(128)
    head[0:4] = b"DDS "
    struct.pack_into("<II", head, 12, h, w)
    struct.pack_into("<I4sI", head, 80, 0x4, fourcc, 0)
    size = 8 if fourcc == b"DXT1" else 16
    return bytes(head) + bytes(rng.randrange(256) for _ in range(((w + 3) // 4) * ((h + 3) // 4) * size))


def _adm(strings, root):
    """A Runic .adm: its string table {id: text} and a node (name id, [(key id, kind, value)], children)."""
    out = bytearray(struct.pack("<II", 0, len(strings)))
    for i, t in strings.items():
        out += struct.pack("<II", i, len(t)) + t.encode("utf-16-le")

    def node(n):
        name, props, children = n
        b = bytearray(struct.pack("<II", name, len(props)))
        for key, kind, value in props:
            b += struct.pack("<II", key, kind) + struct.pack("<Q" if kind in (3, 7) else "<I", value)
        b += struct.pack("<I", len(children))
        for c in children:
            b += node(c)
        return b
    return bytes(out + node(root))


def _image_with_strings():
    sys.path.insert(0, TOOLS)
    import convert_x360_ui as c
    image = bytearray(0xD5000)
    for k, (key, addr) in enumerate(c.STRINGS.items()):
        text = f"Synthetic {k}" if k % 7 else ("Bad=one" if k % 14 == 0 else "")   # some left out by the rules
        image[addr - c.IMAGE_BASE:addr - c.IMAGE_BASE + 2 * len(text) + 2] = text.encode("utf-16-be") + b"\0\0"
    for key, addr in c.STRINGS_UTF8.items():
        image[addr - c.IMAGE_BASE:addr - c.IMAGE_BASE + 12] = "Spéll none\0".encode("utf-8")[:12]
    return bytes(image)


@unittest.skipUnless(BINARY and os.path.isfile(BINARY), "X360UI_INSTALLER not set")
class ConvertCppTest(unittest.TestCase):
    """The converter in C++ against tools/convert_x360_ui.py on a synthetic 360 pak, PC pak and
    image: every output file the same bytes, for the six prompt variants."""

    def paks(self, root):
        from test_tools import ConvertX360Test
        files = dict(ConvertX360Test.FILES)
        # Prompt images in two textures (ARGB and DXT5), DXT1 and DXT3 ones decoded too.
        files["media/UI/icons2/widgets1.imageset"] = (
            '<Imageset Name= "widgets1" Imagefile="media/ui/icons2/widgets1.dds">'
            '<Image Name="a_button" XPos="2" YPos="3" Width="20" Height="18" />'
            '<Image Name="b_button" XPos="30" YPos="0" Width="9" Height="31" />'
            '<Image Name="controller" XPos="0" YPos="34" Width="60" Height="28" />'
            '<Image Name="dpad_up" XPos="40" YPos="40" Width="2" Height="2" />'
            '<Image Name="a" XPos="1" YPos="1" Width="1" Height="1" /></Imageset>')
        files["media/UI/icons2/widgets1.dds"] = _dds_argb(64, 64, 1)
        files["media/UI/loading.imageset"] = ('﻿<Imageset Name="Loading" Imagefile="media/ui/loading.dds">'
                                              '<Image Name="start_button" XPos="4" YPos="4" Width="17" Height="17" />'
                                              '<Image Name="left_bumper" XPos="20" YPos="8" Width="11" Height="7" /></Imageset>')
        files["media/UI/loading.dds"] = _dds_dxt(32, 24, b"DXT5", 2)
        files["media/UI/dxt1.imageset"] = '<Imageset Name="d1" Imagefile="media/ui/dxt1.dds"><Image Name="right_bumper" XPos="0" YPos="0" Width="13" Height="13" /></Imageset>'
        files["media/UI/dxt1.dds"] = _dds_dxt(16, 16, b"DXT1", 3)
        files["media/UI/dxt3.imageset"] = '<Imageset Name="d3" Imagefile="media/ui/dxt3.dds"><Image Name="y_button" XPos="1" YPos="1" Width="14" Height="14" /></Imageset>'
        files["media/UI/dxt3.dds"] = _dds_dxt(16, 16, b"DXT3", 4)
        files["media/UI/hud/schemes/ceguiWidgets.scheme"] = files["media/UI/hud/schemes/ceguiWidgets.scheme"].replace(
            '<Font Name="Serif"', '<Imageset Name="d1" Filename="media/ui/dxt1.imageset" />\n'
            '<Imageset Name="d3" Filename="media/ui/dxt3.imageset" />\n<Font Name="Serif"', 1)
        # The object label's look, for label_button_relative and label_frame_relative.
        files["media/UI/hud/schemes/ceguiWidgets.looknfeel"] = (
            '<Falagard><Image imageset="widgets1" image="a" /><Property name="Font" value="Serif" />\n'
            '<WidgetLook name="ceguiWidget/StaticShared">\n<ImagerySection name="frame">'
            '<Dim type="LeftEdge" ><AbsoluteDim value="-24" /></Dim><Dim type="TopEdge" ><AbsoluteDim value="-9" /></Dim>'
            '<AbsoluteDim value="48" /><AbsoluteDim value="18" /></ImagerySection>\n</WidgetLook>\n'
            '<WidgetLook name="ceguiWidget/ItemTextButton">\n<ImagerySection name="Button">'
            '<Dim type="LeftEdge" ><AbsoluteDim value="-20" /></Dim><Dim type="TopEdge" ><AbsoluteDim value="-1" /></Dim>'
            '<Dim type="Width" ><AbsoluteDim value="21" /></Dim><Dim type="Height" ><AbsoluteDim value="21" /></Dim>'
            '</ImagerySection>\n<Section look="ceguiWidget/StaticShared" section="frame" />\n'
            '<StateImagery name="Enabled" />\n</WidgetLook>\n</Falagard>')
        files["media/UI/modalmenu.uilayout"] = files["media/UI/modalmenu.uilayout"].encode("utf-16")
        tips = {1: "CONTEXTTIPS", 2: "TIP_WELCOME", 3: "Hello\r\nthere", 4: "TIP_STASH", 5: "Stash tip", 6: "ROOT"}
        files["media/GLOBALS.DAT.adm"] = _adm(tips, (6, [], [(1, [(2, 8, 3), (4, 8, 5), (99, 8, 3)], [])]))
        unit = lambda name, desc: _adm({1: "NAME", 2: "DESCRIPTION", 3: name, 4: desc, 5: "U"}, (5, [(1, 8, 3), (2, 8, 4)], []))
        files["media/units/items/a.adm"] = unit("Sword", "The 360's line")
        files["media/units/items/b.adm"] = unit("Same", "Same text")
        files["media/sharedtextures/automapoverlay.dds"] = b"DDS overlay"
        pak = os.path.join(root, "pak.zip")
        with __import__("zipfile").ZipFile(pak, "w") as z:
            for name, data in files.items():
                z.writestr(name, data)
        pc = os.path.join(root, "Pak.zip")
        pc_tips = {1: "CONTEXTTIPS", 2: "TIP_WELCOME", 3: "PC's welcome", 6: "ROOT"}
        with __import__("zipfile").ZipFile(pc, "w") as z:
            z.writestr("media/GLOBALS.DAT.adm", _adm(pc_tips, (6, [], [(1, [(2, 8, 3)], [])])))
            z.writestr("MEDIA/UNITS/ITEMS/A.ADM", unit("Sword", "PC's line"))
            z.writestr("media/units/items/b.adm", unit("Same", "Same text"))
            z.writestr("media/units/items/c.adm", b"broken")
        image = os.path.join(root, "default.bin")
        with open(image, "wb") as f:
            f.write(_image_with_strings())
        return pak, pc, image

    def test_six_variants_as_python(self):
        with tempfile.TemporaryDirectory() as root:
            pak, pc, image = self.paks(root)
            for style in ("xbox", "playstation3", "playstation4", "playstation5", "nintendo", "nintendo-swapped"):
                extra = ["--prompts", "nintendo", "--swap-confirm-cancel"] if style == "nintendo-swapped" else (
                    ["--prompts", style] if style != "xbox" else [])
                py, cpp = os.path.join(root, "py-" + style), os.path.join(root, "cpp-" + style)
                run(sys.executable, os.path.join(TOOLS, "convert_x360_ui.py"), pak, py, "--image", image, "--pc-pak", pc, *extra)
                run(BINARY, "dev", "convert", pak, cpp, "--image", image, "--pc-pak", pc, *extra)
                listing = lambda top: sorted(os.path.relpath(os.path.join(d, f), top) for d, _, fs in os.walk(top) for f in fs)
                self.assertEqual(listing(cpp), listing(py), style)
                for rel in listing(py):
                    with open(os.path.join(py, rel), "rb") as a, open(os.path.join(cpp, rel), "rb") as b:
                        self.assertEqual(b.read(), a.read(), f"{style}: {rel}")
                with open(os.path.join(py, "x360_ui_report.json"), encoding="utf-8") as f:
                    report = f.read()
                self.assertNotRegex(report, r"\d\.\d", "no float in the report (json's repr not matched)")
            with open(os.path.join(root, "py-nintendo", "x360_strings.txt"), encoding="utf-8") as f:
                strings = f.read()
            self.assertIn("tip.TIP_WELCOME=Hello  there", strings)
            self.assertIn("desc.Sword=~The 360's line", strings)

    def test_strings_table_as_python(self):
        sys.path.insert(0, TOOLS)
        import convert_x360_ui as c
        want = "".join(f"{k}=0x{v:08X}\n" for k, v in c.STRINGS.items()) + "".join(
            f"utf8:{k}=0x{v:08X}\n" for k, v in c.STRINGS_UTF8.items())
        self.assertEqual(run(BINARY, "dev", "strings-table").decode(), want)

    def test_stops_where_python_would_need_unicode_tables(self):
        """A character outside ASCII where re decides \\b: Python answers by its tables, the C++ stops."""
        with tempfile.TemporaryDirectory() as root:
            import zipfile
            from test_tools import ConvertX360Test
            pak = os.path.join(root, "pak.zip")
            with zipfile.ZipFile(pak, "w") as z:
                for name, data in ConvertX360Test.FILES.items():
                    if name.endswith("modalmenu.uilayout"):
                        data = data.replace('<Property Name="Font"', '<Property éName="Font"', 1)
                    z.writestr(name, data)
            r = subprocess.run([BINARY, "dev", "convert", pak, os.path.join(root, "o")], capture_output=True)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn(b"outside ASCII", r.stderr)

    def test_regressions_of_the_review(self):
        """Inputs where the C++ once differed in silence: a bad UTF-8 lead byte (both refuse), a
        32-bit DDS with a 16-bit mask (the same message in the report), a Unicode digit (the C++
        stops)."""
        import zipfile
        from test_tools import ConvertX360Test
        with tempfile.TemporaryDirectory() as root:
            def pak_with(change):
                files = dict(ConvertX360Test.FILES)
                change(files)
                path = os.path.join(root, "pak.zip")
                with zipfile.ZipFile(path, "w") as z:
                    for name, data in files.items():
                        z.writestr(name, data)
                return path

            def both(pak, *extra):
                py = subprocess.run([sys.executable, os.path.join(TOOLS, "convert_x360_ui.py"), pak, os.path.join(root, "py"), *extra], capture_output=True)
                cpp = subprocess.run([BINARY, "dev", "convert", pak, os.path.join(root, "cpp"), *extra], capture_output=True)
                return py, cpp

            pak = pak_with(lambda f: f.__setitem__("media/ui/skills.uilayout", b"<GUILayout>\xf5\x80\x80</GUILayout>"))
            py, cpp = both(pak)
            self.assertNotEqual(py.returncode, 0)
            self.assertNotEqual(cpp.returncode, 0)

            def wide_mask(f):
                d = bytearray(_dds_argb(8, 8, 5))
                struct.pack_into("<I", d, 92, 0x0000FFFF)
                f["media/UI/icons2/widgets1.dds"] = bytes(d)
                f["media/UI/icons2/widgets1.imageset"] = ('<Imageset Name= "widgets1" Imagefile="media/ui/icons2/widgets1.dds">'
                                                          '<Image Name="a_button" XPos="0" YPos="0" Width="4" Height="4" /></Imageset>')
            pak = pak_with(wide_mask)
            py, cpp = both(pak, "--prompts", "nintendo")
            self.assertEqual((py.returncode, cpp.returncode), (0, 0), cpp.stderr)
            with open(os.path.join(root, "py", "x360_ui_report.json"), "rb") as a, open(os.path.join(root, "cpp", "x360_ui_report.json"), "rb") as b:
                self.assertEqual(b.read(), a.read())

            def unicode_digit(f):
                wide_mask(f)
                f["media/UI/icons2/widgets1.dds"] = _dds_argb(8, 8, 6)
                f["media/UI/icons2/widgets1.imageset"] = f["media/UI/icons2/widgets1.imageset"].replace('XPos="0"', 'XPos="\u0661"')
            pak = pak_with(unicode_digit)
            py, cpp = both(pak, "--prompts", "nintendo")
            self.assertEqual(py.returncode, 0)
            self.assertNotEqual(cpp.returncode, 0)
            self.assertIn(b"outside ASCII", cpp.stderr)


if __name__ == "__main__":
    unittest.main()

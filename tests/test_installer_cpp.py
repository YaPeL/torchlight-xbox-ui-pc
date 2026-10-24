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
    r = subprocess.run(list(args), capture_output=True)
    if r.returncode:
        raise AssertionError(f"{args[:2]} exited {r.returncode}: {r.stderr.decode(errors='replace')[-2000:]}")
    return r.stdout


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


def _tree(root, skip=()):
    """{relative path: bytes} of a folder's files."""
    out = {}
    for d, _, files in os.walk(root):
        for f in files:
            rel = os.path.relpath(os.path.join(d, f), root)
            if not rel.startswith(skip):
                with open(os.path.join(d, f), "rb") as fh:
                    out[rel] = fh.read()
    return out


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
        pc = os.path.join(root, "pc_pak.zip")
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
        self.assertEqual(run(BINARY, "dev", "strings-table").decode().replace("\r\n", "\n"), want)   # stdout's line ends aside

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

            def unicode_digit_after_underscore(f):   # Python's int("1_\u0661") is 11
                wide_mask(f)
                f["media/UI/icons2/widgets1.dds"] = _dds_argb(8, 8, 6)
                f["media/UI/icons2/widgets1.imageset"] = f["media/UI/icons2/widgets1.imageset"].replace('XPos="0"', 'XPos="0_\u0661"')
            pak = pak_with(unicode_digit_after_underscore)
            py, cpp = both(pak, "--prompts", "nintendo")
            self.assertEqual(py.returncode, 0)
            self.assertNotEqual(cpp.returncode, 0)
            self.assertIn(b"outside ASCII", cpp.stderr)

            def short_dds(f):   # cut inside its pixels: Python's struct.error, uncaught by both
                wide_mask(f)
                f["media/UI/icons2/widgets1.dds"] = _dds_argb(8, 8, 7)[:200]
            pak = pak_with(short_dds)
            py, cpp = both(pak, "--prompts", "nintendo")
            self.assertNotEqual(py.returncode, 0)
            self.assertNotEqual(cpp.returncode, 0)

    def test_image_texts_long_or_with_signs_as_python(self):
        """Texts of 100 characters or more are skipped before isprintable (no stop); Latin-1 signs pass."""
        import convert_x360_ui as c
        with tempfile.TemporaryDirectory() as root:
            image = bytearray(_image_with_strings())
            keys = list(c.STRINGS.items())
            for k, (key, addr) in enumerate(keys[:3]):
                text = ["\u20ac" * 120, "Caf\u00e9 \u00bfs\u00ed?", "\u2014dash\u2014"][k]
                o = addr - c.IMAGE_BASE
                image[o:o + 2 * len(text) + 2] = text.encode("utf-16-be") + b"\0\0"
            from test_tools import ConvertX360Test
            import zipfile
            pak = os.path.join(root, "pak.zip")
            with zipfile.ZipFile(pak, "w") as z:
                for name, data in ConvertX360Test.FILES.items():
                    z.writestr(name, data)
            img = os.path.join(root, "img")
            with open(img, "wb") as f:
                f.write(image)
            run(sys.executable, os.path.join(TOOLS, "convert_x360_ui.py"), pak, os.path.join(root, "py"), "--image", img)
            run(BINARY, "dev", "convert", pak, os.path.join(root, "cpp"), "--image", img)
            with open(os.path.join(root, "py", "x360_strings.txt"), "rb") as a, open(os.path.join(root, "cpp", "x360_strings.txt"), "rb") as b:
                self.assertEqual(b.read(), a.read())

    def test_zip_with_data_before_it_as_python(self):
        """A zip with bytes in front (a self-extractor's shape): its records read from the archive's start."""
        import zipfile
        from test_tools import ConvertX360Test
        with tempfile.TemporaryDirectory() as root:
            plain = os.path.join(root, "plain.zip")
            with zipfile.ZipFile(plain, "w") as z:
                for name, data in ConvertX360Test.FILES.items():
                    z.writestr(name, data)
            pak = os.path.join(root, "pak.zip")
            with open(plain, "rb") as f, open(pak, "wb") as g:
                g.write(b"\x77" * 777 + f.read())
            run(sys.executable, os.path.join(TOOLS, "convert_x360_ui.py"), pak, os.path.join(root, "py"))
            run(BINARY, "dev", "convert", pak, os.path.join(root, "cpp"))
            self.assertEqual(_tree(os.path.join(root, "cpp")), _tree(os.path.join(root, "py")))

    def test_zip_unicode_path_field_as_python(self):
        """An entry whose Info-ZIP Unicode Path field (0x7075, version 1, the raw name's CRC) names
        another file: zipfile (3.12 on) takes that name; the C++ does the same."""
        import zipfile, zlib
        if sys.version_info < (3, 12):
            self.skipTest("zipfile reads the field from Python 3.12")
        from test_tools import ConvertX360Test
        with tempfile.TemporaryDirectory() as root:
            pak = os.path.join(root, "pak.zip")
            with zipfile.ZipFile(pak, "w") as z:
                for name, data in ConvertX360Test.FILES.items():
                    if name.endswith("skills.uilayout"):
                        info = zipfile.ZipInfo("media/ui/other.uilayout")
                        up = b"media/ui/renamed.uilayout"
                        info.extra = struct.pack("<HHBL", 0x7075, 5 + len(up), 1, zlib.crc32(b"media/ui/other.uilayout")) + up
                        z.writestr(info, data)
                    else:
                        z.writestr(name, data)
            run(sys.executable, os.path.join(TOOLS, "convert_x360_ui.py"), pak, os.path.join(root, "py"))
            run(BINARY, "dev", "convert", pak, os.path.join(root, "cpp"))
            d = os.path.join("media", "ui", "x360")
            self.assertTrue(os.path.isfile(os.path.join(root, "py", d, "renamed.layout")))
            self.assertEqual(sorted(os.listdir(os.path.join(root, "cpp", d))), sorted(os.listdir(os.path.join(root, "py", d))))


def _real_cegui_dll(path):
    """A PE32 whose export table is proxy/cegui_exports.txt's (names and ordinals), as the game's
    CEGUIBase.dll's: what the installers check the real DLL against."""
    sys.path.insert(0, TOOLS)
    import cegui_exports
    entries = cegui_exports.exports()
    names = [n.encode() + b"\0" for n, _ in entries]
    n = len(entries)
    base_ord = min(o for _, o in entries)
    sec_rva, sec_raw = 0x1000, 0x400
    body = bytearray(40)
    funcs_off = len(body)
    body += bytes(4 * (max(o for _, o in entries) - base_ord + 1))
    names_off = len(body)
    body += bytes(4 * n)
    ords_off = len(body)
    body += bytes(2 * n)
    dll_name_off = len(body)
    body += b"CEGUIBase.dll\0"
    for k, ((name, ordinal), raw) in enumerate(zip(entries, names)):
        struct.pack_into("<I", body, names_off + 4 * k, sec_rva + len(body))
        struct.pack_into("<H", body, ords_off + 2 * k, ordinal - base_ord)
        body += raw
    struct.pack_into("<IIHHIIIIIII", body, 0, 0, 0, 0, 0, sec_rva + dll_name_off, base_ord,
                     max(o for _, o in entries) - base_ord + 1, n, sec_rva + funcs_off, sec_rva + names_off, sec_rva + ords_off)
    head = bytearray(sec_raw)
    head[0:2] = b"MZ"
    struct.pack_into("<I", head, 0x3C, 0x40)
    head[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", head, 0x44, 0x14C, 1, 0, 0, 0, 0xE0, 0x2102)
    opt = 0x58
    struct.pack_into("<H", head, opt, 0x10B)
    struct.pack_into("<I", head, opt + 92, 16)                       # NumberOfRvaAndSizes
    struct.pack_into("<II", head, opt + 96, sec_rva, len(body))        # the export directory
    sect = opt + 0xE0
    head[sect:sect + 8] = b".edata\0\0"
    struct.pack_into("<IIII", head, sect + 8, len(body), sec_rva, len(body), sec_raw)
    with open(path, "wb") as f:
        f.write(bytes(head) + bytes(body))


@unittest.skipUnless(BINARY and os.path.isfile(BINARY), "X360UI_INSTALLER not set")
class InstallCppTest(unittest.TestCase):
    """The C++ installer against tools/install.py on fake games (a Wine prefix's GOG folder with
    saves beside it): the same files written, the same ini and backups, and an uninstall that
    leaves the folder as it was; then what only the C++ one does (undoing a failed install, a
    folder it may not write)."""

    def setUp(self):
        from test_tools import fake_exe
        self.root = os.path.realpath(tempfile.mkdtemp())     # Windows: the long form the installers write
        self.addCleanup(__import__("shutil").rmtree, self.root, True)
        pak, pc, image = ConvertCppTest().paks(self.root)
        import zipfile
        with zipfile.ZipFile(pak, "a") as z:
            z.writestr("media/UI/mastermenu_xenon.layout", "<GUILayout />")
        self.x360 = os.path.join(self.root, "x360")
        os.makedirs(self.x360)
        os.replace(pak, os.path.join(self.x360, "pak.zip"))
        with open(image, "rb") as f:
            data = bytearray(f.read())
        data[0:2] = b"MZ"                        # an image is a PE (the installers check)
        with open(os.path.join(self.x360, "default.bin"), "wb") as f:
            f.write(data)
        self.proxy = os.path.join(self.root, "proxy.dll")
        with open(self.proxy, "wb") as f:
            f.write(b"MZ a synthetic proxy forwarding to CEGUIBase_orig.Thing")
        self.games = {}
        for who in ("py", "cpp"):
            prefix = os.path.join(self.root, who, "prefix")
            game = os.path.join(prefix, "drive_c", "GOG Games", "Torchlight")
            os.makedirs(game)
            fake_exe(os.path.join(game, "Torchlight.exe"), 0x4FFC6815)
            _real_cegui_dll(os.path.join(game, "CEGUIBase.dll"))
            __import__("shutil").copyfile(pc, os.path.join(game, "Pak.zip"))
            saves = os.path.join(prefix, "drive_c", "users", "me", "AppData", "Roaming", "runic games", "torchlight", "save")
            os.makedirs(saves)
            with open(os.path.join(saves, "0.svt"), "wb") as f:
                f.write(b"a synthetic save")
            self.games[who] = (game, saves)
        self.env = dict(os.environ, HOME=self.root, USERPROFILE=self.root)   # USERPROFILE: Windows' home

    def install(self, who, *args, env=None):
        game = self.games[who][0]
        common = ["--game", game, "--x360-dir", self.x360, "--yes", "--console", "--dll", self.proxy, *args]
        cmd = [sys.executable, os.path.join(TOOLS, "install.py"), "install", *common] if who == "py" else [BINARY, "install", *common]
        return subprocess.run(cmd, capture_output=True, text=True, env=env or self.env)

    def uninstall(self, who, *args):
        game = self.games[who][0]
        cmd = [sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]
        return subprocess.run(cmd + ["uninstall", "--game", game, "--console", *args], capture_output=True, text=True, env=self.env)

    def same_install(self):
        py, cpp = _tree(self.games["py"][0]), _tree(self.games["cpp"][0])
        ini = "x360_ui.ini"
        out = lambda t, who: t[ini].replace(self.games[who][0].replace("/", "\\").encode(), b"GAME")
        self.assertEqual(out(cpp, "cpp"), out(py, "py"))
        del py[ini], cpp[ini]
        self.assertEqual(sorted(cpp), sorted(py))
        for rel in py:
            self.assertEqual(cpp[rel], py[rel], rel)

    def test_the_wizards_questions(self):
        """What the wizard asks before it runs the installer (dev games, game-in, package-check,
        saves): the games found with their mod and bag through an install with the 360's bag, the
        first uninstall's switch to PC's bag and the uninstall; the 360 copy checked."""
        dev = lambda *a: subprocess.run([BINARY, "dev", *a], capture_output=True, text=True, env=self.env)
        game, saves = self.games["cpp"]
        rows = lambda: {l.split("\t")[1]: l.split("\t")[2:] for l in dev("games").stdout.splitlines()}
        self.assertEqual(rows()[os.path.realpath(game)], ["1", "0", "", "0", "0"])
        self.assertEqual(dev("game-in", os.path.join(game, "Torchlight.exe")).stdout.split("\t")[1], os.path.realpath(game))
        self.assertEqual(dev("game-in", self.root).stdout.split("\t")[1], "")       # not a Torchlight
        self.assertEqual(dev("saves", game).stdout.splitlines(), [os.path.realpath(saves)])
        self.assertEqual(self.install("cpp", "--bag", "360").returncode, 0)
        self.assertEqual(rows()[os.path.realpath(game)], ["1", "1", "360", "1", "0"])
        self.assertEqual(self.uninstall("cpp", "--yes").returncode, 0)               # step 1: PC's bag
        self.assertEqual(rows()[os.path.realpath(game)], ["1", "1", "pc", "1", "0"])
        self.assertEqual(self.uninstall("cpp", "--yes", "--anyway").returncode, 0)
        self.assertEqual(rows()[os.path.realpath(game)], ["1", "0", "", "0", "0"])
        r = dev("package-check", self.x360)
        self.assertEqual((r.returncode, r.stdout.strip()), (0, "ok"))
        r = dev("package-check", os.path.join(self.x360, "pak.zip"))                  # a file of the folder
        self.assertEqual((r.returncode, r.stdout.strip()), (0, "ok"))
        junk = os.path.join(self.root, "other", "file.bin")
        os.makedirs(os.path.dirname(junk))
        with open(junk, "wb") as f:
            f.write(b"not a package")
        r = dev("package-check", junk)                                                # not a package
        self.assertEqual(r.returncode, 1)
        self.assertIn("No pak.zip in", r.stdout)
        # PC's pak.zip first, the 360's in a subfolder: the first found is the one the install takes.
        mixed = os.path.join(self.root, "mixed")
        __import__("shutil").copytree(self.x360, os.path.join(mixed, "sub"))
        __import__("shutil").copyfile(os.path.join(game, "Pak.zip"), os.path.join(mixed, "pak.zip"))
        r = dev("package-check", mixed)
        self.assertEqual(r.returncode, 1)
        self.assertIn("is not the Xbox 360 game's", r.stdout)
        self.assertNotEqual(self.install("cpp", "--bag", "pc", "--x360-dir", mixed).returncode, 0)
        r = dev("package-check", os.path.join(self.root, "nothing"))
        self.assertEqual(r.returncode, 1)
        self.assertIn("does not exist", r.stdout)

    def test_progress_file(self):
        """--progress-file (the elevated run's, read by the first): its last line once installed
        and once uninstalled."""
        f = os.path.join(self.root, "progress.txt")
        self.assertEqual(self.install("cpp", "--bag", "pc", "--progress-file", f).returncode, 0)
        with open(f) as p:
            self.assertEqual(p.read(), "1.0000\tInstalled\n")
        self.assertEqual(self.uninstall("cpp", "--yes", "--progress-file", f).returncode, 0)
        with open(f) as p:
            self.assertEqual(p.read(), "1.0000\tUninstalled\n")

    def wizard(self, steps, *extra, shots=None, native=False):
        """The wizard (dev wizard) on SDL's offscreen video and software renderer (native: the
        system's window and renderer), played by a script with a simulated pad; the home has only
        the cpp game."""
        script = os.path.join(self.root, "script.txt")
        with open(script, "w") as f:
            f.write("\n".join(steps) + "\n")
        home = os.path.join(self.root, "cpp")
        video = [] if native else ["--video", "offscreen", "--software"]
        cmd = [BINARY, "dev", "wizard", *video, "--own-picker", "--script", script,
               "--browse-from", self.x360, "--dll", self.proxy, *extra]
        if shots:
            cmd += ["--shots", shots]
        return subprocess.run(cmd, capture_output=True, text=True, timeout=600,
                              env=dict(self.env, HOME=home, USERPROFILE=home))

    # The pad's way through each flow (the own browser lists "..", default.bin, pak.zip).
    WIZARD_INSTALL_360 = ["screen games", "press a", "screen package", "press a", "screen browser", "press down",
                          "press down", "press a", "screen package", "press start", "screen bag", "press down",
                          "press a", "press start", "screen summary", "press start", "screen result 300"]
    WIZARD_TO_PC_BAG = ["screen games", "press a", "screen action", "press down", "press down", "press a",
                        "screen uninstall", "press a", "screen summary", "press start", "screen result 120"]
    WIZARD_UNINSTALL_AFTER_360 = ["screen games", "press a", "screen action", "press down", "press down", "press a",
                                  "screen uninstall", "press a", "press down", "press a", "screen summary",
                                  "press start", "screen result 120"]

    def test_the_wizard_end_to_end(self):
        """The whole wizard with a simulated pad: the 360's bag installed through the package
        found in its own browser, the same bytes as install.py's; then the two steps of the
        uninstall (PC's bag, then the uninstall once each character is loaded and saved), the
        game's folder as it was before."""
        game = self.games["cpp"][0]
        before = _tree(game)
        r = self.wizard(self.WIZARD_INSTALL_360 + ["quit"])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(self.install("py", "--bag", "360").returncode, 0)
        self.same_install()
        backups = [d for d in os.listdir(os.path.dirname(self.games["cpp"][1])) if "-backup-" in d]
        self.assertEqual(len(backups), 1)
        r = self.wizard(self.WIZARD_TO_PC_BAG + ["quit"])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        with open(os.path.join(game, "x360_ui.ini")) as f:
            self.assertIn("bag=pc", f.read())
        r = self.wizard(self.WIZARD_UNINSTALL_AFTER_360 + ["quit"])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(_tree(game), before)

    def test_the_wizard_asks_before_the_360_bag_without_a_backup(self):
        """A game with the mod on PC's bag and no save folder found: "Change to the 360's bag" goes
        through the bag's screen (its warning), and without the no-backup box ticked Start does
        not leave it; nothing is changed."""
        import shutil
        self.assertEqual(self.install("cpp", "--bag", "pc").returncode, 0)
        shutil.rmtree(self.games["cpp"][1])
        steps = ["screen games", "press a", "screen action", "press down", "press a", "screen package", "press a",
                 "screen browser", "press down", "press down", "press a", "screen package", "press start",
                 "screen bag", "press start", "wait 10", "screen bag 1", "quit"]
        r = self.wizard(steps)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        with open(os.path.join(self.games["cpp"][0], "x360_ui.ini")) as f:
            self.assertIn("bag=pc", f.read())

    # Every screen, each shot once its screen is reached (the progress only if it still shows).
    WIZARD_SHOTS = ["screen games", "shot 01-games", "press a", "screen package", "shot 02-package", "press a",
                    "screen browser", "shot 03-browser", "press down", "press down", "press a", "screen package",
                    "shot 04-package-chosen", "press start", "screen bag", "shot 05-bag-pc", "press down", "press a",
                    "wait 3", "shot 06-bag-360", "press start", "screen summary", "shot 07-summary", "press start",
                    "shot-if working 08-working", "screen result 300", "shot 09-result", "press start", "screen games",
                    "shot 10-games-installed", "press a", "screen action", "shot 11-action", "press down",
                    "press down", "press a", "screen uninstall", "shot 12-uninstall-360-bag", "press b",
                    "screen action", "press b", "screen games", "quit"]

    def test_the_wizards_screenshots(self):
        """Every screen drawn, each reached with the simulated pad; with X360UI_SHOTS (a folder),
        their PNGs at 960x600, at 1280x800 (the Steam Deck) and at scale 2, which CI keeps."""
        out = os.environ.get("X360UI_SHOTS")
        sizes = [("960x600", "1"), ("1280x800", "1"), ("960x600", "2")] if out else [("960x600", "1")]
        for size, scale in sizes:
            name = f"{size}-x{scale}"
            folder = os.path.join(out, name) if out else os.path.join(self.root, "shots")
            os.makedirs(folder, exist_ok=True)
            # Windows' own window and renderer for its shots; offscreen where there is no desktop.
            r = self.wizard(self.WIZARD_SHOTS, "--size", size, "--scale", scale, shots=folder,
                            native=bool(out) and os.name == "nt")
            if r.returncode == 2:
                os.rename(folder, folder + "-offscreen")
                folder += "-offscreen"
                r = self.wizard(self.WIZARD_SHOTS, "--size", size, "--scale", scale, shots=folder)
            self.assertEqual(r.returncode, 0, name + r.stdout + r.stderr)
            self.assertIn("12-uninstall-360-bag.png", os.listdir(folder))
            self.assertEqual(self.uninstall("cpp", "--yes", "--anyway").returncode, 0)

    def test_install_and_uninstall_as_python(self):
        before = {who: _tree(g) for who, (g, _) in self.games.items()}
        for bag in ("pc", "360"):
            for who in ("py", "cpp"):
                r = self.install(who, "--bag", bag)
                self.assertEqual(r.returncode, 0, f"{who} {bag}: {r.stdout}{r.stderr}")
            self.same_install()
        backups = {who: sorted(d for d in os.listdir(os.path.dirname(s)) if "-backup-" in d) for who, (_, s) in self.games.items()}
        self.assertEqual([len(b) for b in backups.values()], [1, 1])
        for who, (_, saves) in self.games.items():
            self.assertEqual(_tree(os.path.join(os.path.dirname(saves), backups[who][0])), {"0.svt": b"a synthetic save"})
        # The 360's bag installed: uninstall with --yes switches to PC's bag in both, then --anyway uninstalls.
        for who in ("py", "cpp"):
            self.assertEqual(self.uninstall(who, "--yes").returncode, 0)
        self.same_install()
        for who in ("py", "cpp"):
            r = self.uninstall(who, "--anyway")
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertEqual(_tree(self.games[who][0]), before[who], f"{who}: the folder as it was")

    def test_uninstall_anyway_with_the_360_bag_backs_up_as_python(self):
        """The 360's bag still installed: a no to switching bags, then a yes to uninstalling anyway;
        the saves backed up first, as install.py does, and the folder as it was."""
        before = {who: _tree(g) for who, (g, _) in self.games.items()}
        for who in ("py", "cpp"):
            self.assertEqual(self.install(who, "--bag", "360").returncode, 0)
            saves = self.games[who][1]
            for d in os.listdir(os.path.dirname(saves)):
                if "-backup-" in d:
                    __import__("shutil").rmtree(os.path.join(os.path.dirname(saves), d))
        for who in ("py", "cpp"):
            game = self.games[who][0]
            cmd = [sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]
            r = subprocess.run(cmd + ["uninstall", "--game", game, "--console"], capture_output=True, text=True,
                               env=self.env, input="n\ny\n")
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            self.assertEqual(_tree(game), before[who], f"{who}: the folder as it was")
        for who, (_, saves) in self.games.items():
            backups = [d for d in os.listdir(os.path.dirname(saves)) if "-backup-" in d]
            self.assertEqual(len(backups), 1, who)
            self.assertEqual(_tree(os.path.join(os.path.dirname(saves), backups[0])), {"0.svt": b"a synthetic save"})

    def test_the_360_bag_remembered_until_uninstall(self):
        """The report's case: the 360's bag, then PC's (uninstall's switch), then uninstall: still a
        question and a backup, until the player says each character was loaded and saved with PC's
        bag; the same in both installers (ini, backups, the folder as it was)."""
        before = {who: _tree(g) for who, (g, _) in self.games.items()}
        cmd = lambda who: [sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]
        for who in ("py", "cpp"):
            self.assertEqual(self.install(who, "--bag", "360").returncode, 0)
            self.assertEqual(self.uninstall(who, "--yes").returncode, 0)            # switched to PC's bag
            game, saves = self.games[who]
            for d in os.listdir(os.path.dirname(saves)):
                if "-backup-" in d:
                    __import__("shutil").rmtree(os.path.join(os.path.dirname(saves), d))
        self.same_install()
        for who in ("py", "cpp"):                                                       # a reinstall keeps it
            self.assertEqual(self.install(who, "--bag", "pc").returncode, 0)
        self.same_install()
        for who in ("py", "cpp"):
            with open(os.path.join(self.games[who][0], "x360_ui.ini"), "rb") as f:
                ini = f.read()
            self.assertIn(b"bag=pc\r\nbag_360_used=1\r\n", ini, who)
        no_backup = lambda saves: not any("-backup-" in d for d in os.listdir(os.path.dirname(saves)))
        for who in ("py", "cpp"):
            game, saves = self.games[who]
            installed = _tree(game)
            r = subprocess.run(cmd(who) + ["uninstall", "--game", game, "--console"], capture_output=True, text=True,
                               env=self.env, input="n\n")
            self.assertNotEqual(r.returncode, 0, who)                                  # not confirmed: not uninstalled
            self.assertEqual(_tree(game), installed, who)
            self.assertTrue(no_backup(saves), who)
            r = subprocess.run(cmd(who) + ["uninstall", "--game", game, "--yes", "--console"], capture_output=True,
                               text=True, env=self.env)
            self.assertNotEqual(r.returncode, 0, who)                                  # --yes: the default, no
            self.assertEqual(_tree(game), installed, who)
            self.assertTrue(no_backup(saves), who)
            r = subprocess.run(cmd(who) + ["uninstall", "--game", game, "--console"], capture_output=True, text=True,
                               env=self.env, input="y\n")
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            self.assertEqual(_tree(game), before[who], who)
            backups = [d for d in os.listdir(os.path.dirname(saves)) if "-backup-" in d]
            self.assertEqual(len(backups), 1, who)
            self.assertEqual(_tree(os.path.join(os.path.dirname(saves), backups[0])), {"0.svt": b"a synthetic save"})

    def test_test_options_not_kept(self):
        """The proxy's test options (save_root moves the saves where no backup looks, hold_saves holds
        every save): dropped by an install in both installers, the user's other lines kept."""
        text = (b"save_root=Z:\\somewhere\r\n hold_saves=1\r\ntest_no_stop =1\r\npad_detect=057e\r\n"
                b"console=MONEY 1\r\ntest_bare\r\nfont_diff=1\r\ndialog_markup=1\r\n")
        for who in ("py", "cpp"):
            with open(os.path.join(self.games[who][0], "x360_ui.ini"), "wb") as f:
                f.write(text)
            r = self.install(who, "--bag", "pc")
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            with open(os.path.join(self.games[who][0], "x360_ui.ini"), "rb") as f:
                ini = f.read()
            for key in (b"save_root", b"hold_saves", b"test_no_stop", b"pad_detect", b"console", b"test_bare"):
                self.assertNotIn(key, ini, who)
            self.assertTrue(ini.endswith(b"font_diff=1\r\ndialog_markup=1\r\n"), who)
        self.same_install()

    def test_uninstall_removes_the_mods_data_folder(self):
        """The proxy's %LOCALAPPDATA%\\x360ui (x360_stop.txt): removed by both uninstallers."""
        for who in ("py", "cpp"):
            game, saves = self.games[who]
            local = os.path.join(saves.split(os.sep + "Roaming" + os.sep)[0], "Local", "x360ui")
            self.assertEqual(self.install(who, "--bag", "pc").returncode, 0)
            os.makedirs(local)
            with open(os.path.join(local, "x360_stop.txt"), "w") as f:
                f.write("a stop's text")
            r = self.uninstall(who, "--yes")
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            self.assertFalse(os.path.exists(local), who)
            self.assertTrue(os.path.isdir(os.path.dirname(local)), who)               # AppData/Local itself kept

    def test_two_games_shown_and_chosen(self):
        """Two supported games found: both installers name them, in the order they are found (the
        folders' own order, the same in both); --yes takes the first and says so; on the console a
        no to the first and a yes to the second installs into the second; a no to both installs
        nothing; uninstall offers only the game that has the mod."""
        import shutil
        home = os.path.join(self.root, "home2")
        games = [os.path.join(home, n, "p", "x", "drive_c", "GOG Games", "Torchlight") for n in ("a", "b")]
        env = dict(self.env, HOME=home, USERPROFILE=home)
        cmd = lambda who: [sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]
        outs = {}
        for who in ("py", "cpp"):
            shutil.rmtree(home, ignore_errors=True)
            for g in games:
                shutil.copytree(self.games["py"][0], g)
            common = ["install", "--x360-dir", self.x360, "--console", "--dll", self.proxy, "--bag", "pc"]
            r = subprocess.run(cmd(who) + common + ["--yes"], capture_output=True, text=True, env=env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            outs[who] = [l for l in r.stdout.splitlines() if "supported PC Torchlights" in l or l.startswith(("gog: ", "using "))]
            order = [l[len("gog: "):] for l in outs[who] if l.startswith("gog: ")]   # as found
            self.assertEqual(sorted(order), sorted(os.path.realpath(g) for g in games), who)
            self.assertIn("x360_ui.ini", os.listdir(order[0]), who)                   # --yes: the first
            self.assertNotIn("x360_ui.ini", os.listdir(order[1]), who)
            shutil.rmtree(order[0]); shutil.copytree(self.games["py"][0], order[0])
            r = subprocess.run(cmd(who) + common, capture_output=True, text=True, env=env, input="n\nn\n")
            self.assertNotEqual(r.returncode, 0, who)                                  # a no to both
            self.assertIn("No game chosen", r.stdout + r.stderr, who)
            self.assertFalse(any("x360_ui.ini" in os.listdir(g) for g in order), who)
            r = subprocess.run(cmd(who) + common, capture_output=True, text=True, env=env, input="n\ny\n")
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            outs[who] += [l for l in r.stdout.split("\n") if "Use " in l]               # the questions, as shown
            self.assertNotIn("x360_ui.ini", os.listdir(order[0]), who)                # no to the first, yes to the second
            self.assertIn("x360_ui.ini", os.listdir(order[1]), who)
            before = _tree(self.games["py"][0])
            r = subprocess.run(cmd(who) + ["uninstall", "--console", "--yes"], capture_output=True, text=True, env=env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)               # only the one with the mod offered
            self.assertEqual(_tree(order[1]), before, who)
        self.assertEqual(outs["cpp"], outs["py"])
        self.assertEqual(len([l for l in outs["py"] if l.startswith("gog: ")]), 2)

    def test_uninstall_removes_steams_prefix_data_folder(self):
        """Steam under Proton (a fake library): the mod's own folder in compatdata/41500's
        AppData/Local is removed by both uninstallers, and nothing else of the prefix is touched."""
        import shutil
        from test_tools import fake_exe
        for who in ("py", "cpp"):
            apps = os.path.join(self.root, "lib-" + who, "steamapps")
            game = os.path.join(apps, "common", "Torchlight")
            os.makedirs(game)
            fake_exe(os.path.join(game, "Torchlight.exe"), 0x4B5F7519)
            _real_cegui_dll(os.path.join(game, "CEGUIBase.dll"))
            shutil.copyfile(os.path.join(self.games["py"][0], "Pak.zip"), os.path.join(game, "Pak.zip"))
            local = os.path.join(apps, "compatdata", "41500", "pfx", "drive_c", "users", "steamuser", "AppData", "Local")
            os.makedirs(os.path.join(local, "x360ui"))
            with open(os.path.join(local, "x360ui", "x360_ui.log"), "w") as f:
                f.write("a log")
            with open(os.path.join(local, "other.txt"), "w") as f:
                f.write("not the mod's")
            cmd = ([sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY])
            r = subprocess.run(cmd + ["install", "--game", game, "--x360-dir", self.x360, "--yes", "--console",
                                      "--dll", self.proxy, "--bag", "pc"], capture_output=True, text=True, env=self.env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            r = subprocess.run(cmd + ["uninstall", "--game", game, "--yes", "--console"], capture_output=True, text=True, env=self.env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            self.assertFalse(os.path.exists(os.path.join(local, "x360ui")), who)
            self.assertTrue(os.path.isfile(os.path.join(local, "other.txt")), who)

    def test_uninstall_without_saves_found_asks(self):
        """A Steam install under Proton (a fake library): its saves are in Steam's prefix, which the
        installers leave alone. Uninstalling after the 360's bag says no save folder was found and
        asks (no by default) before going on without a backup; --anyway goes on and says so."""
        import shutil
        from test_tools import fake_exe
        for who in ("py", "cpp"):
            apps = os.path.join(self.root, "nosave-" + who, "steamapps")
            game = os.path.join(apps, "common", "Torchlight")
            os.makedirs(game)
            fake_exe(os.path.join(game, "Torchlight.exe"), 0x4B5F7519)
            _real_cegui_dll(os.path.join(game, "CEGUIBase.dll"))
            shutil.copyfile(os.path.join(self.games["py"][0], "Pak.zip"), os.path.join(game, "Pak.zip"))
            cmd = [sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]
            r = subprocess.run(cmd + ["install", "--game", game, "--x360-dir", self.x360, "--yes", "--console",
                                      "--dll", self.proxy, "--bag", "pc"], capture_output=True, text=True, env=self.env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            ini = os.path.join(game, "x360_ui.ini")
            with open(ini, "ab") as f:
                f.write(b"bag_360_used=1\r\n")
            installed = _tree(game)
            u = cmd + ["uninstall", "--game", game, "--console"]
            r = subprocess.run(u, capture_output=True, text=True, env=self.env, input="y\nn\n")
            self.assertNotEqual(r.returncode, 0, who)
            self.assertIn("No save folder of this game was found", r.stdout, who)
            self.assertEqual(_tree(game), installed, who)
            r = subprocess.run(u + ["--anyway"], capture_output=True, text=True, env=self.env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            self.assertIn("none backed up", r.stdout, who)
            self.assertNotIn("x360_ui.ini", os.listdir(game), who)

    def test_notices_carried(self):
        """`notices`: the licence and the third-party notices built into the executable (miniz's MIT
        notice must go with every copy of it), the same texts as the repository's files."""
        r = subprocess.run([BINARY, "notices"], capture_output=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
        for rel in ("LICENSE", "installer/third_party/NOTICES.md", "installer/third_party/LGPL-2.1.txt",
                    "installer/third_party/OFL-1.1.txt", "installer/third_party/SDL-NOTICES.md"):
            with open(os.path.join(root, rel), "rb") as f:
                self.assertIn(f.read().replace(b"\r\n", b"\n"), r.stdout.replace(b"\r\n", b"\n"), rel)

    def test_an_ini_read_as_python(self):
        """An ini of the user's with bytes outside UTF-8, a lone CR, Unicode spaces and an owned key:
        rewritten as install.py rewrites it."""
        text = b"\xff\xfekey=1\r\nlone\rcr=2\n\xc2\xa0bag\xe2\x80\x83=360\n\xe3\x80\x80\n\xed\xa0\x80x=\xe2\x82\nlast=\xf0\x9f\x99\x82"
        for who in ("py", "cpp"):
            with open(os.path.join(self.games[who][0], "x360_ui.ini"), "wb") as f:
                f.write(text)
            r = self.install(who, "--bag", "pc")
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
        self.same_install()

    def test_without_a_proxy_nothing_is_converted(self):
        """No --dll and none built in: refused before the conversion, nothing left in the folder."""
        game = self.games["cpp"][0]
        before = sorted(os.listdir(game))
        common = ["--game", game, "--x360-dir", self.x360, "--yes", "--console", "--bag", "pc"]
        r = subprocess.run([BINARY, "install", *common], capture_output=True, text=True, env=self.env)
        if r.returncode == 0:
            self.skipTest("this build has its proxies built in")
        self.assertIn("no proxy", r.stderr)
        self.assertNotIn("converting", r.stdout)
        self.assertEqual(sorted(os.listdir(game)), before)

    def test_the_elevated_run_keeps_the_first_runs_answers(self):
        """What the first run passes to the elevated one: the save folders it found (none here) and the
        yes to no backup keep the 360's bag; the result is written where the first run reads it."""
        game = self.games["cpp"][0]
        result = os.path.join(self.root, "result.txt")
        r = self.install("cpp", "--bag", "360", "--elevated", "--saves-known", "--no-backup-ok", "--result-file", result)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        with open(os.path.join(game, "x360_ui.ini"), "rb") as f:
            self.assertIn(b"bag=360\r\n", f.read())
        self.assertNotIn("-backup-", " ".join(os.listdir(os.path.dirname(self.games["cpp"][1]))))
        with open(result, encoding="utf-8") as f:
            self.assertTrue(f.read().startswith("Installed."))
        r = self.install("cpp", "--bag", "360", "--elevated", "--saves-known", "--result-file", result,
                         env=dict(self.env, X360UI_TEST_FAIL_AFTER_PROXY="1"))
        self.assertNotEqual(r.returncode, 0)
        with open(result, encoding="utf-8") as f:
            self.assertIn("a failure asked for by a test", f.read())

    def test_a_failed_install_leaves_the_folder_as_it_was(self):
        game = self.games["cpp"][0]
        before = _tree(game)
        r = self.install("cpp", "--bag", "pc", env=dict(self.env, X360UI_TEST_FAIL_AFTER_PROXY="1"))
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(_tree(game), before)
        self.assertEqual(sorted(os.listdir(game)), sorted(before))   # no work, partial or undo folder left
        # Installed, then a failed uninstall: the install as it was.
        self.assertEqual(self.install("cpp", "--bag", "pc").returncode, 0)
        installed = _tree(game)
        __import__("shutil").copytree(game, os.path.join(self.root, "installed-copy"))
        r = subprocess.run([BINARY, "uninstall", "--game", game, "--yes", "--console"], capture_output=True, text=True,
                           env=dict(self.env, X360UI_TEST_FAIL_AFTER_PROXY="1"))
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(_tree(game), installed)
        # A rollback that cannot put an original back: the undo folder kept (marked), the message says where,
        # and an uninstall leaves it.
        r = self.install("cpp", "--bag", "pc", env=dict(self.env, X360UI_TEST_FAIL_AFTER_PROXY="1", X360UI_TEST_FAIL_UNDO="1"))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("Not all could be undone", r.stderr)
        kept = [n for n in os.listdir(game) if n.startswith("x360ui-undo-")]
        self.assertEqual(len(kept), 1)
        self.assertIn(kept[0], r.stderr)
        self.assertTrue(os.path.isfile(os.path.join(game, kept[0], "x360ui-keep.txt")))
        shutil = __import__("shutil")
        shutil.rmtree(game)
        shutil.copytree(os.path.join(self.root, "installed-copy"), game)
        # An undo folder a commit could not remove (a file in use): gone after an uninstall.
        os.makedirs(os.path.join(game, "x360ui-undo-123"))
        with open(os.path.join(game, "x360ui-undo-123", "0"), "wb") as f:
            f.write(b"an old proxy")
        marked = os.path.join(game, "x360ui-undo-456")
        os.makedirs(marked)
        with open(os.path.join(marked, "x360ui-keep.txt"), "wb") as f:
            f.write(b"kept")
        r = subprocess.run([BINARY, "uninstall", "--game", game, "--yes", "--console"], capture_output=True, text=True, env=self.env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertTrue(os.path.isdir(marked))                         # a marked one is the user's to look at
        self.assertIn(marked, r.stdout)
        __import__("shutil").rmtree(marked)
        self.assertEqual(_tree(game), before)
        self.assertEqual(sorted(os.listdir(game)), sorted(before))

    @unittest.skipUnless(os.name == "nt" and os.environ.get("CI"), "Windows' registry, on CI only (it sets the user's SteamPath)")
    def test_steam_found_from_the_registry(self):
        """A Steam installed elsewhere than Program Files: both installers find its game through
        SteamPath (HKCU\\Software\\Valve\\Steam), the same `list`; the value is put back after."""
        import shutil, winreg
        from test_tools import fake_exe
        steam = os.path.join(self.root, "Elsewhere", "Steam")
        game = os.path.join(steam, "steamapps", "common", "Torchlight")
        os.makedirs(game)
        open(os.path.join(steam, "steamapps", "appmanifest_41500.acf"), "w").close()
        fake_exe(os.path.join(game, "Torchlight.exe"), 0x4B5F7519)
        _real_cegui_dll(os.path.join(game, "CEGUIBase.dll"))
        shutil.copyfile(os.path.join(self.games["py"][0], "Pak.zip"), os.path.join(game, "Pak.zip"))
        key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam")
        try:
            old = winreg.QueryValueEx(key, "SteamPath")
        except OSError:
            old = None
        winreg.SetValueEx(key, "SteamPath", 0, winreg.REG_SZ, steam.replace("\\", "/"))   # as Steam writes it
        try:
            lst = lambda cmd: subprocess.run(cmd + ["list"], capture_output=True, text=True, env=self.env).stdout
            py_list = lst([sys.executable, os.path.join(TOOLS, "install.py")])
            self.assertIn("steam: ", py_list)
            self.assertIn(os.path.realpath(game).lower(), py_list.lower())
            self.assertEqual(lst([BINARY]).lower(), py_list.lower())
        finally:
            if old is None:
                winreg.DeleteValue(key, "SteamPath")
            else:
                winreg.SetValueEx(key, "SteamPath", 0, old[1], old[0])
            winreg.CloseKey(key)

    def test_stores_found_and_steam_and_epic_installed_as_python(self):
        """A home with a Steam library (Steam's build) and a Legendary (Epic) install of GOG's
        build beside the Wine prefix's GOG one: `list` the same; Steam's installs with no question in
        both, and is the one `--yes` takes with no --game (the owner: Steam's first); Epic's installs
        as GOG's."""
        import json, shutil
        from test_tools import fake_exe
        steam_root = os.path.join(self.root, ".steam", "steam", "steamapps")
        steam_game = os.path.join(steam_root, "common", "Torchlight")
        os.makedirs(steam_game)
        open(os.path.join(steam_root, "appmanifest_41500.acf"), "w").close()
        fake_exe(os.path.join(steam_game, "Torchlight.exe"), 0x4B5F7519)
        _real_cegui_dll(os.path.join(steam_game, "CEGUIBase.dll"))
        shutil.copyfile(os.path.join(self.games["py"][0], "Pak.zip"), os.path.join(steam_game, "Pak.zip"))
        epic_game = os.path.join(self.root, "Epic", "Torchlight")
        shutil.copytree(self.games["cpp"][0], epic_game)
        os.makedirs(os.path.join(self.root, ".config", "legendary"))
        with open(os.path.join(self.root, ".config", "legendary", "installed.json"), "w") as f:
            json.dump({"Torchlight": {"title": "Torchlight", "install_path": epic_game}}, f)
        lst = lambda cmd: subprocess.run(cmd + ["list"], capture_output=True, text=True, env=self.env).stdout
        py_list = lst([sys.executable, os.path.join(TOOLS, "install.py")])
        self.assertEqual(lst([BINARY]), py_list)
        self.assertIn("steam: ", py_list)
        self.assertIn("epic: ", py_list)
        took = {}
        for who in ("py", "cpp"):                                     # no --game: Steam's first
            cmd = ([sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]) + [
                "install", "--x360-dir", self.x360, "--yes", "--console", "--dll", self.proxy, "--bag", "pc"]
            r = subprocess.run(cmd, capture_output=True, text=True, env=self.env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            took[who] = [l for l in r.stdout.splitlines() if l.startswith(("using ", "the game: ", "gog: ", "steam: ", "epic: "))
                         or "supported PC Torchlights" in l]                # the whole list, in its order
            self.assertIn("x360_ui.ini", os.listdir(steam_game), who)
            r = subprocess.run(([sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]) +
                               ["uninstall", "--game", steam_game, "--yes", "--console"], capture_output=True, text=True, env=self.env)
            self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
        self.assertEqual(took["cpp"], took["py"])
        for game in (steam_game, epic_game):
            twins = {}
            for who in ("py", "cpp"):
                copy = os.path.join(self.root, who + "-" + os.path.basename(os.path.dirname(game)))
                shutil.copytree(game, copy)
                twins[who] = copy
            for who, copy in twins.items():
                cmd = ([sys.executable, os.path.join(TOOLS, "install.py")] if who == "py" else [BINARY]) + [
                    "install", "--game", copy, "--x360-dir", self.x360, "--yes", "--console", "--dll", self.proxy, "--bag", "pc"]
                r = subprocess.run(cmd, capture_output=True, text=True, env=self.env)
                self.assertEqual(r.returncode, 0, who + r.stdout + r.stderr)
            a, b = _tree(twins["py"]), _tree(twins["cpp"])
            norm = lambda t, p: t["x360_ui.ini"].replace(p.replace("/", "\\").encode(), b"GAME")
            self.assertEqual(norm(b, twins["cpp"]), norm(a, twins["py"]))
            del a["x360_ui.ini"], b["x360_ui.ini"]
            self.assertEqual(b, a)

    @unittest.skipUnless(os.name == "nt", "Windows: a folder whose write is denied by its ACL")
    def test_a_folder_windows_denies(self):
        """Write denied to everyone on the game's folder (icacls): the installer asks for an
        administrator's permission only then (X360UI_NO_ELEVATE stands for a declined UAC prompt;
        an elevated runner is refused by the ACL as well) and changes nothing."""
        game = self.games["cpp"][0]
        before = _tree(game)
        subprocess.run(["icacls", game, "/deny", "*S-1-1-0:(W,D,DC,AD,WD)"], check=True, capture_output=True)
        try:
            r = self.install("cpp", "--bag", "pc", env=dict(self.env, X360UI_NO_ELEVATE="1"))
        finally:
            subprocess.run(["icacls", game, "/remove:d", "*S-1-1-0"], check=True, capture_output=True)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("nothing was changed", r.stderr)
        self.assertEqual(_tree(game), before)
        self.assertFalse(any(n.startswith("x360ui") for n in os.listdir(game)))

    @unittest.skipIf(os.name == "nt" or (hasattr(os, "geteuid") and os.geteuid() == 0), "a folder without write permission (POSIX, not root)")
    def test_a_folder_it_may_not_write(self):
        game = self.games["cpp"][0]
        before = _tree(game)
        os.chmod(game, 0o555)
        try:
            r = self.install("cpp", "--bag", "pc")
        finally:
            os.chmod(game, 0o755)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("nothing was changed", r.stderr)
        self.assertEqual(_tree(game), before)


if __name__ == "__main__":
    unittest.main()

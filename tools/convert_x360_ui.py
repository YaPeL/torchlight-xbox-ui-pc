#!/usr/bin/env python3
"""The Xbox 360 interface's files, converted from the user's 360 `pak.zip` for the PC game.

convert_x360_ui.py PAK OUT     writes OUT/media/ui/x360/ and OUT/x360_ui_report.json

What it takes from PAK: the 360 scheme (`media/ui/hud/schemes/ceguiWidgets.scheme`), its looknfeel,
the imagesets and fonts the scheme names with their textures and TTF files, and every `.uilayout`,
written as `.layout`. Everything lands flat in `media/ui/x360/`, so no path of PC's own UI is
replaced, and every imageset of the scheme is renamed `X360_<name>` (in the imagesets, the scheme,
the looknfeel and the layouts), so none meets a PC imageset of the same name (both games have a
`Loading`, on different textures). Every font is renamed `X360_<name>` too (in the font files, the
scheme, the looknfeel's and the layouts' `Font` properties): PC has fonts of the same names from
the same TTF files, and the proxy adjusts the 360's fonts' letter spacing to the 360's (section 11),
which must not touch PC's own. The scheme is renamed `X360_<name>`.

A layout's reference to an imageset the scheme does not name is left as it is and listed in the
report (some 360 layouts name `widgets3`-`widgets6` and `characters`, which the pak lacks).
OUT must be outside this repository: the output is game data.
"""
import json
import os
import re
import sys
import zipfile

SCHEME = "media/ui/hud/schemes/ceguiwidgets.scheme"
DEST = "media/ui/x360/"
PREFIX = "X360_"

TAG = r"<{}\b[^>]*>"
ATTR = r'(\b{}\s*=\s*")([^"]*)(")'


def decode(data):
    """(text, codec) of an XML file: UTF-16 with a BOM, or UTF-8 (BOM kept)."""
    if data[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return data.decode("utf-16"), "utf-16"
    if data[:3] == b"\xef\xbb\xbf":
        return data[3:].decode("utf-8"), "utf-8-sig"
    return data.decode("utf-8"), "utf-8"


def attr(tag, name):
    m = re.search(ATTR.format(name), tag, re.I)
    return m.group(2) if m else None


def set_attr(tag, name, value):
    return re.sub(ATTR.format(name), lambda m: m.group(1) + value + m.group(3), tag, count=1, flags=re.I)


def retag(text, element, fn):
    """text with every <element ...> tag replaced by fn(tag)."""
    return re.sub(TAG.format(element), lambda m: fn(m.group(0)), text)


def basename(path):
    return path.replace("\\", "/").rsplit("/", 1)[-1]


def rename_sets(text, names):
    """text with the imagesets of `names` ({old: new}) renamed where a layout or looknfeel names
    them (`set:N image:...`, `imageset="N"`); returns (text, sorted unknown set names)."""
    unknown = set()

    def set_ref(m):
        if m.group(2) in names:
            return m.group(1) + names[m.group(2)]
        unknown.add(m.group(2))
        return m.group(0)

    def attr_ref(m):
        if m.group(2) in names:
            return m.group(1) + names[m.group(2)] + m.group(3)
        unknown.add(m.group(2))
        return m.group(0)

    text = re.sub(r'(\bset:)([^\s"]+)', set_ref, text)
    text = re.sub(ATTR.format("imageset"), attr_ref, text, flags=re.I)
    return text, sorted(unknown)


def rename_fonts(text, fonts):
    """text with the fonts of `fonts` ({old: new}) renamed where a `Font` property names them
    (layouts: `<Property Name="Font" Value="..."`; looknfeel: `name="Font" value="..."`)."""
    def prop(m):
        return m.group(1) + fonts.get(m.group(2), m.group(2)) + m.group(3)
    return re.sub(r'(\b[Nn]ame\s*=\s*"Font"\s+[Vv]alue\s*=\s*")([^"]*)(")', prop, text)


def convert(files):
    """files: {lowercase pak path: bytes}. Returns ({output path: bytes}, report)."""
    out = {}
    report = {"imagesets": {}, "fonts": [], "font_names": {}, "layouts": {}, "missing_files": []}
    fonts = report["font_names"]

    def put(name, data):
        path = DEST + name
        if path.lower() in {p.lower() for p in out}:
            raise ValueError(f"two files would be written as {path}")
        out[path] = data
        return path

    def take(path):
        data = files.get(path.replace("\\", "/").lower())
        if data is None:
            report["missing_files"].append(path)
        return data

    if SCHEME not in files:
        raise ValueError(f"{SCHEME} is not in the pak: is this the 360 game's pak.zip?")
    scheme, codec = decode(files[SCHEME])

    names = {}
    for tag in re.findall(TAG.format("Imageset"), scheme):
        names[attr(tag, "Name")] = PREFIX + attr(tag, "Name")
    report["imagesets"] = dict(names)

    # The imagesets and their textures.
    written = {}

    def imageset(tag):
        src = attr(tag, "Filename")
        if src.lower() not in written:
            data = take(src)
            if data is None:
                written[src.lower()] = src
                return tag
            text, enc = decode(data)

            def fix(t):
                t = set_attr(t, "Name", names.get(attr(t, "Name"), attr(t, "Name")))
                image = attr(t, "Imagefile")
                texture = take(image)
                if texture is not None:
                    t = set_attr(t, "Imagefile", put(basename(image), texture))
                return t

            written[src.lower()] = put(basename(src), retag(text, "Imageset", fix).encode(enc))
        tag = set_attr(tag, "Name", names[attr(tag, "Name")])
        return set_attr(tag, "Filename", written[src.lower()])

    # The fonts and their TTF files (a font file the scheme names twice is written once).
    def font(tag):
        src = attr(tag, "Filename")
        if src.lower() not in written:
            data = take(src)
            if data is None:
                written[src.lower()] = src
                return tag
            text, enc = decode(data)

            def fix(t):
                fonts[attr(t, "Name")] = PREFIX + attr(t, "Name")
                t = set_attr(t, "Name", PREFIX + attr(t, "Name"))
                ttf = attr(t, "Filename")
                if ttf.lower() not in written:
                    blob = take(ttf)
                    written[ttf.lower()] = put(basename(ttf), blob) if blob is not None else ttf
                return set_attr(t, "Filename", written[ttf.lower()])

            written[src.lower()] = put(basename(src), retag(text, "Font", fix).encode(enc))
            report["fonts"].append(basename(src))
        tag = set_attr(tag, "Filename", written[src.lower()])
        return set_attr(tag, "Name", PREFIX + attr(tag, "Name")) if attr(tag, "Name") else tag

    def looknfeel(tag):
        src = attr(tag, "Filename")
        data = take(src)
        if data is None:
            return tag
        text, enc = decode(data)
        text, unknown = rename_sets(text, names)
        text = rename_fonts(text, fonts)
        report["looknfeel_unknown_imagesets"] = unknown
        return set_attr(tag, "Filename", put(basename(src), text.encode(enc)))

    scheme = retag(scheme, "GUIScheme", lambda t: set_attr(t, "Name", PREFIX + attr(t, "Name")))
    scheme = retag(scheme, "Imageset", imageset)
    scheme = retag(scheme, "Font", font)
    scheme = retag(scheme, "LookNFeel", looknfeel)
    report["scheme"] = put(basename(SCHEME.replace("ceguiwidgets", "ceguiWidgets")), scheme.encode(codec))

    # The layouts.
    for path in sorted(p for p in files if p.endswith(".uilayout")):
        text, enc = decode(files[path])
        text, unknown = rename_sets(text, names)
        text = rename_fonts(text, fonts)
        name = basename(path)[: -len(".uilayout")] + ".layout"
        put(name, text.encode(enc))
        report["layouts"][name] = {"windows": len(re.findall(r"<Window\b", text)), "unknown_imagesets": unknown}
    return out, report


def read_pak(path):
    """{lowercase path: bytes} of the pak's files that the converter may need."""
    wanted = (".uilayout", ".scheme", ".looknfeel", ".imageset", ".font", ".dds", ".ttf", ".tga", ".png")
    with zipfile.ZipFile(path) as z:
        return {i.filename.lower(): z.read(i) for i in z.infolist()
                if i.filename.lower().startswith("media/ui/") and i.filename.lower().endswith(wanted)}


def inside(path, root):
    path, root = os.path.realpath(path), os.path.realpath(root)
    return path == root or path.startswith(root + os.sep)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    pak, dest = sys.argv[1:]
    repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    if inside(dest, repo):
        print(f"{dest} is inside this repository; write the game's files elsewhere.")
        return 2
    out, report = convert(read_pak(pak))
    for rel, data in out.items():
        p = os.path.join(dest, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "wb") as f:
            f.write(data)
    with open(os.path.join(dest, "x360_ui_report.json"), "w") as f:
        json.dump(report, f, indent=1)
    unknown = sorted({s for v in report["layouts"].values() for s in v["unknown_imagesets"]})
    print(f"{len(out)} files into {os.path.join(dest, DEST)}: {len(report['layouts'])} layouts, "
          f"{len(report['imagesets'])} imagesets, {len(report['fonts'])} fonts")
    if unknown:
        print(f"imagesets named by layouts but not by the scheme (left as they are): {', '.join(unknown)}")
    if report["missing_files"]:
        print(f"named but not in the pak: {', '.join(report['missing_files'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

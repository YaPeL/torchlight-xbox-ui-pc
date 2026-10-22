#!/usr/bin/env python3
"""The Xbox 360 interface's files, converted from the user's 360 `pak.zip` for the PC game.

convert_x360_ui.py PAK OUT [--prompts STYLE] [--swap-confirm-cancel] [--image IMAGE] [--pc-pak PCPAK]
                               writes OUT/media/ui/x360/ and OUT/x360_ui_report.json

--image IMAGE: the user's decrypted 360 executable (its image at 0x82000000, as their recomp
tools write it): the texts the 360's code puts in its screens (not in its layouts: the pages'
names, the bars' buttons, the stances...) are read from it at the addresses in STRINGS and written
to OUT/x360_strings.txt (key=value, UTF-8), which the proxy loads; the repository holds only the
keys and the addresses.

--pc-pak PCPAK: the user's PC `Pak.zip`. The context tips (`media/GLOBALS.DAT.adm`, its
CONTEXTTIPS: TIP_WELCOME, TIP_STASH, ...) of both games are written to OUT/x360_strings.txt as
`tip.ID` (the 360's text, written for the pad) and `pctip.ID` (PC's), so that the proxy shows the
360's text of the tip PC opens.

--prompts playstation3|playstation4|playstation5|playstation|nintendo draws the pad prompts (a_button,
left_bumper, start_button, the sticks, the d-pad, the help screen's controller...) with
that pad's icons (tools/pad_prompts.py, data/prompts/, Kenney's, CC0) into the converted widget
textures; xbox (the default) keeps the 360's. --swap-confirm-cancel: Nintendo's A and B swapped.

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

import pad_prompts

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
        text = label_button_relative(text)
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


def apply_prompts(out, report, icons):
    """The converted textures with the prompt icons drawn in ({image name: pad_prompts.Image});
    every converted imageset naming one of them has its texture rewritten (A8R8G8B8 DDS)."""
    report["prompts"] = {}
    if not icons:
        return out
    for path in sorted(p for p in out if p.endswith(".imageset")):
        xml, _ = decode(out[path])
        regions = pad_prompts.image_regions(xml)
        if not any(name in regions for name in icons):
            continue
        texture = re.search(r'Imagefile\s*=\s*"([^"]*)"', xml)
        key = next((k for k in out if texture and k.lower() == texture.group(1).lower()), None)
        if key is None:
            report["prompts"][path] = "its texture is not among the converted files"
            continue
        try:
            out[key], count = pad_prompts.patch_texture(xml, out[key], icons)
            report["prompts"][path] = count
        except ValueError as e:
            report["prompts"][path] = str(e)
    return out


def read_pak(path):
    """{lowercase path: bytes} of the pak's files that the converter may need."""
    wanted = (".uilayout", ".scheme", ".looknfeel", ".imageset", ".font", ".dds", ".ttf", ".tga", ".png")
    with zipfile.ZipFile(path) as z:
        return {i.filename.lower(): z.read(i) for i in z.infolist()
                if i.filename.lower().startswith("media/ui/") and i.filename.lower().endswith(wanted)}


def inside(path, root):
    path, root = os.path.realpath(path), os.path.realpath(root)
    return path == root or path.startswith(root + os.sep)


# The 360 code's texts: key -> address of a UTF-16BE string in the 360 image (default.xex
# decrypted, base 0x82000000). Each address was found in the user's image; none of the texts is
# kept here.
IMAGE_BASE = 0x82000000
STRINGS = {
    "page.inventory": 0x820D17D0, "page.stats": 0x820CFB74, "page.skills": 0x820BF818, "page.quest": 0x820CF9A8,
    "bag.mine": 0x820D0E6C, "bag.pet": 0x820D0E7C, "title.merchant": 0x820D0E8C, "title.stash": 0x820D0EA0,
    "title.gambler": 0x820D0EE0, "title.sharedstash": 0x820D0EC4, "title.transmuter": 0x820D0EAC,
    "filter.transmute": 0x820D0C40, "title.enchanter": 0x820D058C, "title.enchanting": 0x820D17E4,
    "title.destroygems": 0x820D1044, "title.recovergems": 0x820D1028, "act.enchant": 0x820CF118, "ench.chance": 0x820CF164, "ench.max": 0x820CF128, "ench.cannot": 0x820CF18C,
    "act.unequip": 0x820D0FB0, "act.equip": 0x820D0FA4, "act.use": 0x820D0F9C, "act.learn": 0x820D1080,
    "act.drop": 0x820D0F90, "act.swap": 0x820D0F84, "act.identify": 0x820D1014, "act.buy": 0x820D0F48,
    "act.sell": 0x820D0F50, "act.socket": 0x820D1060, "act.insert": 0x820D1070, "filter.insert": 0x820D0C54,
    "compare.enable": 0x820D0FEC, "compare.disable": 0x820D0FC0,
    "filter.0": 0x820D0BE0, "filter.1": 0x820C0CB4, "filter.2": 0x820D0BF0, "filter.3": 0x820D0C00,
    "filter.4": 0x820D0C1C, "filter.5": 0x820D0C34,
    "effects": 0x820D0EF0, "level": 0x820C0DD4, "requires_level": 0x820C0DB4, "identify_hint": 0x820C17FC,
    "reward.xp": 0x820BEB40, "reward.gp": 0x820CF528, "reward.fame": 0x820C0494,
    "stance.0": 0x820D198C, "stance.1": 0x820D1968, "stance.2": 0x820D19AC,
    "time.hrs": 0x820CF7C8, "time.mins": 0x820CF7D0, "time.secs": 0x820CF7DC,
    "journal.0": 0x820CF5AC, "journal.1": 0x820CF5C4, "journal.2": 0x820CF5E0, "journal.3": 0x820CF600,
    "journal.4": 0x820CF618, "journal.5": 0x820CF63C, "journal.6": 0x820CF64C, "journal.7": 0x820CF670,
    "journal.8": 0x820CF698, "journal.9": 0x820CF6C0, "journal.10": 0x820CF6DC, "journal.11": 0x820CF6F8,
    "journal.12": 0x820CF718, "journal.13": 0x820CF734, "journal.14": 0x820CF750, "journal.15": 0x820CF768,
    "journal.16": 0x820CF784, "journal.17": 0x820CF7A8,
}


LABEL_LOOK = "ceguiWidget/ItemTextButton"
LABEL_HEIGHT_720 = 17.27        # the 360's object label: 26 pixels at 1080 (the catalog's in-game/07)


def label_button_relative(text):
    """The object label's A button (ItemTextButton's "Button" section: 21x21 at -20, -1, pixels at
    the 360's 720 rows) as fractions of the label's height, so that it keeps the 360's size and
    place at any resolution: PC's looks are not scaled with the screen."""
    start = text.find(f'<WidgetLook name="{LABEL_LOOK}">')
    if start < 0:
        return text
    end = text.find("</WidgetLook>", start)
    section = text.find('<ImagerySection name="Button">', start, end)
    if section < 0:
        return text
    close = text.find("</ImagerySection>", section)
    body = text[section:close]

    def rel(v):
        return f'<UnifiedDim scale="{v / LABEL_HEIGHT_720:.4f}" type="Height" />'

    for kind, value in (("LeftEdge", -20), ("TopEdge", -1), ("Width", 21), ("Height", 21)):
        body = body.replace(f'<Dim type="{kind}" ><AbsoluteDim value="{value}" /></Dim>',
                            f'<Dim type="{kind}" >{rel(value)}</Dim>')
    return text[:section] + body + text[close:]


def image_strings(image):
    """{key: text} of STRINGS read from the 360 image's bytes; a key whose bytes are not a short
    printable UTF-16BE string is left out (and listed by the caller)."""
    out = {}
    for key, addr in STRINGS.items():
        off = addr - IMAGE_BASE
        if not 0 <= off < len(image) - 1:
            continue
        end = off
        while end + 1 < len(image) and end - off < 400 and image[end:end + 2] != b"\0\0":
            end += 2
        try:
            text = image[off:end].decode("utf-16-be")
        except UnicodeDecodeError:
            continue
        if text and len(text) < 100 and text.isprintable() and "=" not in text:
            out[key] = text
    return out


def adm_tree(raw):
    """A Runic .adm file: (its string table {id: text}, its root (name, [(key, type, value)], children)).
    Keys and names are given as their strings; a string value (type 8) stays its id."""
    import struct
    count = struct.unpack_from("<I", raw, 4)[0]
    o, table = 8, {}
    for _ in range(count):
        i, n = struct.unpack_from("<II", raw, o)
        o += 8
        table[i] = raw[o:o + 2 * n].decode("utf-16-le")
        o += 2 * n

    def node(o):
        name, n = struct.unpack_from("<II", raw, o)
        o += 8
        props = []
        for _ in range(n):
            key, kind = struct.unpack_from("<II", raw, o)
            o += 8
            fmt = "<Q" if kind == 6 else "<I"
            props.append((table.get(key, key), kind, struct.unpack_from(fmt, raw, o)[0]))
            o += struct.calcsize(fmt)
        n = struct.unpack_from("<I", raw, o)[0]
        o += 4
        children = []
        for _ in range(n):
            o, child = node(o)
            children.append(child)
        return o, (table.get(name, name), props, children)
    return table, node(o)[1]


def context_tips(raw):
    """{TIP_ID: text} of a GLOBALS.DAT.adm's CONTEXTTIPS node."""
    table, root = adm_tree(raw)
    out = {}

    def walk(n):
        name, props, children = n
        if name == "CONTEXTTIPS":
            for key, kind, value in props:
                if kind == 8 and value in table:
                    out[str(key)] = table[value].replace("\r", " ").replace("\n", " ")
        for c in children:
            walk(c)
    walk(root)
    return out


def globals_of(pak):
    with zipfile.ZipFile(pak) as z:
        for i in z.infolist():
            if i.filename.lower() == "media/globals.dat.adm":
                return z.read(i)
    raise ValueError(f"{pak} has no media/GLOBALS.DAT.adm")


def main():
    args = sys.argv[1:]
    image = None
    if "--image" in args:
        i = args.index("--image")
        image = args[i + 1] if i + 1 < len(args) else ""
        del args[i:i + 2]
    pc_pak = None
    if "--pc-pak" in args:
        i = args.index("--pc-pak")
        pc_pak = args[i + 1] if i + 1 < len(args) else ""
        del args[i:i + 2]
    style, swap = "xbox", False
    if "--swap-confirm-cancel" in args:
        swap = True
        args.remove("--swap-confirm-cancel")
    if "--prompts" in args:
        i = args.index("--prompts")
        style = args[i + 1] if i + 1 < len(args) else ""
        del args[i:i + 2]
    if len(args) != 2 or style not in pad_prompts.STYLES:
        print(__doc__)
        return 2
    pak, dest = args
    repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    if inside(dest, repo):
        print(f"{dest} is inside this repository; write the game's files elsewhere.")
        return 2
    out, report = convert(read_pak(pak))
    icons_dir = os.path.join(repo, "data", "prompts")
    out = apply_prompts(out, report, pad_prompts.load_icons(icons_dir, style, swap))
    for rel, data in out.items():
        p = os.path.join(dest, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "wb") as f:
            f.write(data)
    with open(os.path.join(dest, "x360_ui_report.json"), "w") as f:
        json.dump(report, f, indent=1)
    texts = {}
    if image:
        with open(image, "rb") as f:
            texts = image_strings(f.read())
        missing = sorted(set(STRINGS) - set(texts))
        print(f"{len(texts)} texts of the 360's code into x360_strings.txt" + (f"; not found: {', '.join(missing)}" if missing else ""))
    if pc_pak:
        tips, pc_tips = context_tips(globals_of(pak)), context_tips(globals_of(pc_pak))
        texts.update({f"tip.{k}": v for k, v in tips.items()})
        texts.update({f"pctip.{k}": v for k, v in pc_tips.items()})
        print(f"{len(tips)} tips of the 360's, {len(pc_tips)} of PC's, {len(set(tips) & set(pc_tips))} in both")
    if texts:
        with open(os.path.join(dest, "x360_strings.txt"), "w", encoding="utf-8") as f:
            f.write("".join(f"{k}={v}\n" for k, v in sorted(texts.items())))
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

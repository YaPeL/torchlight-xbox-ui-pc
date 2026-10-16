#!/usr/bin/env python3
"""The 360 interface's pad prompts drawn with PlayStation or Nintendo icons.

The 360's button, stick and d-pad images live inside its widget imagesets (a_button, left_bumper,
start_button, dpad_up, analog_button_left_lrud...). Their textures are rewritten with each
replaced image's region cleared and the icon scaled into it (aspect kept, centred, inside a
1-texel transparent border), so every window that names an image keeps its size and place; the
imagesets and layouts are not touched. The help screen's controller picture becomes the style's
controller, at 0.8 of its region. PlayStation's style follows a model (PS3, PS4, PS5): its
controller, and start and back as that model draws them.

A Python port of the recomp's src/mods/pad_prompts.cpp (PromptIcons, FitIcon, PatchPromptTexture,
the mods session's work, its develop at b0cd91a), so the two games show the same icons; no code is
shared between the repositories. The icons are Kenney's "Input Prompts" (CC0), in
data/prompts/<folder>/ as PNG, the recomp's crop (data/prompts/LICENSE.txt).

Used by convert_x360_ui.py (--prompts STYLE). Pure Python: no Pillow.
"""
import re
import struct
import zlib

STYLES = ("xbox", "playstation3", "playstation4", "playstation5", "playstation", "nintendo")
CONTROLLER_SCALE = 0.8        # the help screen's controller, of its region
PROMPT_MARGIN = 1             # transparent texels kept around an icon inside its region


def _style(style):
    """The style itself: "playstation" alone is the PS5's (the recomp's choice with no pad)."""
    return "playstation5" if style == "playstation" else style


def icon_folder(style):
    """The icons' folder below data/prompts/: "playstation" for the three models, "nintendo"."""
    return "playstation" if _style(style).startswith("playstation") else "nintendo"


def swaps_confirm_cancel(style, swap_setting):
    """Whether confirm and cancel are swapped: only with Nintendo's style."""
    return bool(swap_setting) and style == "nintendo"


def prompt_icons(style, swap_confirm_cancel=False):
    """[(the game's image name, our icon file without .png, scale)] a style replaces; none for
    Xbox."""
    style = _style(style)
    if style.startswith("playstation"):
        model = "ps" + style[-1]
        icons = [("a_button", "cross"), ("b_button", "circle"), ("button_x", "square"),
                 ("y_button", "triangle"), ("left_bumper", "l1"), ("right_bumper", "r1"),
                 ("left_trigger", "l2"), ("right_trigger", "r2"),
                 # Start and back as the pad's model draws them, and its controller.
                 ("start_button", "start_" + model), ("back_button", "back_" + model)]
        icons = [(i, f, 1.0) for i, f in icons] + [("controller", "controller_" + model, CONTROLLER_SCALE)]
    elif style == "nintendo":
        # By position the 360's A is the bottom button, B on a Nintendo pad; swapped, the game's A
        # is the right one, A.
        swap = swaps_confirm_cancel(style, swap_confirm_cancel)
        icons = [("a_button", "a" if swap else "b"), ("b_button", "b" if swap else "a"), ("button_x", "y"),
                 ("y_button", "x"), ("left_bumper", "l"), ("right_bumper", "r"),
                 ("left_trigger", "zl"), ("right_trigger", "zr"), ("start_button", "plus"),
                 ("back_button", "minus")]
        icons = [(i, f, 1.0) for i, f in icons] + [("controller", "controller", CONTROLLER_SCALE)]
    else:
        return []
    # The sticks and the d-pad, the same names in both folders. The 360's sticks pushed in any
    # direction ("lrud") become the plain stick; analog_button_left_down's drawing is the right
    # stick pressed.
    sticks = [("analog_button_left_lrud", "stick_l"), ("analog_button_right_lrud", "stick_r"),
              ("analog_button_left_lr", "stick_l_horizontal"), ("analog_button_right_lr", "stick_r_horizontal"),
              ("analog_button_left_ud", "stick_l_vertical"), ("analog_button_right_ud", "stick_r_vertical"),
              ("analog_button_left_down", "stick_r_press"), ("dpad_updown", "dpad_vertical"),
              ("dpad_leftright", "dpad_horizontal"), ("dpad_up", "dpad_up"), ("dpad_down", "dpad_down"),
              ("dpad_left", "dpad_left"), ("dpad_right", "dpad_right")]
    return icons + [(i, f, 1.0) for i, f in sticks]


class Image:
    """width, height and RGBA bytes, row by row."""

    def __init__(self, width, height, rgba=None):
        self.width, self.height = width, height
        self.rgba = bytearray(rgba) if rgba is not None else bytearray(width * height * 4)


# ---- PNG (8-bit RGBA or RGB, not interlaced: what the icons are) ----

def read_png(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat, width = 8, b"", None
    while pos < len(data):
        length, kind = struct.unpack_from(">I4s", data, pos)
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or colour not in (2, 6) or interlace:
                raise ValueError("only 8-bit RGB or RGBA PNGs, not interlaced")
            channels = 4 if colour == 6 else 3
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    if width is None:
        raise ValueError("no IHDR")
    raw = zlib.decompress(idat)
    stride = width * channels
    out = bytearray(width * height * 4)
    prev = bytearray(stride)
    for y in range(height):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        for x in range(width):
            o = (y * width + x) * 4
            out[o:o + 3] = line[x * channels:x * channels + 3]
            out[o + 3] = line[x * channels + 3] if channels == 4 else 255
        prev = line
    return Image(width, height, out)


# ---- DDS ----

def _565(c):
    return ((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31


def _colour_block(block, opaque_only):
    c0, c1, bits = struct.unpack_from("<HHI", block, 0)
    p0, p1 = _565(c0), _565(c1)
    if c0 > c1 or opaque_only:
        p2 = tuple((2 * a + b) // 3 for a, b in zip(p0, p1))
        p3 = tuple((a + 2 * b) // 3 for a, b in zip(p0, p1))
        alpha3 = 255
    else:
        p2 = tuple((a + b) // 2 for a, b in zip(p0, p1))
        p3 = (0, 0, 0)
        alpha3 = 0
    pal = (p0, p1, p2, p3)
    return [(pal[(bits >> (2 * i)) & 3], alpha3 if ((bits >> (2 * i)) & 3) == 3 else 255) for i in range(16)]


def decode_dds(data):
    """The top level of a DDS: DXT1, DXT3, DXT5 or uncompressed 32-bit (A8R8G8B8 / X8R8G8B8)."""
    if data[:4] != b"DDS ":
        raise ValueError("not a DDS")
    height, width = struct.unpack_from("<II", data, 12)
    pf_flags, fourcc, bits = struct.unpack_from("<I4sI", data, 80)
    img = Image(width, height)
    pos = 128
    if pf_flags & 0x4:
        if fourcc not in (b"DXT1", b"DXT3", b"DXT5"):
            raise ValueError(f"unsupported DDS format {fourcc!r}")
        size = 8 if fourcc == b"DXT1" else 16
        for by in range((height + 3) // 4):
            for bx in range((width + 3) // 4):
                block = data[pos:pos + size]
                pos += size
                alphas = [255] * 16
                if fourcc == b"DXT3":
                    a = int.from_bytes(block[:8], "little")
                    alphas = [((a >> (4 * i)) & 15) * 17 for i in range(16)]
                elif fourcc == b"DXT5":
                    a0, a1 = block[0], block[1]
                    abits = int.from_bytes(block[2:8], "little")
                    if a0 > a1:
                        apal = [a0, a1] + [((6 - i) * a0 + (1 + i) * a1) // 7 for i in range(6)]
                    else:
                        apal = [a0, a1] + [((4 - i) * a0 + (1 + i) * a1) // 5 for i in range(4)] + [0, 255]
                    alphas = [apal[(abits >> (3 * i)) & 7] for i in range(16)]
                texels = _colour_block(block[size - 8:], fourcc != b"DXT1")
                for i in range(16):
                    x, y = bx * 4 + i % 4, by * 4 + i // 4
                    if x < width and y < height:
                        (r, g, b), a1 = texels[i]
                        o = (y * width + x) * 4
                        img.rgba[o:o + 4] = bytes((r, g, b, min(a1, alphas[i])))
        return img
    if bits == 32:
        rmask, gmask, bmask, amask = struct.unpack_from("<4I", data, 92)
        shifts = [(m & -m).bit_length() - 1 if m else 0 for m in (rmask, gmask, bmask, amask)]
        for i in range(width * height):
            v = struct.unpack_from("<I", data, pos + 4 * i)[0]
            r, g, b = ((v & m) >> s for m, s in zip((rmask, gmask, bmask), shifts))
            a = (v & amask) >> shifts[3] if amask else 255
            img.rgba[4 * i:4 * i + 4] = bytes((r, g, b, a))
        return img
    raise ValueError("unsupported DDS pixel format")


def encode_dds_argb(img):
    """An uncompressed A8R8G8B8 DDS, no mipmaps (the recomp's EncodeDdsArgb)."""
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into("<7I", header, 4, 124, 0x100F, img.height, img.width, img.width * 4, 0, 0)
    struct.pack_into("<8I", header, 76, 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into("<I", header, 108, 0x1000)
    px = bytearray(len(img.rgba))
    px[0::4], px[1::4], px[2::4], px[3::4] = img.rgba[2::4], img.rgba[1::4], img.rgba[0::4], img.rgba[3::4]
    return bytes(header) + bytes(px)


# ---- fitting ----

def fit_icon(icon, width, height):
    """icon scaled into width x height (an area filter on premultiplied alpha: for shrinking),
    its aspect kept, centred, transparent around it."""
    out = Image(width, height)
    if not icon.width or not icon.height or not width or not height:
        return out
    scale = min(width / icon.width, height / icon.height)
    w = min(max(int(round(icon.width * scale)), 1), width)
    h = min(max(int(round(icon.height * scale)), 1), height)
    left, top = (width - w) // 2, (height - h) // 2
    sx, sy = icon.width / w, icon.height / h
    for y in range(h):
        y0, y1 = y * sy, (y + 1) * sy
        for x in range(w):
            x0, x1 = x * sx, (x + 1) * sx
            s = [0.0, 0.0, 0.0, 0.0]
            area = 0.0
            iy = int(y0)
            while iy < icon.height and iy < y1:
                wy = min(iy + 1, y1) - max(iy, y0)
                ix = int(x0)
                while ix < icon.width and ix < x1:
                    wgt = (min(ix + 1, x1) - max(ix, x0)) * wy
                    if wgt > 0:
                        o = (iy * icon.width + ix) * 4
                        a = icon.rgba[o + 3] / 255.0
                        s[0] += icon.rgba[o] * a * wgt
                        s[1] += icon.rgba[o + 1] * a * wgt
                        s[2] += icon.rgba[o + 2] * a * wgt
                        s[3] += icon.rgba[o + 3] * wgt
                        area += wgt
                    ix += 1
                iy += 1
            if area <= 0 or s[3] <= 0:
                continue
            alpha = s[3] / area
            q = ((top + y) * width + left + x) * 4
            for c in range(3):
                out.rgba[q + c] = min(max(int(round(s[c] / area * 255.0 / alpha)), 0), 255)
            out.rgba[q + 3] = min(max(int(round(alpha)), 0), 255)
    return out


def image_regions(imageset_xml):
    """{image name: (x, y, width, height)} of an imageset's XML."""
    regions = {}
    for tag in re.findall(r"<Image\b[^>]*>", imageset_xml):
        attrs = dict(re.findall(r'(\w+)\s*=\s*"([^"]*)"', tag))
        try:
            regions[attrs["Name"]] = tuple(int(attrs[k]) for k in ("XPos", "YPos", "Width", "Height"))
        except (KeyError, ValueError):
            continue
    return regions


def patch_texture(imageset_xml, texture, icons):
    """(the texture redrawn as an A8R8G8B8 DDS, how many images were replaced): every image of
    `icons` ({image name: Image, or (Image, scale)}) cleared and its icon fitted in, inside a
    PROMPT_MARGIN border (the game's windows draw these images a little smaller than their
    regions and clip the bottom; the 360's art leaves the same border), at scale of that room;
    the other pixels as they were. Raises ValueError if the texture does not decode or an image
    lies outside it."""
    img = decode_dds(texture)
    count = 0
    for name, (x, y, w, h) in image_regions(imageset_xml).items():
        if name not in icons:
            continue
        icon, scale = icons[name] if isinstance(icons[name], tuple) else (icons[name], 1.0)
        if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > img.width or y + h > img.height:
            raise ValueError(f"image {name} lies outside its texture")
        margin = PROMPT_MARGIN if w > 2 * PROMPT_MARGIN and h > 2 * PROMPT_MARGIN else 0
        fitted = fit_icon(icon, max(1, int(round((w - 2 * margin) * scale))),
                          max(1, int(round((h - 2 * margin) * scale))))
        left, top = x + (w - fitted.width) // 2, y + (h - fitted.height) // 2
        for row in range(h):
            o = ((y + row) * img.width + x) * 4
            img.rgba[o:o + w * 4] = bytes(w * 4)
        for row in range(fitted.height):
            o = ((top + row) * img.width + left) * 4
            img.rgba[o:o + fitted.width * 4] = fitted.rgba[row * fitted.width * 4:(row + 1) * fitted.width * 4]
        count += 1
    return encode_dds_argb(img), count


def load_icons(icons_dir, style, swap_confirm_cancel=False):
    """{the game's image name: (Image, scale)} for a style, from icons_dir/<folder>/<file>.png."""
    import os
    icons = {}
    for image, file, scale in prompt_icons(style, swap_confirm_cancel):
        with open(os.path.join(icons_dir, icon_folder(style), file + ".png"), "rb") as f:
            icons[image] = (read_png(f.read()), scale)
    return icons

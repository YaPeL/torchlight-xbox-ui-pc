#!/usr/bin/env python3
"""Save a PNG of an X11 window (the game under Xwayland, where a root grab fails), by title.

grab_window.py TITLE OUT.png     the largest mapped window whose title contains TITLE
"""
import ctypes
import ctypes.util
import sys

from PIL import Image

x = ctypes.cdll.LoadLibrary(ctypes.util.find_library("X11"))
x.XOpenDisplay.restype = ctypes.c_void_p
x.XOpenDisplay.argtypes = [ctypes.c_char_p]
x.XDefaultRootWindow.restype = ctypes.c_ulong
x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
x.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
                         ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
                         ctypes.POINTER(ctypes.c_uint)]
x.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_char_p)]


class Attrs(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int), ("height", ctypes.c_int),
                ("border_width", ctypes.c_int), ("depth", ctypes.c_int), ("visual", ctypes.c_void_p),
                ("root", ctypes.c_ulong), ("class_", ctypes.c_int), ("bit_gravity", ctypes.c_int),
                ("win_gravity", ctypes.c_int), ("backing_store", ctypes.c_int),
                ("backing_planes", ctypes.c_ulong), ("backing_pixel", ctypes.c_ulong),
                ("save_under", ctypes.c_int), ("colormap", ctypes.c_ulong),
                ("map_installed", ctypes.c_int), ("map_state", ctypes.c_int),
                ("all_event_masks", ctypes.c_long), ("your_event_mask", ctypes.c_long),
                ("do_not_propagate_mask", ctypes.c_long), ("override_redirect", ctypes.c_int),
                ("screen", ctypes.c_void_p)]


class XImage(ctypes.Structure):
    _fields_ = [("width", ctypes.c_int), ("height", ctypes.c_int), ("xoffset", ctypes.c_int),
                ("format", ctypes.c_int), ("data", ctypes.POINTER(ctypes.c_ubyte)),
                ("byte_order", ctypes.c_int), ("bitmap_unit", ctypes.c_int), ("bitmap_bit_order", ctypes.c_int),
                ("bitmap_pad", ctypes.c_int), ("depth", ctypes.c_int), ("bytes_per_line", ctypes.c_int),
                ("bits_per_pixel", ctypes.c_int)]


x.XGetWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(Attrs)]
x.XGetImage.restype = ctypes.POINTER(XImage)
x.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int, ctypes.c_uint,
                        ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]


def windows(d, w):
    root, parent = ctypes.c_ulong(), ctypes.c_ulong()
    kids, n = ctypes.POINTER(ctypes.c_ulong)(), ctypes.c_uint()
    if not x.XQueryTree(d, w, ctypes.byref(root), ctypes.byref(parent), ctypes.byref(kids), ctypes.byref(n)):
        return
    for i in range(n.value):
        yield kids[i]
        yield from windows(d, kids[i])


ERROR_HANDLER = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
_ignore_errors = ERROR_HANDLER(lambda d, e: 0)  # a failed XGetImage returns NULL instead of exiting
x.XSetErrorHandler.argtypes = [ERROR_HANDLER]
x.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]


def candidates(d, title):
    """[(window, width, height)] to grab, best first: untitled mapped windows of the place and size
    of the largest mapped window titled TITLE, then that window. Once Wine restores the game's
    window, the game draws (Vulkan, DXVK) into such an untitled window, and the titled one keeps a
    stale frame; at other times the untitled one cannot be read."""
    best, plain = None, []
    for w in windows(d, x.XDefaultRootWindow(d)):
        name = ctypes.c_char_p()
        x.XFetchName(d, w, ctypes.byref(name))
        a = Attrs()
        x.XGetWindowAttributes(d, w, ctypes.byref(a))
        if a.map_state != 2:
            continue
        if name.value and title in name.value:
            if not best or a.width * a.height > best[1] * best[2]:
                best = (w, a.width, a.height, a.x, a.y)
        elif not name.value:
            plain.append((w, a.width, a.height, a.x, a.y))
    if not best:
        return []
    return [p[:3] for p in plain if p[1:] == best[1:]] + [best[:3]]


def main():
    title, out = sys.argv[1].encode(), sys.argv[2]
    d = x.XOpenDisplay(b":0")
    x.XSetErrorHandler(_ignore_errors)
    found = candidates(d, title)
    if not found:
        print("no mapped window titled", title)
        return 1
    for w, width, height in found:
        img = x.XGetImage(d, w, 0, 0, width, height, 0xFFFFFFFF, 2)
        x.XSync(d, 0)
        if img:
            break
    else:
        print("XGetImage failed")
        return 1
    i = img.contents
    raw = ctypes.string_at(i.data, i.bytes_per_line * i.height)
    im = Image.frombuffer("RGBX", (i.width, i.height), raw, "raw", "BGRX", i.bytes_per_line, 1).convert("RGB")
    im.save(out)
    print(hex(w), im.size)
    return 0


if __name__ == "__main__":
    sys.exit(main())

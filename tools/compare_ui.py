#!/usr/bin/env python3
"""Two UI dumps ("torchlight-ui-dump 1") compared window by window: a screen of the 360 catalog
against the same screen in the PC game (proxy/ui_dump.c writes PC's).

compare_ui.py REF.json PC.json [--under NAME] [--tolerance PX] [--all]

Only the windows shown on screen (`shown`) are compared. Windows are matched by base_name (the
name without its layout's prefix) and their parent's base_name; a base_name that is unique on
both sides matches alone. For each pair: the 1080p rectangle (rect_1080p, within the tolerance,
1 px by default), the text, the font, the images and the effective alpha. Font and imageset names
are compared without the converter's `X360_` prefix (tools/convert_x360_ui.py).

--under NAME   only the windows under the one of that base_name (on both sides)
--all          also list the shown PC windows that have no 360 counterpart
A PC window shown where the 360's of the same base name and parent is hidden is reported too
("shown on PC, hidden on the 360"), as one the 360 hides by its rules (the gold at the gem pages).
Prints one line per difference and a count; the exit code is 1 if anything differs.
"""
import argparse
import json
import sys

PREFIX = "X360_"


def strip(name):
    return name[len(PREFIX):] if name and name.startswith(PREFIX) else name


def base(w):
    """The base name without a leading layout instance number the 360's dump kept ("103Base")."""
    name = w.get("base_name") or ""
    stripped = name.lstrip("0123456789")
    return stripped.lstrip("_") if stripped and stripped != name else name


def flatten(windows, under=None, hidden=False):
    """[(window, parent base_name)] of the shown windows (the hidden ones with hidden=True), in
    tree order."""
    out = []

    def walk(w, parent, inside):
        inside = inside or under is None or base(w) == under
        if inside and bool(w.get("shown")) != hidden:
            out.append((w, parent))
        for c in w.get("children", []):
            walk(c, base(w), inside)

    for w in windows:
        walk(w, None, False)
    return out


def index(flat):
    by_pair, by_name = {}, {}
    for w, parent in flat:
        by_pair.setdefault((base(w), parent), []).append(w)
        by_name.setdefault(base(w), []).append(w)
    return by_pair, by_name


def images(w):
    return {k: (strip(v.get("imageset")), v.get("image")) for k, v in (w.get("images") or {}).items()}


def differences(ref, pc, tolerance):
    out = []
    a, b = ref.get("rect_1080p") or {}, pc.get("rect_1080p") or {}
    off = [k for k in ("left", "top", "right", "bottom") if abs((a.get(k) or 0) - (b.get(k) or 0)) > tolerance]
    if off:
        out.append("rect " + ", ".join(f"{k} {a.get(k)} vs {b.get(k)}" for k in off))
    if (ref.get("text") or "") != (pc.get("text") or ""):
        out.append(f"text {ref.get('text')!r} vs {pc.get('text')!r}")
    if strip(ref.get("font") or "") != strip(pc.get("font") or ""):
        out.append(f"font {ref.get('font')!r} vs {pc.get('font')!r}")
    ia, ib = images(ref), images(pc)
    for k in sorted(set(ia) | set(ib)):
        if ia.get(k) != ib.get(k):
            out.append(f"{k} {ia.get(k)} vs {ib.get(k)}")
    ea, eb = ref.get("effective_alpha"), pc.get("effective_alpha")
    if ea is not None and eb is not None and abs(ea - eb) > 0.02:
        out.append(f"alpha {ea} vs {eb}")
    return out


def auto_named(name):
    """A name CEGUI made up (a layout's unnamed window: "i_135", "17"): matched by place."""
    return name.isdigit() or (name.startswith("i_") and name[2:].isdigit())


def visible_rank(w):
    """PC keeps its own windows of the same names drawn transparent: the visible one first."""
    return (0 if (w.get("effective_alpha") or 0) > 0.01 else 1, 0 if w.get("name", "").startswith("x360") else 1)


def compare(ref_dump, pc_dump, under=None, tolerance=1.0, show_extra=False):
    """[line] of the differences between two dumps (dicts). Named windows are matched by base name
    (and their parent's); the unnamed children of a matched pair are matched in order."""
    ref_flat = flatten(ref_dump["windows"], under)
    pc_flat = flatten(pc_dump["windows"], under)
    pc_pair, pc_name = index(pc_flat)
    _, ref_name = index(ref_flat)
    used, lines, pairs = set(), [], []
    for w, parent in ref_flat:
        name = base(w)
        if auto_named(name):
            continue
        cands = [c for c in pc_pair.get((name, parent), []) if id(c) not in used]
        if not cands and len(ref_name.get(name, [])) == 1:
            cands = [c for c in pc_name.get(name, []) if id(c) not in used]
        if not cands:
            lines.append(f"{parent}/{name}: missing on PC")
            continue
        match = sorted(cands, key=visible_rank)[0]
        used.add(id(match))
        pairs.append((w, match))
        lines += [f"{parent}/{name}: {d}" for d in differences(w, match, tolerance)]

    def unnamed(ref, pc, path):
        pk = [c for c in pc.get("children", []) if c.get("shown") and auto_named(base(c)) and id(c) not in used]
        for c in ref.get("children", []):
            if not (c.get("shown") and auto_named(base(c))):
                continue
            same = pk   # in order (the 360's dumps name every type DefaultWindow)
            if not same:
                lines.append(f"{path}/{c['base_name']}: missing on PC")
                continue
            m = same[0]
            pk.remove(m)
            used.add(id(m))
            lines.extend(f"{path}/{c['base_name']}: {d}" for d in differences(c, m, tolerance))
            unnamed(c, m, f"{path}/{c['base_name']}")

    for ref, pc in pairs:
        unnamed(ref, pc, ref["base_name"])
    ref_hidden = {(base(w), parent) for w, parent in flatten(ref_dump["windows"], under, hidden=True)}
    ref_shown = {(base(w), parent) for w, parent in ref_flat}
    for w, parent in pc_flat:
        key = (base(w), parent)
        if id(w) not in used and key in ref_hidden and key not in ref_shown and (w.get("effective_alpha") or 0) > 0.01:
            used.add(id(w))
            lines.append(f"{parent}/{base(w)}: shown on PC, hidden on the 360")
    if show_extra:
        lines += [f"{parent}/{base(w)}: only on PC" for w, parent in pc_flat if id(w) not in used]
    return lines


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("ref")
    ap.add_argument("pc")
    ap.add_argument("--under")
    ap.add_argument("--tolerance", type=float, default=1.0)
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args(argv)
    with open(args.ref, encoding="utf-8") as f:
        ref = json.load(f)
    with open(args.pc, encoding="utf-8") as f:
        pc = json.load(f)
    lines = compare(ref, pc, args.under, args.tolerance, args.all)
    for line in lines:
        print(line)
    print(f"{len(lines)} differences")
    return 1 if lines else 0


if __name__ == "__main__":
    sys.exit(main())

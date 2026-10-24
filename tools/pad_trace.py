#!/usr/bin/env python3
"""The pad's movement measured (docs/xbox-ui-on-pc.md section 27b): recorded stick routes, and the
hero's traces compared.

pad_trace.py routes OUT_DIR                 write the standard routes (pad replay v1 files)
pad_trace.py summary TRACE...               per trace: path length, net displacement, the
                                            farthest point from the start, the speed, as text
pad_trace.py compare REF TRACE [--svg OUT]  TRACE against REF (the recompiled 360's): the largest
                                            distance between the two paths at the same time,
                                            the speed per 250 ms span, PASS or FAIL; a plot

Formats (the same for this repository's proxy and for the recompiled 360 build):
  replay: "# x360ui pad replay v1", "# start_ms N", then "t_ms lx ly lt rt buttons" (XInput's raw
          values, each held until the next line).
  trace:  "# x360ui pad trace v1 SOURCE", the header line, then
          "t_us,frame,x_mu,z_mu,fx_mu,fz_mu,scale_mu,speed_mu,lx,ly" (thousandths of world units).
Traces are compared from the first sample whose stick is out of the 360's dead zone (0.1), so the
two builds' starts need not match; positions are relative to the hero's at that moment.
"""
import argparse
import math
import os
import sys

RAW = 32767
DEAD_ZONE = 0.1                 # the 360's (sub_82199E08)
SPAN_US = 250_000
# The tolerances (section 27b): the paths at most this far apart at any time, in world units ...
MAX_GAP = 0.5
# ... and each 250 ms span's speed within this ratio of the reference's, where the reference moves.
SPEED_TOLERANCE = 0.10
MOVING = 0.5                    # units per second: below it a span counts as at rest


def stick(x, y):
    return int(round(max(-1.0, min(1.0, x)) * RAW)), int(round(max(-1.0, min(1.0, y)) * RAW))


def route_lines(samples, start_ms=3000):
    out = ["# x360ui pad replay v1", f"# start_ms {start_ms}"]
    for t, x, y in samples:
        lx, ly = stick(x, y)
        out.append(f"{int(t)} {lx} {ly} 0 0 0")
    return "\n".join(out) + "\n"


def straight(length, tilt, angle_deg=90.0, ms=2000, step=10):
    a = math.radians(angle_deg)
    s = [(t, tilt * math.cos(a), tilt * math.sin(a)) for t in range(0, ms, step)]
    return s + [(ms, 0.0, 0.0)]


def spin(turns_per_s, tilt, ms=3000, step=10, angle_deg=90.0):
    s = []
    for t in range(0, ms, step):
        a = math.radians(angle_deg) + 2 * math.pi * turns_per_s * t / 1000.0
        s.append((t, tilt * math.cos(a), tilt * math.sin(a)))
    return s + [(ms, 0.0, 0.0)]


def reversal(tilt=1.0, ms=1000, step=10):
    s = [(t, 0.0, tilt) for t in range(0, ms, step)]
    s += [(ms + t, 0.0, -tilt) for t in range(0, ms, step)]
    return s + [(2 * ms, 0.0, 0.0)]


ROUTES = {
    "straight-full": lambda: straight(2000, 1.0),
    "straight-half": lambda: straight(2000, 0.5),
    "straight-edge": lambda: straight(2000, 0.12),
    "spin1-full": lambda: spin(1.0, 1.0),
    "spin2-full": lambda: spin(2.0, 1.0),
    "spin1-half": lambda: spin(1.0, 0.5),
    "spin2-half": lambda: spin(2.0, 0.5),
    "reversal": lambda: reversal(),
}


def write_routes(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for name, make in ROUTES.items():
        with open(os.path.join(out_dir, name + ".txt"), "w", newline="\n") as f:
            f.write(route_lines(make()))
    return sorted(ROUTES)


def read_trace(path):
    """[(t_s, x, z, fx, fz, scale, speed, sx, sy)], the stick as -1..1."""
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith("t_us"):
                continue
            v = [int(p) for p in line.split(",")]
            if len(v) != 10:
                raise ValueError(f"{path}: a line of {len(v)} fields: {line}")
            rows.append((v[0] / 1e6, v[2] / 1e3, v[3] / 1e3, v[4] / 1e3, v[5] / 1e3,
                         v[6] / 1e3, v[7] / 1e3, v[8] / RAW, v[9] / RAW))
    rows.sort(key=lambda r: r[0])
    return rows


def aligned(rows):
    """From the first sample with the stick out: (t, x, z) relative to it."""
    for i, r in enumerate(rows):
        if math.hypot(r[7], r[8]) > DEAD_ZONE:
            t0, x0, z0 = r[0], r[1], r[2]
            return [(q[0] - t0, q[1] - x0, q[2] - z0) for q in rows[i:]]
    return []


def at(path, t):
    """The position at time t, linear between samples (a sample holds until the next frame)."""
    if t <= path[0][0]:
        return path[0][1], path[0][2]
    for (ta, xa, za), (tb, xb, zb) in zip(path, path[1:]):
        if ta <= t <= tb:
            k = 0.0 if tb == ta else (t - ta) / (tb - ta)
            return xa + (xb - xa) * k, za + (zb - za) * k
    return path[-1][1], path[-1][2]


def summary(rows):
    p = aligned(rows)
    if not p:
        return {"samples": len(rows), "moved": False}
    length = sum(math.hypot(b[1] - a[1], b[2] - a[2]) for a, b in zip(p, p[1:]))
    farthest = max(math.hypot(q[1], q[2]) for q in p)
    duration = p[-1][0] - p[0][0]
    return {"samples": len(rows), "moved": True, "duration_s": duration, "length": length,
            "net": math.hypot(p[-1][1], p[-1][2]), "farthest": farthest,
            "mean_speed": length / duration if duration > 0 else 0.0}


def spans(path, end):
    """Speed (units per second) in each SPAN_US span from 0 to end."""
    out, t = [], 0.0
    while t + SPAN_US / 1e6 <= end + 1e-9:
        a, b = at(path, t), at(path, t + SPAN_US / 1e6)
        out.append(math.hypot(b[0] - a[0], b[1] - a[1]) / (SPAN_US / 1e6))
        t += SPAN_US / 1e6
    return out


def compare(ref_rows, rows):
    ref, mine = aligned(ref_rows), aligned(rows)
    if not ref or not mine:
        return {"pass": False, "reason": "a trace whose stick never leaves the dead zone"}
    end = min(ref[-1][0], mine[-1][0])
    gap, worst_t, t = 0.0, 0.0, 0.0
    while t <= end:
        a, b = at(ref, t), at(mine, t)
        d = math.hypot(a[0] - b[0], a[1] - b[1])
        if d > gap:
            gap, worst_t = d, t
        t += 0.01
    rs, ms = spans(ref, end), spans(mine, end)
    bad = [i for i, (r, m) in enumerate(zip(rs, ms))
           if r >= MOVING and abs(m - r) > SPEED_TOLERANCE * r]
    still = [i for i, (r, m) in enumerate(zip(rs, ms)) if r < MOVING and m >= MOVING]
    ok = gap <= MAX_GAP and not bad and not still
    return {"pass": ok, "max_gap": gap, "max_gap_at_s": worst_t, "spans": len(rs),
            "speed_spans_off": bad, "moving_where_ref_rests": still,
            "ref_speeds": rs, "speeds": ms, "duration_s": end}


def svg(paths, out, title=""):
    """The paths (name, [(t, x, z)]) on one plot, the same scale on both axes."""
    colors = ["#1b7f3b", "#c0392b", "#2c6fbb", "#8e44ad"]
    pts = [q for _n, p in paths for q in p] or [(0, 0, 0)]
    xs, zs = [q[1] for q in pts], [q[2] for q in pts]
    span = max(max(xs) - min(xs), max(zs) - min(zs), 1.0) * 1.1
    cx, cz = (max(xs) + min(xs)) / 2, (max(zs) + min(zs)) / 2
    size = 480

    def px(x, z):
        return (size / 2 + (x - cx) / span * size, size / 2 - (z - cz) / span * size)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size + 40}" '
             f'viewBox="0 0 {size} {size + 40}"><rect width="100%" height="100%" fill="#fff"/>',
             f'<text x="8" y="{size + 16}" font-family="sans-serif" font-size="12">{title} '
             f'(scale: {span:.2f} units across)</text>']
    for i, (name, p) in enumerate(paths):
        c = colors[i % len(colors)]
        d = " ".join(f"{px(q[1], q[2])[0]:.1f},{px(q[1], q[2])[1]:.1f}" for q in p)
        parts.append(f'<polyline fill="none" stroke="{c}" stroke-width="1.5" points="{d}"/>')
        parts.append(f'<text x="{8 + 160 * i}" y="{size + 34}" fill="{c}" font-family="sans-serif" '
                     f'font-size="12">{name}</text>')
    x0, y0 = px(0, 0)
    parts.append(f'<circle cx="{x0:.1f}" cy="{y0:.1f}" r="3" fill="#000"/></svg>')
    with open(out, "w") as f:
        f.write("\n".join(parts) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("routes")
    r.add_argument("out_dir")
    s = sub.add_parser("summary")
    s.add_argument("traces", nargs="+")
    s.add_argument("--svg")
    c = sub.add_parser("compare")
    c.add_argument("ref")
    c.add_argument("trace")
    c.add_argument("--svg")
    a = ap.parse_args(argv)
    if a.cmd == "routes":
        for name in write_routes(a.out_dir):
            print(os.path.join(a.out_dir, name + ".txt"))
        return 0
    if a.cmd == "summary":
        paths = []
        for path in a.traces:
            rows = read_trace(path)
            info = summary(rows)
            print(path + ": " + ", ".join(f"{k} {v:.3f}" if isinstance(v, float) else f"{k} {v}"
                                         for k, v in info.items()))
            paths.append((os.path.basename(path), aligned(rows)))
        if a.svg:
            svg(paths, a.svg, "summary")
        return 0
    ref_rows, rows = read_trace(a.ref), read_trace(a.trace)
    res = compare(ref_rows, rows)
    if "reason" in res:
        print("FAIL: " + res["reason"])
        return 1
    print(f"{'PASS' if res['pass'] else 'FAIL'}: largest gap {res['max_gap']:.3f} units "
          f"(at {res['max_gap_at_s']:.2f} s; tolerance {MAX_GAP}), speed off in "
          f"{len(res['speed_spans_off'])} of {res['spans']} spans (tolerance {SPEED_TOLERANCE:.0%}), "
          f"moving where the reference rests in {len(res['moving_where_ref_rests'])}")
    if a.svg:
        svg([("reference", aligned(ref_rows)), ("trace", aligned(rows))], a.svg,
            os.path.basename(a.trace))
    return 0 if res["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

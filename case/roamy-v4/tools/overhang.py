"""Printability check on the print-pose STLs (run `swift run` first): python3 tools/overhang.py [part ...]

Slices each part at the print layer height and finds material that the layer below doesn't carry: anything more
than ALLOW (one extrusion width) past the previous layer. Such a piece prints cleanly only as a bridge: straight
lines across it land on the layer below at both ends, and the next layer covers it. Anything else droops: a
cantilever (lines land at one end only), or a bare bridge (a one-layer skin with nothing on top, which prints as
loose strands). Drooping pieces that reach no further than OVERHANG print as overhang walls, which slicers slow
down for; they are listed but pass. Farther ones fail unless they lie in the part's painted-support zone
(`<part>-painted-support.stl`, the features that the slicer supports on purpose, such as the guide pins).

Needs trimesh, shapely (scripts/setup-cloud.sh). Exits non-zero on an unexpected drooping piece.
"""
import math, pathlib, sys
import numpy as np, trimesh
from shapely import affinity, unary_union
from shapely.geometry import LineString, MultiPolygon, Point, Polygon

ROOT = pathlib.Path(__file__).resolve().parents[3]
CHECK = ROOT / "build/roamy-v4/check"
LAYER = 0.2           # Parameters.swift P.layer
ALLOW = 0.45          # one extrusion width past the layer below prints without support
OVERHANG = 0.65       # about 1.5 extrusion widths; the r3 seat, at 0.8, drooped
TOLERANCE = 0.05      # slicing noise on tilted faces
MIN_AREA = 0.02       # mm^2; smaller pieces are slicing noise
MAX_BRIDGE = 20.0


def slices(mesh, heights):
    out = []
    for path in mesh.section_multiplane(plane_origin=[0, 0, 0], plane_normal=[0, 0, 1], heights=heights):
        polygons = [] if path is None else list(path.polygons_full)
        out.append(unary_union(polygons) if polygons else Polygon())
    return out


def polygons(geometry):
    if isinstance(geometry, Polygon):
        return [] if geometry.is_empty else [geometry]
    return [g for g in getattr(geometry, "geoms", []) if isinstance(g, Polygon)]


def reach(piece, below):
    """How far the piece reaches past the layer below: the farthest outline point from it."""
    return max(below.distance(Point(p)) for p in piece.exterior.segmentize(0.1).coords)


def bridged(piece, below):
    """True if, in some direction, straight lines across the piece land on the layer below at both ends."""
    anchored = lambda q: below.distance(Point(q)) <= ALLOW + TOLERANCE
    c = piece.centroid
    for angle in range(0, 180, 15):
        turned = affinity.rotate(piece, -angle, origin=c)
        x0, y0, x1, y1 = turned.bounds
        total = landed = 0.0
        for y in np.arange(y0 + 0.1, y1, 0.2):
            chord = LineString([(x0 - 1, y), (x1 + 1, y)]).intersection(turned)
            for seg in getattr(chord, "geoms", [chord]):
                if seg.is_empty or seg.geom_type != "LineString":
                    continue
                total += seg.length
                ends = [affinity.rotate(Point(q), angle, origin=c) for q in (seg.coords[0], seg.coords[-1])]
                if seg.length <= MAX_BRIDGE and all(anchored(e.coords[0]) for e in ends):
                    landed += seg.length
        if total and landed / total >= 0.9:
            return True
    return False


def check(part):
    mesh = trimesh.load(CHECK / f"{part}.stl")
    heights = np.arange(LAYER / 2, mesh.bounds[1][2], LAYER)
    layers = slices(mesh, heights)
    zone_file = CHECK / f"{part}-painted-support.stl"
    zones = slices(trimesh.load(zone_file), heights) if zone_file.exists() else [Polygon()] * len(heights)

    findings = []
    for i in range(1, len(layers)):
        below, above = layers[i - 1], layers[i + 1] if i + 1 < len(layers) else Polygon()
        for piece in polygons(layers[i].difference(below.buffer(ALLOW))):
            if piece.area < MIN_AREA or (d := reach(piece, below)) <= ALLOW + TOLERANCE:
                continue
            if bridged(piece, below):
                kind = "bridge" if piece.difference(above).area < 0.1 * piece.area else "bare bridge"
            else:
                kind = "cantilever"
            painted = piece.intersection(zones[i].buffer(0.5)).area > 0.5 * piece.area
            note = "painted support" if painted else "ok" if kind == "bridge" else "overhang" if d <= OVERHANG else "FAIL"
            findings.append((heights[i], kind, d, piece.centroid, note))

    failures = [f for f in findings if f[4] == "FAIL"]
    print(f"{part}: {len(layers)} layers, {len(findings)} unsupported pieces, {len(failures)} drooping")
    for z, kind, d, c, note in findings:
        print(f"  z {z:5.2f}  ({c.x:6.1f}, {c.y:6.1f})  {kind:11s} reaches {d:4.2f}  {note}")
    return not failures


parts = sys.argv[1:] or ["shell-print", "floor-print", "terminator-print"]
results = [check(p) for p in parts]
sys.exit(0 if all(results) else 1)

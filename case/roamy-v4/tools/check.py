"""Mesh checks on the check STLs (run `swift run` first): python3 tools/check.py

- Board positions against KiCad's own 3D export, independent of Frame.bx/by (catches a mirrored model).
- No overlap between the parts of two joined modules; header pins fully in the neighbour's sockets.
- The bare board rises out of the shell along its normal without contact.
- Sliding a neighbour on along its board plane touches only where the latch tooth rides the guide pin.
- Solder jig: no overlap with the board, straight lift-out, stops in contact.
Needs trimesh, manifold3d, lxml (scripts/setup-cloud.sh) and kicad-cli. Exits non-zero on a failure.
"""
import math, pathlib, re, subprocess, sys, tempfile
import numpy as np, trimesh, manifold3d as m3d

ROOT = pathlib.Path(__file__).resolve().parents[3]
CHECK, PCB = ROOT / "build/roamy-v4/check", ROOT / "electronics/KeyModule/KeyModule.kicad_pcb"
END_WALL, CLEARANCE, WALL = 4.4, 0.2, 1.8             # Parameters.swift
BOARD_X0, BOARD_Y0, BOARD_LENGTH = 70.55, 40.0, 100.0
failures = []

def man(name):
    t = trimesh.load(CHECK / f"{name}.stl", process=True)
    return m3d.Manifold(m3d.Mesh(vert_properties=np.asarray(t.vertices, dtype=np.float32),
                                 tri_verts=np.asarray(t.faces, dtype=np.uint32)))

def overlap(a, b, move=(0, 0, 0)):
    return (a ^ b.translate(list(move))).volume()

def solid_at(m, points, r=0.05):
    return np.array([overlap(m, m3d.Manifold.cube([2 * r] * 3).translate(list(q - r))) > 1e-6 for q in points])

def expect(ok, text):
    print(("ok    " if ok else "FAIL  ") + text)
    if not ok:
        failures.append(text)

P = {n: man(n) for n in ["shell", "floor", "board", "bare-board", "right-shell", "right-floor", "right-board", "jig"]}

# KiCad cross-check. glTF is right-handed: x = KiCad x, y toward the front, z = KiCad y. Turn it front-up and
# place the board where the case holds it; nothing here goes through Frame.bx/by.
with tempfile.TemporaryDirectory() as tmp:
    glb = pathlib.Path(tmp) / "board.glb"
    subprocess.run(["kicad-cli", "pcb", "export", "glb", "--no-components", "--include-pads", "--output", str(glb), str(PCB)],
                   check=True, capture_output=True)
    scene = trimesh.load(glb)
    pads = []
    for node in scene.graph.nodes_geometry:
        transform, geometry = scene.graph[node]
        if "pad" in geometry:
            pads.append(trimesh.transformations.transform_points(scene.geometry[geometry].vertices, transform).mean(axis=0) * 1000)
g = np.array(pads)
p = np.c_[g[:, 0] - BOARD_X0 + WALL + CLEARANCE, -g[:, 2] + BOARD_Y0 + BOARD_LENGTH + END_WALL + CLEARANCE, g[:, 1]]
front = p[p[:, 2] > 0.8] * [1, 1, 0] + [0, 0, 1.6 + 0.3]
over = np.sum(~solid_at(P["jig"], front))
expect(over == len(front), f"jig: {over}/{len(front)} front pad pieces over pockets")
refs = {}
for m in re.finditer(r'\(footprint "[^"]+"(.*?)\n\t\)', PCB.read_text(), re.S):
    ref = re.search(r'\(property "Reference" "([^"]+)"', m.group(1)).group(1)
    refs[ref] = tuple(float(v) for v in re.search(r'\(at ([\d.\-]+) ([\d.\-]+)', m.group(1)).groups())
for prefix, x, z in [("J_LEFT", 0.5, -1.0), ("J_RIGHT", 20.3, -1.2)]:
    ys = [BOARD_Y0 + BOARD_LENGTH + END_WALL + CLEARANCE - refs[f"{prefix}{i}"][1] for i in (1, 2, 3)]
    pins = np.array([[x, y + d, z] for y in ys for d in (-2.54, 0, 2.54)])
    open_ = np.sum(~solid_at(P["shell"], pins))
    expect(open_ == len(pins), f"shell: {open_}/{len(pins)} {prefix} pins at wall openings")

# Two joined modules
pairs = [("shell", "board"), ("floor", "board"), ("shell", "floor"), ("shell", "right-shell"), ("shell", "right-board"),
         ("right-shell", "board"), ("floor", "right-floor"), ("floor", "right-board"), ("right-floor", "board"),
         ("shell", "right-floor"), ("right-shell", "floor")]
worst = max(overlap(P[a], P[b]) for a, b in pairs)
expect(worst < 1e-3, f"joined modules: largest overlap {worst:.4f} mm³")
pins = overlap(P["board"], P["right-board"])
expect(abs(pins - 9 * 0.64 ** 2 * 5.615) < 0.1, f"header pins in the neighbour's sockets: {pins:.2f} mm³ (9 × 0.64² × 5.6)")

worst = max(overlap(P["shell"], P["bare-board"], (0, 0, -d)) for d in np.arange(0, 16, 0.25))
expect(worst < 1e-3, f"board drops into the shell: largest overlap {worst:.4f} mm³")

a = math.radians(8)
along = np.array([math.cos(a), 0, -math.sin(a)])
touch = [d for d in np.arange(0, 9, 0.25) if overlap(P["shell"], P["right-shell"], along * d) > 1e-3]
others = max(overlap(P[x], P[y], along * d) for d in np.arange(0.25, 9, 0.25)
             for x, y in [("shell", "right-board"), ("board", "right-shell"), ("floor", "right-floor")])
expect(others < 1e-3 and touch and max(touch) <= 3.0,
       f"slide-on: shells touch only within {max(touch, default=0):.2f} mm of home (tooth), nothing else touches")

expect(overlap(P["jig"], P["bare-board"]) < 1e-3, "jig: no overlap with the board")
worst = max(overlap(P["jig"], P["bare-board"], (0, 0, -d)) for d in np.arange(0, 12, 0.25))
expect(worst < 1e-3, f"jig: board lifts straight out (largest overlap {worst:.4f} mm³)")
for move, what in [((-0.05, 0, 0), "socket mouth stops"), ((0.05, 0, 0), "header stops"), ((0, 0, 0.05), "floors")]:
    expect(overlap(P["jig"], P["bare-board"], move) > 1e-3, f"jig: {what} in contact")

sys.exit(1 if failures else 0)

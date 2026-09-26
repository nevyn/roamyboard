"""Mesh checks on the check STLs (run `swift run` first): python3 tools/check.py

- Board positions against KiCad's own 3D export, independent of Frame.bx/by (catches a mirrored model).
- No overlap between the parts of two joined modules; header pins fully in the neighbour's sockets.
- The bare board rises out of the shell along its normal without contact.
- Sliding a neighbour on along its board plane touches only where the latch tooth rides the guide pin.
- Solder jig: no overlap with the board, straight lift-out, stops in contact.
- Terminator module: its header side meets the contract, the embedded headers fit their pockets, lie below the
  embed pause and sit in the neighbour's sockets; nothing collides with the neighbour, joined or during slide-on.
- JointSide contract: within the body, a joint side's cuts and parts stay in its reserved blocks, the shell has no
  features of its own there, and nothing but the joint's own parts sits in its keep-out.
Needs trimesh, manifold3d, lxml (scripts/setup-cloud.sh) and kicad-cli. Exits non-zero on a failure.
"""
import math, pathlib, re, subprocess, sys, tempfile
import numpy as np, trimesh, manifold3d as m3d

ROOT = pathlib.Path(__file__).resolve().parents[3]
CHECK, PCB = ROOT / "build/roamy-v4/check", ROOT / "electronics/KeyModule/KeyModule.kicad_pcb"
PARAMS = (ROOT / "case/roamy-v4/Sources/roamy-v4/Parameters.swift").read_text()
END_WALL = float(re.search(r"static let endWall = ([\d.]+)", PARAMS).group(1))
CLEARANCE, WALL = 0.2, 1.8                            # Parameters.swift
BOARD_X0, BOARD_Y0, BOARD_LENGTH = 70.55, 40.0, 100.0
failures = []

def man(name):
    if (CHECK / f"{name}.stl").stat().st_size <= 84:          # binary STL header and a zero count: nothing
        return m3d.Manifold()
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

P = {n: man(n) for n in ["shell", "floor", "board", "bare-board", "right-shell", "right-floor", "right-board", "jig",
                             "terminator", "terminator-embedded", "terminator-embedded-print"]}

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

touch = [d for d in np.arange(0, 9, 0.25) if overlap(P["terminator"], P["right-shell"], along * d) > 1e-3]
expect(touch and max(touch) <= 3.0, f"terminator slide-on: touches only within {max(touch, default=0):.2f} mm of home (tooth)")

for move, what in [((0.15, 0, 0), "+x"), ((-0.15, 0, 0), "-x"), ((0, 0.15, 0), "+y"), ((0, -0.15, 0), "-y")]:
    expect(overlap(P["shell"], P["floor"], move) > 1e-3, f"floor: tabs locate it in the shell ({what} 0.15 mm collides)")
# M2 screws: shank through the floor's clearance, biting into the shell's pilot hole
screw_x = [float(v) for v in re.search(r"static let screwXs = \[([^\]]+)\]", PARAMS).group(1).split(",")]
screw_y = float(re.search(r"static let screwY = ([\d.]+)", PARAMS).group(1))
length = END_WALL * 2 + BOARD_LENGTH + 2 * CLEARANCE
shanks = [m3d.Manifold.cylinder(8, 1.0, 1.0, 32).translate([x, y, -8]) for x in screw_x for y in (screw_y, length - screw_y)]
expect(max(overlap(P["floor"], s_) for s_ in shanks) < 1e-3, f"screws: {len(shanks)} M2 shanks clear the floor")
expect(min(overlap(P["shell"], s_) for s_ in shanks) > 0.5, "screws: every shank bites into the shell")
expect(overlap(P["jig"], P["bare-board"]) < 1e-3, "jig: no overlap with the board")
worst = max(overlap(P["jig"], P["bare-board"], (0, 0, -d)) for d in np.arange(0, 12, 0.25))
expect(worst < 1e-3, f"jig: board lifts straight out (largest overlap {worst:.4f} mm³)")
for move, what in [((-0.05, 0, 0), "socket mouth stops"), ((0.05, 0, 0), "header stops"), ((0, 0, 0.05), "floors")]:
    expect(overlap(P["jig"], P["bare-board"], move) > 1e-3, f"jig: {what} in contact")

# JointSide contract. Inside reserved, the shell is the outline minus the joint's cuts plus its parts; outside the body,
# the keep-out holds nothing but the joint's parts.
def contract(host, outline, sides):
    for side in sides:
        j = {k: man(f"{side}-joint-{k}") for k in ("reserved", "removed", "added", "keep-out")}
        region = outline ^ j["reserved"]
        expected = (region - j["removed"]) + (j["added"] ^ j["reserved"])
        got = P[host] ^ j["reserved"]
        stray = (got - expected).volume() + (expected - got).volume()
        expect(stray < 1e-3, f"{host}, {side} joint side: matches the joint inside reserved ({stray:.4f} mm³ differ)")
        spill = (((j["removed"] + j["added"]) ^ outline) - j["reserved"]).volume()
        expect(spill < 1e-3, f"{host}, {side} joint side: its cuts and parts stay in reserved within the body ({spill:.4f} mm³ outside)")
        intrusion = ((P[host] - j["added"]) ^ j["keep-out"]).volume()
        expect(intrusion < 1e-3, f"{host}, {side} joint side: keep-out clear of the {host} ({intrusion:.4f} mm³)")

contract("shell", man("shell-outline"), ("header", "socket"))

# Terminator module: key module frame, neighbour at the joint pose (right-*)
contract("terminator", man("terminator-outline"), ("header",))
expect(overlap(P["terminator"], P["terminator-embedded"]) < 1e-3, "terminator: headers and wire fit their pockets")
pause = float((CHECK / "terminator-pause.txt").read_text())
top = P["terminator-embedded-print"].bounding_box()[5]
expect(top <= pause, f"terminator: embedded parts below the pause ({top:.2f} ≤ {pause:.2f} mm)")
worst = max(overlap(P["terminator"], P[n]) for n in ["right-shell", "right-board", "right-floor"])
worst = max(worst, overlap(P["terminator-embedded"], P["right-shell"]))
expect(worst < 1e-3, f"terminator: no overlap with its neighbour ({worst:.4f} mm³)")
pins = overlap(P["terminator-embedded"], P["right-board"])
expect(abs(pins - 6 * 0.64 ** 2 * 5.615) < 0.1, f"terminator: header pins in the neighbour's sockets: {pins:.2f} mm³ (6 × 0.64² × 5.6)")

sys.exit(1 if failures else 0)

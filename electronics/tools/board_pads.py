"""Print every footprint's pads with absolute board positions and nets.

    python3 board_pads.py [board.kicad_pcb]
"""
import math, pathlib, re, sys

path = sys.argv[1] if len(sys.argv) > 1 else str(pathlib.Path(__file__).resolve().parent.parent / "KeyModule" / "KeyModule.kicad_pcb")
s = open(path).read()
for b in re.findall(r'\t\(footprint "[^"]+".*?\n\t\)\n', s, flags=re.S):
    layer = re.search(r'\n\t\t\(layer "([^"]+)"\)', b).group(1)
    fx, fy, rot = map(float, re.search(r'\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', b).groups(default="0"))
    ref = re.search(r'\(property "Reference" "([^"]+)"', b).group(1)
    a = math.radians(rot); out = []
    for p in re.finditer(r'\(pad "([^"]+)" (\w+) \w+\s*\(at ([-\d.]+) ([-\d.]+)(?: [-\d.]+)?\)(?:(?!\(pad ).)*?\(net "([^"]+)"\)', b, flags=re.S):
        n, kind, px, py, net = p.groups(); px, py = float(px), float(py)
        x = fx + px * math.cos(a) + py * math.sin(a); y = fy - px * math.sin(a) + py * math.cos(a)
        out.append(f"{n}@({x:.3f},{y:.3f}) {net}")
    print(ref, layer, f"({fx},{fy},{int(rot)})", " | ".join(out))

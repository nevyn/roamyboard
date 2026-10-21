"""Print every footprint's pads with absolute board positions and nets.

    python3 board_pads.py [board.kicad_pcb]

`footprints(path)` returns the same data for other tools: {ref: {pad number: (x, y, net)}}.
"""
import math, pathlib, re, sys

DEFAULT = pathlib.Path(__file__).resolve().parent.parent / "KeyModule" / "KeyModule.kicad_pcb"


def parse(path=DEFAULT):
    s = open(path).read()
    for b in re.findall(r'\t\(footprint "[^"]+".*?\n\t\)\n', s, flags=re.S):
        layer = re.search(r'\n\t\t\(layer "([^"]+)"\)', b).group(1)
        fx, fy, rot = map(float, re.search(r'\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', b).groups(default="0"))
        ref = re.search(r'\(property "Reference" "([^"]+)"', b).group(1)
        a = math.radians(rot); pads = []
        for p in re.finditer(r'\(pad "([^"]+)" (\w+) \w+\s*\(at ([-\d.]+) ([-\d.]+)(?: [-\d.]+)?\)(?:(?!\(pad ).)*?\(net "([^"]+)"\)', b, flags=re.S):
            n, kind, px, py, net = p.groups(); px, py = float(px), float(py)
            pads.append((n, fx + px * math.cos(a) + py * math.sin(a), fy - px * math.sin(a) + py * math.cos(a), net))
        yield ref, layer, (fx, fy, rot), pads


def footprints(path=DEFAULT):
    return {ref: {n: (x, y, net) for n, x, y, net in pads} for ref, _, _, pads in parse(path)}


if __name__ == "__main__":
    for ref, layer, (fx, fy, rot), pads in parse(sys.argv[1] if len(sys.argv) > 1 else DEFAULT):
        print(ref, layer, f"({fx},{fy},{int(rot)})", " | ".join(f"{n}@({x:.3f},{y:.3f}) {net}" for n, x, y, net in pads))

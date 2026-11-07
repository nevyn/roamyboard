"""Draw the pointing module's bench diagrams into docs/images/.

    python3 pointing_diagrams.py

pointing-rework.svg: the MCU module rework, on the socket board's back with the nice!nano underside up, laid out
like bench-wiring.svg. Pad positions come from KeyModule.kicad_pcb.
pointing-bench.svg: the TPS48's solder pads by silkscreen, and the jumpers from them into the MCU module's sockets.
pointing-u1-marker.svg: K's board marks the pointing module: U1 pin 5 lifted off its pad and wired to pin 16.
pointing-sockets.svg: V's loose sockets wired to the J_LEFT pads of K's board.
"""
import math, pathlib
from board_pads import footprints

OUT = pathlib.Path(__file__).resolve().parents[2] / "docs" / "images"
FONT = 'font-family="Helvetica, Arial, sans-serif"'
SDA, SCL, INT, VCC, GND, RES, OLD = "#1f77b4", "#9467bd", "#17becf", "#d62728", "#222222", "#8c564b", "#bbbbbb"


def text(x, y, s, size=11, anchor="start", weight="normal", fill="#222"):
    return f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" text-anchor="{anchor}" font-weight="{weight}" fill="{fill}" {FONT}>{s}</text>'


def line(points, color, width=3, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"{d}/>'


def svg(w, h, body):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}"><rect width="{w}" height="{h}" fill="#fff"/>\n' + "\n".join(body) + "\n</svg>\n"


def rework():
    fp = footprints()
    s, x0, y0 = 4.6, 170.0, 110.0                     # bench-wiring.svg's scale, shifted right for the labels
    bx = lambda x: x0 + (87.4 - x) * s                 # back view: the header side is on the left
    by = lambda y: y0 + (y - 40.0) * s
    c = x0 + 16.85 * s / 2
    b = [text(c, 72, "Socket board", 15, "middle", "bold"), text(c, 88, "back (connector side) up", 11, "middle"),
         text(c, 102, "SW5 end", 10, "middle", fill="#555"), text(c, 584, "SW1 end", 10, "middle", fill="#555"),
         f'<rect x="{bx(87.4)}" y="{by(40)}" width="{16.85 * s:.1f}" height="{100 * s:.1f}" rx="4" fill="#3d2b5a" stroke="#222"/>']
    for ref in ("J_LEFT1", "J_LEFT2", "J_LEFT3"):
        ys = [p[1] for p in fp[ref].values()]
        b.append(f'<rect x="{bx(77.05):.1f}" y="{by(min(ys) - 3.94):.1f}" width="{8.5 * s:.1f}" height="{7.87 * s:.1f}" fill="#111" opacity="0.85"/>')
        for n, (x, y, net) in fp[ref].items():
            b.append(f'<rect x="{bx(x) - 3.4:.1f}" y="{by(y) - 2.3:.1f}" width="6.8" height="4.6" fill="#c9a227"/>')
    for ref in ("SW1", "SW2"):
        x, y, _ = fp[ref]["1"]
        b.append(f'<rect x="{bx(x) - 6:.1f}" y="{by(y) - 6:.1f}" width="12" height="12" fill="#c9a227" stroke="{VCC}" stroke-width="1.5"/>')
        b.append(text(bx(87.4) - 6, by(y) + 4, f"{ref} pad 1, +3.3V", 10, "end", "bold", VCC))

    # existing wires, faint
    for ref, n in (("J_LEFT1", "2"), ("J_LEFT1", "3"), ("J_LEFT2", "1"), ("J_LEFT2", "2"), ("J_LEFT2", "3")):
        x, y, _ = fp[ref][n]
        b.append(line([(bx(x), by(y)), (bx(x) + 70, by(y))], OLD, 3))
    b.append(text(bx(79.7) + 74, by(77.1) + 4, "existing wires: leave them", 10, fill="#888"))

    # nice!nano underside up, rows as in bench-wiring.svg
    nx0, nx1, ny, pitch = 470.0, 620.0, 154.0, 22.0
    left = ["RAW", "GND", "RST", "VCC", "031", "029", "002", "115", "113", "111", "010", "009"]
    right = ["006", "008", "GND", "GND", "017", "020", "022", "024", "100", "011", "104", "106"]
    b += [text(545, 46, "nice!nano v2, underside up", 15, "middle", "bold"), text(545, 62, "USB-C end", 10, "middle", fill="#555"),
          f'<rect x="{nx0}" y="{ny - 22}" width="{nx1 - nx0}" height="{pitch * 12 + 14}" rx="6" fill="#1b3a6b" stroke="#222"/>']
    names = {"022": "D4", "024": "D5", "100": "D6"}
    for i, (l, r) in enumerate(zip(left, right)):
        y = ny + i * pitch
        for x, label, anchor, dx in ((nx0 + 14, l, "start", 0), (nx1 - 14, r, "end", 0)):
            hot = label in names
            b.append(f'<circle cx="{x - 6 if anchor == "start" else x + 6:.1f}" cy="{y - 4:.1f}" r="4.5" fill="{"#fff" if hot else "#c9a227"}" stroke="#222"/>')
            b.append(text(x + 4 if anchor == "start" else x - 4, y, label + (f" = {names[label]}" if hot else ""), 11, anchor, "bold" if hot else "normal", "#fff"))

    # new wires: from the D4/D5/D6 pads around the nano's far end to the socket joints
    pin = lambda label: (nx1 - 8, ny + right.index(label) * pitch - 4)
    route = [("022", "J_LEFT3", "2", SDA, "SDA", 0), ("024", "J_LEFT3", "3", SCL, "SCL", 1), ("100", "J_LEFT3", "1", INT, "INT", 2)]
    for label, ref, n, color, name, k in route:
        px, py = pin(label)
        x, y, _ = fp[ref][n]
        out = {"INT": 636, "SDA": 664, "SCL": 678}[name]   # nested lanes, so no two wires cross
        b.append(line([(px, py), (out, py), (out, by(y)), (bx(x), by(y))], color, 3))
        b.append(line([(720, 455 + k * 20), (750, 455 + k * 20)], color, 3))
        b.append(text(756, 459 + k * 20, f"{name}: {names[label]} ({label}) to {ref} pin {n}", 11, weight="bold", fill=color))

    # pull-ups: body along the line between the joint and the +3.3V pad
    for (ref, n), sw, name in ((("J_LEFT3", "2"), "SW2", "SDA"), (("J_LEFT3", "3"), "SW1", "SCL")):
        x1, y1, _ = fp[ref][n]; x2, y2, _ = fp[sw]["1"]
        ax, ay, cx, cy = bx(x1), by(y1), bx(x2), by(y2)
        mx, my = (ax + cx) / 2, (ay + cy) / 2
        b.append(line([(ax, ay), (cx, cy)], "#999", 1.6))
        ang = math.degrees(math.atan2(cy - ay, cx - ax))
        b.append(f'<rect x="{mx - 15:.1f}" y="{my - 5:.1f}" width="30" height="10" rx="3" fill="#e8d2a6" stroke="{RES}" transform="rotate({ang:.1f} {mx:.1f} {my:.1f})"/>')
        b.append(text(bx(87.4) - 6, my + 4, f"2.2 kΩ, {name} to +3.3V", 10, "end", "bold", RES))

    notes = ["New: three wires from the nano to the socket joints, and two 2.2 kΩ resistors from SDA and SCL to +3.3V.",
             "Pin 1 of each connector is nearest the SW5 end (the USB-C end). J_LEFT3 pin 1 was LED; LED moves to J_LEFT1 pin 1 later.",
             "Lay each resistor flat on the board, its leads leaving the socket joint away from the socket body.",
             "Checked in the case model: a 1/4 W body (2.5 × 6.5 mm) clears the floor by 0.5 mm, a 1/8 W body by 0.8 mm."]
    b += [text(30, 640 + 18 * i, t, 11) for i, t in enumerate(notes)]
    return svg(980, 720, b)


def bench():
    b = [text(30, 36, "Wire the TPS48 to the MCU module's sockets", 16, weight="bold"),
         text(30, 54, "Pads by their silkscreen on the trackpad's back, in order from the notch end. Schematic, not to scale.", 11)]
    # trackpad back: the seven solder pads in a column, notch end at the top
    px, py0, pitch = 210.0, 178.0, 36.0
    pads = [("IO4", None, None), ("IO5", "RDY", INT), ("GND", "VSS", GND), ("3V3", "VDDHI", VCC),
            ("IO1", "SCL", SCL), ("IO2", "SDA", SDA), ("IO3", None, None)]
    b.append(f'<path d="M60,{py0 - 34} H{px - 26} L{px + 6},{py0 - 34 + 32} V{py0 + 6 * pitch + 34} H60 Z" fill="#3d2b5a" stroke="#222"/>')
    b.append(text(px - 4, py0 - 42, "notch end", 10, "end", fill="#555"))
    b.append(text(110, py0 + 3 * pitch + 4, "TPS48 back", 12, "middle", "bold", "#eee"))
    pad_y = {}
    for i, (name, chip, color) in enumerate(pads):
        y = py0 + i * pitch
        pad_y[name] = y
        b.append(f'<rect x="{px - 16}" y="{y - 7}" width="16" height="14" fill="#c9a227"/>')
        b.append(text(px - 22, y + 5, name, 13, "end", "bold", "#fff"))
        if chip is None:
            b.append(text(px + 12, y + 4, "leave empty", 10, fill="#999"))

    # MCU module socket face: J_LEFT1 at the SW5 end, J_LEFT3 near the SW1 end
    fx0, fy0, s = 640.0, 160.0, 2.6
    fy = lambda y: fy0 + (y - 40) * s
    b.append(f'<rect x="{fx0}" y="{fy(40)}" width="40" height="{100 * s}" rx="4" fill="#ddd" stroke="#888"/>')
    b.append(text(fx0 + 20, fy(40) - 6, "SW5 end (USB-C)", 9, "middle", fill="#555"))
    b.append(text(fx0 + 20, fy(140) + 14, "SW1 end", 9, "middle", fill="#555"))
    b.append(text(fx0 + 20, fy(140) + 30, "MCU module, socket face", 10, "middle", fill="#555"))
    fp = footprints()
    jump = {("J_LEFT3", "1"): ("INT", "IO5", INT, 330), ("J_LEFT1", "2"): ("+3.3V", "3V3", VCC, 450),
            ("J_LEFT1", "3"): ("GND", "GND", GND, 390), ("J_LEFT3", "2"): ("SDA", "IO2", SDA, 570),
            ("J_LEFT3", "3"): ("SCL", "IO1", SCL, 510)}
    for ref in ("J_LEFT1", "J_LEFT2", "J_LEFT3"):
        mid = fy(fp[ref]["2"][1])
        for n, (_, y, net) in fp[ref].items():
            yy, ly = fy(y), mid + (int(n) - 2) * 22
            b.append(f'<rect x="{fx0 + 14}" y="{yy - 3}" width="12" height="6" fill="#111"/>')
            j = jump.get((ref, n))
            if j:
                sig, pad, color, elbow = j
                b.append(line([(px, pad_y[pad]), (elbow, pad_y[pad]), (elbow, yy), (fx0 + 14, yy)], color, 3))
                b.append(line([(fx0 + 26, yy), (fx0 + 54, ly), (fx0 + 62, ly)], color, 1.5))
                b.append(text(fx0 + 68, ly + 4, f"pin {n}  {sig}  from {pad}", 11, weight="bold", fill=color))
            elif ref == "J_LEFT1":
                b.append(line([(fx0 + 26, yy), (fx0 + 54, ly), (fx0 + 62, ly)], "#bbb", 1))
                b.append(text(fx0 + 68, ly + 4, f"pin {n}  +5V, later LED: leave empty", 10, fill="#999"))
        b.append(text(fx0 + 68, mid - 40 if ref != "J_LEFT2" else mid + 4, ref if ref != "J_LEFT2" else "J_LEFT2: leave empty",
                      11 if ref != "J_LEFT2" else 10, weight="bold" if ref != "J_LEFT2" else "normal", fill="#222" if ref != "J_LEFT2" else "#999"))
    notes = ["IO3 and IO4 stay unconnected. The trackpad has no NRST pad: the IQS572 pulls it up itself.",
             "In IQS572 terms: IO5 is RDY (pin 8), IO2 SDA (pin 2), IO1 SCL (pin 3), 3V3 VDDHI (pin 4), GND VSS (pin 5).",
             "Switch the half off before you plug the jumpers in. With the pad in the sockets, this half reads 0 key modules.",
             "Tape a 1 to 2 mm plastic overlay on the pad's face, without air gaps, and leave solder link S1 open."]
    b += [text(30, 470 + 18 * i, t, 11) for i, t in enumerate(notes)]
    return svg(980, 560, b)


def marker():
    fp = footprints()
    u1 = fp["U1"]
    s, x0, y0 = 44.0, 40.0, 150.0                     # back view, as in pointing-rework.svg, zoomed on U1
    bx = lambda x: x0 + (87.0 - x) * s
    by = lambda y: y0 + (y - 92.0) * s
    lift, gnd, wire = "#e67e22", "#555555", "#2ca02c"
    b = [text(30, 36, "Mark K's board as a pointing module", 16, weight="bold"),
         text(30, 56, "U1 (74HC165) on the board's back: lift pin 5 (input G) off its pad and wire it to pin 16 (+3.3V).", 11),
         text(30, 72, "Back view, as the board lies in the jig: SW5 end up, header side left. The pin 1 corner of the package is bevelled or dotted.", 11, fill="#555"),
         f'<rect x="{bx(86.6)}" y="{by(92.3)}" width="{10.4 * s}" height="{9.0 * s}" rx="6" fill="#3d2b5a"/>']
    # package body, 3.9 x 9.9 mm, centred on the footprint
    cx, cy = 81.325, 96.7
    b.append(f'<rect x="{bx(cx + 4.95)}" y="{by(cy - 1.95)}" width="{9.9 * s}" height="{3.9 * s}" rx="4" fill="#222" stroke="#000"/>')
    b.append(f'<circle cx="{bx(cx - 4.3):.1f}" cy="{by(cy - 1.3):.1f}" r="6" fill="#777"/>')  # pin 1 corner
    b.append(text(bx(cx), by(cy) + 6, "74HC165", 16, "middle", "bold", "#eee"))
    for n, (x, y, net) in u1.items():
        n = int(n)
        top = y < cy
        foot_y0, foot_y1 = (y - 0.75, y + 0.75)
        pad = f'<rect x="{bx(x) - 0.3 * s:.1f}" y="{by(y - 0.8):.1f}" width="{0.6 * s:.1f}" height="{1.6 * s:.1f}" fill="#c9a227"/>'
        b.append(pad)
        edge = cy - 1.95 if top else cy + 1.95
        if n == 5:
            b.append(f'<rect x="{bx(x) - 0.3 * s:.1f}" y="{by(y - 0.8):.1f}" width="{0.6 * s:.1f}" height="{1.6 * s:.1f}" fill="none" stroke="{lift}" stroke-width="3" stroke-dasharray="5 3"/>')
            b.append(f'<rect x="{bx(x) - 0.22 * s:.1f}" y="{by(edge - 0.55):.1f}" width="{0.44 * s:.1f}" height="{0.55 * s:.1f}" fill="#bbb" stroke="{lift}" stroke-width="2"/>')
        else:
            b.append(f'<rect x="{bx(x) - 0.22 * s:.1f}" y="{by(min(edge, y)):.1f}" width="{0.44 * s:.1f}" height="{abs(edge - y) * s:.1f}" fill="#bbb"/>')
        label_y = by(y - 1.25) if top else by(y + 1.25) + 12
        color = lift if n == 5 else VCC if net == "+3.3V" else gnd if net == "GND" else "#999"
        b.append(text(bx(x), label_y, str(n), 13 if n in (5, 16) else 11, "middle", "bold" if n in (5, 16) else "normal", color))
        if net == "GND" and n != 5:
            b.append(text(bx(x), label_y + (-13 if top else 13), "GND", 9, "middle", fill=gnd))
    # the jumper: from pin 5's lifted foot, over the package, to pin 16's joint
    x5, y5, _ = u1["5"]; x16, y16, _ = u1["16"]
    p5 = (bx(x5), by(cy - 1.95 - 0.55)); p16 = (bx(x16), by(y16))
    b.append(f'<path d="M{p5[0]:.1f},{p5[1]:.1f} C{p5[0]:.1f},{p5[1] - 60:.1f} {p16[0] + 40:.1f},{by(cy):.1f} {p16[0]:.1f},{p16[1]:.1f}" fill="none" stroke="{wire}" stroke-width="5" stroke-linecap="round"/>')
    b.append(text(bx(x16) - 14, by(y16) + 52, "pin 16 = +3.3V (VCC)", 12, "end", "bold", VCC))
    b.append(text(bx(x5), by(92.3) - 8, "pin 5 = input G: lifted", 12, "middle", "bold", lift))
    b.append(text(bx(cx), by(cy) + 30, "insulated wire over the package", 12, "middle", fill=wire))

    # side view of the top row: pin 5 bent up off its pad
    sx, sy, sk = 640.0, 170.0, 30.0
    b.append(text(sx, sy - 34, "Side view of pins 4, 5 and 6", 13, weight="bold"))
    b.append(f'<rect x="{sx - 10}" y="{sy + 70}" width="{5.6 * sk}" height="8" fill="#3d2b5a"/>')
    b.append(f'<rect x="{sx - 10}" y="{sy + 10}" width="{5.6 * sk}" height="{1.5 * sk}" rx="4" fill="#222"/>')
    for i, n in enumerate((4, 5, 6)):
        px = sx + 20 + i * 1.27 * sk * 1.0 + i * 15
        b.append(f'<rect x="{px - 10}" y="{sy + 66}" width="20" height="4" fill="#c9a227"/>')
        if n == 5:
            b.append(f'<polyline points="{px},{sy + 32} {px},{sy + 46} {px + 6},{sy + 28} {px + 22},{sy + 20}" fill="none" stroke="{lift}" stroke-width="4"/>')
            b.append(f'<path d="M{px + 22},{sy + 20} C{px + 60},{sy - 10} {px + 120},{sy - 20} {px + 140},{sy - 6}" fill="none" stroke="{wire}" stroke-width="4"/>')
            b.append(text(px, sy + 92, "5", 12, "middle", "bold", lift))
            b.append(text(px + 145, sy - 10, "to pin 16", 11, fill=wire))
        else:
            b.append(f'<polyline points="{px},{sy + 32} {px},{sy + 66} {px + 8},{sy + 66}" fill="none" stroke="#bbb" stroke-width="4"/>')
            b.append(text(px, sy + 92, str(n), 11, "middle", fill=gnd))
    b.append(text(sx - 10, sy + 116, "Pin 5's foot clears its pad by about 0.5 mm.", 11))
    b.append(text(sx - 10, sy + 132, "The pad stays on the GND pour: keep it bare.", 11))

    notes = ["1. Heat pin 5's joint and lift the foot with tweezers or a fine pick, bending the leg at its knee, until it clears the pad.",
             "2. Wick the pad flat, so that no solder can bridge to the lifted foot. Pins 4 and 6 stay soldered: they are GND.",
             "3. Solder a short piece of thin insulated wire (wire-wrap wire) from pin 5's foot tip to pin 16's joint, over the package.",
             "4. Check with a meter: continuity from pin 5 to pin 16, none from pin 5 to pin 4, 6 or 8."]
    b += [text(30, 590 + 18 * i, t, 11) for i, t in enumerate(notes)]
    return svg(980, 670, b)


def sockets():
    fp = footprints()
    s, x0, y0 = 4.6, 110.0, 120.0                     # back view, as in pointing-rework.svg: header side on the left
    bx = lambda x: x0 + (87.4 - x) * s
    by = lambda y: y0 + (y - 40.0) * s
    LED, CLK, PL, DIN = "#ff7f0e", "#2ca02c", "#e377c2", RES
    sig = {("J_LEFT1", "1"): ("+5V / LED", LED), ("J_LEFT1", "2"): ("+3.3V", VCC), ("J_LEFT1", "3"): ("GND", GND),
           ("J_LEFT2", "1"): ("CLK", CLK), ("J_LEFT2", "2"): ("/PL", PL), ("J_LEFT2", "3"): ("DATA_IN", DIN),
           ("J_LEFT3", "1"): ("INT", INT), ("J_LEFT3", "2"): ("SDA", SDA), ("J_LEFT3", "3"): ("SCL", SCL)}
    trackpad = {("J_LEFT1", "2"): "3V3", ("J_LEFT1", "3"): "GND", ("J_LEFT3", "1"): "IO5", ("J_LEFT3", "2"): "IO2",
                ("J_LEFT3", "3"): "IO1"}
    b = [text(30, 36, "Wire V's loose sockets to K's board", 16, weight="bold"),
         text(30, 54, "Back view, as K's board lies in the upturned shell. V's floor is laid out flat beside it; "
                      "in the module it sits 16° to K's board.", 11, fill="#555")]
    kx, kw = bx(87.4), 16.85 * s
    b.append(f'<rect x="{kx}" y="{by(40)}" width="{kw:.1f}" height="{100 * s:.1f}" rx="4" fill="#3d2b5a" stroke="#222"/>')
    b.append(text(kx + kw / 2, by(40) - 26, "K's board", 13, "middle", "bold"))
    b.append(text(kx + kw / 2, by(40) - 12, "back up", 10, "middle", fill="#555"))
    b.append(text(kx + kw / 2, by(40) + 16, "SW5 end", 9, "middle", fill="#ddd"))
    b.append(text(kx + kw / 2, by(140) - 8, "SW1 end", 9, "middle", fill="#ddd"))
    b.append(text(kx - 8, by(40) + 60, "header side", 10, "end", fill="#555"))
    b.append(text(kx + kw + 8, by(40) + 60, "socket side", 10, fill="#555"))
    xs = [v[0] for v in fp["U1"].values()]
    ys = [v[1] for v in fp["U1"].values()]
    b.append(f'<rect x="{bx(max(xs)):.1f}" y="{by(min(ys)) - 3:.1f}" width="{(max(xs) - min(xs)) * s:.1f}" height="{(max(ys) - min(ys)) * s + 6:.1f}" fill="#222" opacity="0.7"/>')
    b.append(text(bx(81.3), by(96.7) + 4, "U1", 10, "middle", fill="#ddd"))
    for ref in ("J_RIGHT1", "J_RIGHT2", "J_RIGHT3"):
        for x, y, _ in fp[ref].values():
            b.append(f'<rect x="{bx(x) - 3:.1f}" y="{by(y) - 3:.1f}" width="6" height="6" fill="#888"/>')

    # V's floor with the three sockets in their cradles, tails toward K, mouths toward V's socket face
    vx0, vx1 = 400.0, 500.0
    b.append(f'<rect x="{vx0}" y="{by(40)}" width="{vx1 - vx0}" height="{100 * s:.1f}" rx="4" fill="#f1e6f5" stroke="#b9a3c4"/>')
    b.append(text((vx0 + vx1) / 2, by(40) - 26, "V's floor", 13, "middle", "bold"))
    b.append(text((vx0 + vx1) / 2, by(40) - 12, "sockets in their cradles", 10, "middle", fill="#555"))
    b.append(text(vx1 + 8, by(40) + 16, "V's socket face", 10, fill="#555"))
    tail, body0, body1 = 422.0, 434.0, 478.0
    for ref in ("J_LEFT1", "J_LEFT2", "J_LEFT3"):
        pins = fp[ref]
        mid = by(pins["2"][1])
        b.append(f'<rect x="{body0}" y="{mid - 3.94 * s:.1f}" width="{body1 - body0}" height="{7.87 * s:.1f}" rx="2" fill="#111"/>')
        b.append(text(vx1 + 8, mid - 40, f"for {ref}", 11, weight="bold"))
        for n, (x, y, _) in pins.items():
            yy, ly = by(y), mid + (int(n) - 2) * 20
            name, color = sig[(ref, n)]
            b.append(f'<rect x="{bx(x) - 4:.1f}" y="{yy - 4:.1f}" width="8" height="8" fill="#c9a227"/>')
            b.append(f'<rect x="{tail}" y="{yy - 1.5}" width="{body0 - tail}" height="3" fill="#c9a227"/>')
            b.append(f'<rect x="{body1 - 6}" y="{yy - 2}" width="4" height="4" fill="#666"/>')
            b.append(line([(bx(x), yy), (tail, yy)], color, 3))
            b.append(line([(body1, yy), (vx1 + 30, ly), (vx1 + 40, ly)], color, 1.2))
            b.append(text(vx1 + 46, ly + 4, f"pin {n}  {name}", 11, weight="bold", fill=color))
            if (ref, n) in trackpad:
                b.append(f'<circle cx="{bx(x):.1f}" cy="{yy:.1f}" r="7.5" fill="none" stroke="#fff" stroke-width="1.5"/>')
    b.append(text(bx(70.55) + 10, by(122), "ringed: pads that already", 10, fill="#555"))
    b.append(text(bx(70.55) + 10, by(122) + 14, "carry a trackpad wire", 10, fill="#555"))
    b.append(text((body0 + body1) / 2, by(55.56) - 16, "pin 1 toward the SW5 end", 10, "middle", fill="#555"))
    notes = ["Glue each socket into its cradle with epoxy: mouth toward V's socket face, pin 1 toward the SW5 end. Keep the glue off the tails.",
             "Wire each tail to the J_LEFT pad with the same connector and pin number: nine wires, each straight across.",
             "Five of those pads already carry a trackpad wire: the socket's wire goes onto the same pad.",
             "Leave slack in every wire: the floor folds over onto the shell when the module closes."]
    b += [text(30, 640 + 18 * i, t, 11) for i, t in enumerate(notes)]
    return svg(980, 720, b)


if __name__ == "__main__":
    (OUT / "pointing-rework.svg").write_text(rework())
    (OUT / "pointing-bench.svg").write_text(bench())
    (OUT / "pointing-u1-marker.svg").write_text(marker())
    (OUT / "pointing-sockets.svg").write_text(sockets())
    print("wrote pointing-rework.svg, pointing-bench.svg, pointing-u1-marker.svg and pointing-sockets.svg in", OUT)

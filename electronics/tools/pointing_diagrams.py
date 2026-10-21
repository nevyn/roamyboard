"""Draw the pointing module's bench diagrams into docs/images/.

    python3 pointing_diagrams.py

pointing-rework.svg: the MCU module rework, on the socket board's back with the nice!nano underside up, laid out
like bench-wiring.svg. Pad positions come from KeyModule.kicad_pcb.
pointing-bench.svg: the IQS572's pins for ringing out the TPS48's pads, and the jumpers into the MCU module's sockets.
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
    b = [text(30, 36, "Find the TPS48's pads", 16, weight="bold"),
         text(30, 54, "Ring each solder pad on the module's back out against the IQS572's pins (continuity beep).", 11),
         text(30, 69, "The datasheet draws only the GPIO variant (silkscreen IO1..IO5, GND, 3V3); the I2C mapping is not published.", 11)]
    # IQS572 QFN28, top view: pins 1-7 down the left, 8-14 along the bottom, 15-21 up the right, 22-28 along the top
    cx, cy, half, step = 200.0, 260.0, 105.0, 26.0
    b.append(f'<rect x="{cx - half}" y="{cy - half}" width="{2 * half}" height="{2 * half}" rx="8" fill="#2b2b2b" stroke="#000"/>')
    b.append(f'<circle cx="{cx - half + 18}" cy="{cy - half + 18}" r="6" fill="#777"/>')
    b.append(text(cx, cy - 6, "IQS572", 16, "middle", "bold", "#eee"))
    b.append(text(cx, cy + 14, "QFN28, top view", 11, "middle", fill="#ccc"))
    b.append(text(cx, cy + 30, "dot = pin 1", 10, "middle", fill="#ccc"))
    sig = {2: ("SDA", SDA), 3: ("SCL", SCL), 4: ("VDDHI", VCC), 5: ("VSS", GND), 7: ("NRST", "#7f7f7f"), 8: ("RDY", INT)}
    for p in range(1, 29):
        side, i = (p - 1) // 7, (p - 1) % 7
        off = (i - 3) * step
        x, y, ax, ay, anchor = {0: (cx - half - 8, cy + off, -1, 0, "end"), 1: (cx + off, cy + half + 8, 0, 1, "middle"),
                                2: (cx + half + 8, cy - off, 1, 0, "start"), 3: (cx - off, cy - half - 8, 0, -1, "middle")}[side]
        name, color = sig.get(p, ("", "#999"))
        w, h = (14, 7) if side in (0, 2) else (7, 14)
        b.append(f'<rect x="{x - w / 2 + ax * 4:.1f}" y="{y - h / 2 + ay * 4:.1f}" width="{w}" height="{h}" fill="{color if name else "#c9a227"}"/>')
        tx, ty = x + ax * 18, y + ay * 22 + 4
        label = f"{p} {name}" if name and side == 0 else (f"{name} {p}" if name else str(p))
        b.append(text(tx, ty, label, 11 if name else 9, anchor, "bold" if name else "normal", color if name else "#777"))
    b.append(text(cx, cy + half + 62, "Pins from the IQS5xx-B000 datasheet, p. 11.", 10, "middle", fill="#555"))

    # MCU module socket face: J_LEFT1 at the SW5 end, J_LEFT3 near the SW1 end; jumper per signal
    b += [text(600, 120, "Bench: jumpers into the MCU module's sockets", 14, "middle", "bold"),
          text(600, 136, "socket face of the rewired MCU module, no key modules attached", 10, "middle", fill="#555")]
    fx0, fy0, s = 470.0, 160.0, 2.6
    fy = lambda y: fy0 + (y - 40) * s
    b.append(f'<rect x="{fx0}" y="{fy(40)}" width="40" height="{100 * s}" rx="4" fill="#ddd" stroke="#888"/>')
    b.append(text(fx0 + 20, fy(40) - 6, "SW5 end (USB-C)", 9, "middle", fill="#555"))
    b.append(text(fx0 + 20, fy(140) + 14, "SW1 end", 9, "middle", fill="#555"))
    fp = footprints()
    jump = {("J_LEFT3", "1"): ("INT", "RDY", INT), ("J_LEFT1", "2"): ("+3.3V", "VDDHI", VCC), ("J_LEFT1", "3"): ("GND", "VSS", GND),
            ("J_LEFT3", "2"): ("SDA", "SDA", SDA), ("J_LEFT3", "3"): ("SCL", "SCL", SCL)}
    for ref in ("J_LEFT1", "J_LEFT2", "J_LEFT3"):
        mid = fy(fp[ref]["2"][1])
        if ref == "J_LEFT2":
            b.append(text(fx0 + 50, mid + 4, "J_LEFT2: leave empty", 10, fill="#999"))
        for n, (_, y, net) in fp[ref].items():
            yy, ly = fy(y), mid + (int(n) - 2) * 24
            b.append(f'<rect x="{fx0 + 14}" y="{yy - 3}" width="12" height="6" fill="#111"/>')
            j = jump.get((ref, n))
            if j:
                b.append(line([(fx0 + 26, yy), (560, yy), (590, ly), (640, ly)], j[2], 3))
                b.append(text(646, ly + 4, f"pin {n} {j[0]}  to the pad's {j[1]}", 11, weight="bold", fill=j[2]))
            elif ref == "J_LEFT1":
                b.append(text(646, ly + 4, f"pin {n} (+5V, later LED): leave empty", 10, fill="#999"))
        if ref != "J_LEFT2":
            b.append(text(560, mid - 44, ref, 11, "middle", "bold"))
    notes = ["Solder short stranded wires to the pad's VDDHI, VSS, SDA, SCL and RDY pads, with male Dupont pins at the far end.",
             "Leave NRST unconnected: the IQS572 pulls it up itself.",
             "Switch the half off before you plug the jumpers in. With the pad in the sockets, this half reads 0 key modules.",
             "Tape a 1 to 2 mm plastic overlay on the pad's face, without air gaps, and leave solder link S1 open."]
    b += [text(30, 470 + 18 * i, t, 11) for i, t in enumerate(notes)]
    return svg(980, 560, b)


if __name__ == "__main__":
    (OUT / "pointing-rework.svg").write_text(rework())
    (OUT / "pointing-bench.svg").write_text(bench())
    print("wrote", OUT / "pointing-rework.svg", "and", OUT / "pointing-bench.svg")

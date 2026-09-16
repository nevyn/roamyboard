"""Kailh Choc v1 (PG1350) hotswap footprint for a switch inserted from the front.

Hole pattern from the Kailh PG1350 drawing, in the orientation this board uses
(the drawing's pattern rotated 180 degrees): contact holes at (0, -5.9) and
(5, -3.8). The socket sits on the back; pad 1 is the outboard pad on the +x
side so that it can carry the +3.3V rail along the board edge.
"""
import uuid
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "KeyModule" / "Library.pretty"
NAME = "Kailh_choc_v1_hotswap"
MODELS = "${KIPRJMOD}/Library.3dshapes/"

HOLE_A = (0.0, -5.9)       # contact hole on the centre line
HOLE_B = (5.0, -3.8)       # contact hole toward +x
PAD_OFFSET = 3.275         # socket pad centre from its hole centre
PAD = 2.6
BOSS = 3.429
LEG = 1.9                  # peg is 1.8; the drawing asks for 1.9
BODY = 13.8                # housing at board level; 15.0 at the rim

def uid():
    return str(uuid.uuid4())

def line(a, b, layer, w):
    return (f"\t(fp_line (start {a[0]:.3f} {a[1]:.3f}) (end {b[0]:.3f} {b[1]:.3f})"
            f" (stroke (width {w}) (type solid)) (layer \"{layer}\") (uuid \"{uid()}\"))")

def rect(x0, y0, x1, y1, layer, w):
    return "\n".join([line((x0, y0), (x1, y0), layer, w), line((x1, y0), (x1, y1), layer, w),
                      line((x1, y1), (x0, y1), layer, w), line((x0, y1), (x0, y0), layer, w)])

def circle(c, r, layer, w):
    return (f"\t(fp_circle (center {c[0]:.3f} {c[1]:.3f}) (end {c[0] + r:.3f} {c[1]:.3f})"
            f" (stroke (width {w}) (type solid)) (fill no) (layer \"{layer}\") (uuid \"{uid()}\"))")

def poly(pts, layer, w):
    xy = " ".join(f"(xy {x:.3f} {y:.3f})" for x, y in pts)
    return (f"\t(fp_poly (pts {xy}) (stroke (width {w}) (type solid)) (fill no)"
            f" (layer \"{layer}\") (uuid \"{uid()}\"))")

def prop(name, value, y, layer, hide=False):
    hide_s = "\n\t\t(hide yes)" if hide else ""
    return (f"\t(property \"{name}\" \"{value}\"\n\t\t(at 0 {y:.2f} 0)\n\t\t(unlocked yes)\n"
            f"\t\t(layer \"{layer}\"){hide_s}\n\t\t(uuid \"{uid()}\")\n"
            f"\t\t(effects (font (size 1 1) (thickness 0.15)))\n\t)")

def npth(c, d):
    return (f"\t(pad \"\" np_thru_hole circle (at {c[0]:.3f} {c[1]:.3f}) (size {d} {d}) (drill {d})"
            f" (layers \"*.Cu\" \"*.Mask\") (uuid \"{uid()}\"))")

def smd(n, c):
    return (f"\t(pad \"{n}\" smd rect (at {c[0]:.3f} {c[1]:.3f}) (size {PAD} {PAD})"
            f" (layers \"B.Cu\" \"B.Mask\" \"B.Paste\") (uuid \"{uid()}\"))")

def model(file, rot_z, off_z):
    return (f"\t(model \"{MODELS}{file}\"\n\t\t(offset (xyz 0 0 {off_z}))\n\t\t(scale (xyz 1 1 1))\n"
            f"\t\t(rotate (xyz 0 0 {rot_z}))\n\t)")

pad1 = (HOLE_B[0] + PAD_OFFSET, HOLE_B[1])
pad2 = (HOLE_A[0] - PAD_OFFSET, HOLE_A[1])
# Socket body on the back, plus its two solder tabs under the pads
sock_x0, sock_x1, sock_y0, sock_y1 = -2.0, 7.0, -8.2, -1.5
cy_x0, cy_x1 = pad2[0] - PAD / 2 - 0.25, pad1[0] + PAD / 2 + 0.25
cy_y0, cy_y1 = sock_y0 - 0.25, sock_y1 + 0.25
hb = BODY / 2

parts = [
    f"(footprint \"{NAME}\"",
    "\t(version 20260206)",
    "\t(generator \"choc_footprint.py\")",
    "\t(generator_version \"10.0\")",
    "\t(layer \"F.Cu\")",
    "\t(descr \"Kailh Choc v1 PG1350 keyswitch on the front, Kailh hotswap socket on the back; pad 1 is the outboard pad on the +x side\")",
    "\t(tags \"kailh choc pg1350 hotswap\")",
    prop("Reference", "REF**", 8.8, "F.SilkS"),
    prop("Value", NAME, -7.8, "F.Fab"),
    prop("Datasheet", "", 0, "F.Fab", hide=True),
    prop("Description", "", 0, "F.Fab", hide=True),
    "\t(attr smd)",
    # switch housing, front
    rect(-hb, -hb, hb, hb, "F.Fab", 0.1),
    rect(-7.0, -7.0, 7.0, 7.0, "F.CrtYd", 0.05),
    # socket, back: fab outline and a courtyard merged with the centre boss
    rect(sock_x0, sock_y0, sock_x1, sock_y1, "B.Fab", 0.1),
    rect(-1.7, sock_y0 + 0.2, 6.8, sock_y1 - 0.5, "B.SilkS", 0.12),
    poly([(cy_x0, cy_y0), (cy_x1, cy_y0), (cy_x1, cy_y1), (BOSS / 2 + 0.25, cy_y1),
          (BOSS / 2 + 0.25, BOSS / 2 + 0.25), (-BOSS / 2 - 0.25, BOSS / 2 + 0.25),
          (-BOSS / 2 - 0.25, cy_y1), (cy_x0, cy_y1)], "B.CrtYd", 0.05),
    circle((-5.5, 0), LEG / 2 + 0.25, "B.CrtYd", 0.05),
    circle((5.5, 0), LEG / 2 + 0.25, "B.CrtYd", 0.05),
    npth((0, 0), BOSS),
    npth((-5.5, 0), LEG),
    npth((5.5, 0), LEG),
    npth(HOLE_A, 3.0),
    npth(HOLE_B, 3.0),
    smd(1, pad1),
    smd(2, pad2),
    model("Kailh_choc_v1.step", 0, 0),
    model("Kailh_choc_v1_hotswap_socket.step", 180, -3.4),
    ")",
]
OUT.joinpath(NAME + ".kicad_mod").write_text("\n".join(parts) + "\n")
print(NAME, "pad1", pad1, "pad2", pad2, "B.CrtYd x", cy_x0, cy_x1, "y", cy_y0, cy_y1)

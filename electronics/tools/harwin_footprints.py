#!/usr/bin/env python3
"""Generate KiCad footprints for the Harwin M20 horizontal SMT module interconnect.

Dimensions come from Harwin drawings DRG-02613 (M20-791xx42R socket) and
DRG-02615 (M20-889xx45R pin header). Footprint origin is the center of the pad
row; the connector body and its mating side extend toward negative Y, so the
part overhangs the board edge when placed with the pads just inside it.

Usage: python3 harwin_footprints.py  (writes into ../KeyModule/Library.pretty)
"""
import uuid
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "KeyModule" / "Library.pretty"
PITCH = 2.54
COURTYARD = 0.25
SILK_W = 0.12
FAB_W = 0.10


def uid():
    return str(uuid.uuid4())


def line(a, b, layer, w):
    return (f"\t(fp_line (start {a[0]:.3f} {a[1]:.3f}) (end {b[0]:.3f} {b[1]:.3f})"
            f" (stroke (width {w}) (type solid)) (layer \"{layer}\") (uuid \"{uid()}\"))")


def rect(x0, y0, x1, y1, layer, w):
    return "\n".join([
        line((x0, y0), (x1, y0), layer, w),
        line((x1, y0), (x1, y1), layer, w),
        line((x1, y1), (x0, y1), layer, w),
        line((x0, y1), (x0, y0), layer, w),
    ])


def prop(name, value, y, layer, hide=False):
    hide_s = "\n\t\t(hide yes)" if hide else ""
    return (f"\t(property \"{name}\" \"{value}\"\n\t\t(at 0 {y:.2f} 0)\n\t\t(unlocked yes)\n"
            f"\t\t(layer \"{layer}\"){hide_s}\n\t\t(uuid \"{uid()}\")\n"
            f"\t\t(effects (font (size 0.5 0.5) (thickness 0.1)))\n\t)")


def pad(n, x, w, h):
    return (f"\t(pad \"{n}\" smd roundrect (at {x:.3f} 0) (size {w} {h})"
            f" (layers \"F.Cu\" \"F.Mask\" \"F.Paste\") (roundrect_rratio 0.15) (uuid \"{uid()}\"))")


def footprint(name, descr, datasheet, n, pad_h, body_w, body_d, pad_center_from_body, extra_fab, extra_depth, pin1_at_plus_x=False):
    """extra_fab: list of fab lines drawn beyond the body; extra_depth: how far they reach past it.

    Sockets sit on the left board edge and headers on the right, both on the back, so their
    pin rows run in opposite directions; the header numbers its pads from +x so that pin 1
    of both lands at the same end of the module."""
    xs = [(i - (n - 1) / 2) * PITCH for i in range(n)]
    body_y1 = -pad_center_from_body            # body face on the tail side
    body_y0 = body_y1 - body_d
    half_w = body_w / 2
    cy0 = body_y0 - extra_depth - COURTYARD
    cy1 = pad_h / 2 + COURTYARD
    cx = max(half_w, xs[-1] + 0.51) + COURTYARD
    silk_y = pad_h / 2 + 0.3                   # silk stays clear of the pads
    pin1_x = xs[-1] if pin1_at_plus_x else xs[0]
    parts = [
        f"(footprint \"{name}\"",
        "\t(version 20260206)", "\t(generator \"harwin_footprints.py\")", "\t(generator_version \"10.0\")",
        "\t(layer \"F.Cu\")",
        f"\t(descr \"{descr}\")", "\t(tags \"harwin m20 horizontal smd connector\")",
        prop("Reference", "REF**", cy1 + 0.6, "F.SilkS"),
        prop("Value", name, cy0 - 0.6, "F.Fab"),
        prop("Datasheet", datasheet, 0, "F.Fab", hide=True),
        prop("Description", descr, 0, "F.Fab", hide=True),
        "\t(attr smd)",
        rect(-cx, cy0, cx, cy1, "F.CrtYd", 0.05),
        rect(-half_w, body_y0, half_w, body_y1, "F.Fab", FAB_W),
        *extra_fab,
        # Silk: body sides and the mating face, plus a pin-1 tick outside pad 1.
        line((-half_w, body_y0), (-half_w, body_y1), "F.SilkS", SILK_W),
        line((half_w, body_y0), (half_w, body_y1), "F.SilkS", SILK_W),
        line((-half_w, body_y0), (half_w, body_y0), "F.SilkS", SILK_W),
        line((pin1_x - 0.6, silk_y), (pin1_x - 0.6, silk_y + 0.8), "F.SilkS", SILK_W),
        *[pad(n - i if pin1_at_plus_x else i + 1, x, 1.02, pad_h) for i, x in enumerate(xs)],
        f"\t(fp_text user \"${{REFERENCE}}\" (at 0 {(body_y0 + body_y1) / 2:.2f} 0) (layer \"F.Fab\")"
        f" (uuid \"{uid()}\") (effects (font (size 0.5 0.5) (thickness 0.1))))",
        ")",
    ]
    (OUT / f"{name}.kicad_mod").write_text("\n".join(parts) + "\n")
    print("wrote", OUT / f"{name}.kicad_mod")


def socket(n):
    # DRG-02613: body (2.54*n + 0.25) wide, 8.50 deep, 2.50 tall; tail 3.40 past body face;
    # pads 1.02 x 2.00. Pad center sits 2.65 from the body so the toe clears the tail by 0.25.
    body_w = PITCH * n + 0.25
    footprint(
        name=f"Harwin_M20-791_1x{n:02d}_Horizontal_SMD",
        descr=f"Harwin M20-791{n:02d}42R, 1x{n} 2.54mm SIL socket, horizontal SMT, mouth overhangs board edge",
        datasheet="https://content.harwin.com/m/0e0398fdb977d498/original/DRG-02613-Technical-Drawing-Datasheet-M20-791R-pdf.pdf",
        n=n, pad_h=2.00, body_w=body_w, body_d=8.50, pad_center_from_body=2.65,
        extra_fab=[], extra_depth=0,
    )


def header(n):
    # DRG-02615: body 2.54*n wide, 2.50 deep, 2.50 tall; pins 0.64 sq, 6.00 past the body;
    # tail 3.00 past body face; pads 1.02 x 3.17. Pad center 1.60 from the body.
    body_w = PITCH * n
    body_y0 = -1.60 - 2.50
    xs = [(i - (n - 1) / 2) * PITCH for i in range(n)]
    pins = []
    for x in xs:
        pins.append(rect(x - 0.32, body_y0 - 6.00, x + 0.32, body_y0, "F.Fab", FAB_W))
    footprint(
        name=f"Harwin_M20-889_1x{n:02d}_Horizontal_SMD",
        descr=f"Harwin M20-889{n:02d}45R, 1x{n} 2.54mm SIL pin header, horizontal SMT, pins overhang board edge",
        datasheet="https://content.harwin.com/asset/bcd8efee-7ad9-4ddd-8fcc-2925970fdfe6/DRG-02615-Technical-Drawing-Datasheet-M20-889-pdf.pdf",
        n=n, pad_h=3.17, body_w=body_w, body_d=2.50, pad_center_from_body=1.60,
        extra_fab=pins, extra_depth=6.00, pin1_at_plus_x=True,
    )


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    socket(3)
    header(3)

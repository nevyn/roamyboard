#!/usr/bin/env python3
"""Build STEP models for the Harwin M20 module connectors with CadQuery.

Dimensions are the same Harwin drawings the footprints come from (DRG-02613 socket,
DRG-02615 header). Model frame is KiCad's: origin at the footprint origin (pad row
center), X along the pin row, Y toward the connector body (KiCad's 3D Y points up
the footprint, i.e. footprint -Y), Z up from the board surface.

Run with a Python that has cadquery, e.g. ~/cq-editor/bin/python harwin_models.py
"""
import cadquery as cq
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "KeyModule" / "Library.3dshapes"
PITCH = 2.54
PIN = 0.64


def socket(n):
    body_w, body_d, body_h = PITCH * n + 0.25, 8.50, 2.50
    pad_to_body = 2.65
    body = (cq.Workplane("XY").box(body_w, body_d, body_h, centered=(True, False, False))
            .translate((0, pad_to_body, 0)))
    for i in range(n):
        x = (i - (n - 1) / 2) * PITCH
        body = body.cut(cq.Workplane("XY").box(0.9, 6.6, 0.9).translate((x, pad_to_body + body_d - 3.3, body_h / 2)))
        tail = (cq.Workplane("XY").box(PIN, 3.4 + 1.0, 0.3, centered=(True, False, False))
                .translate((x, pad_to_body - 3.4, 0)))
        body = body.union(tail)
    return body


def header(n):
    body_w, body_d, body_h = PITCH * n, 2.50, 2.50
    pad_to_body = 1.60
    body = (cq.Workplane("XY").box(body_w, body_d, body_h, centered=(True, False, False))
            .translate((0, pad_to_body, 0)))
    for i in range(n):
        x = (i - (n - 1) / 2) * PITCH
        pin = (cq.Workplane("XY").box(PIN, 6.0 + body_d + 3.0, PIN, centered=(True, False, True))
               .translate((x, pad_to_body - 3.0, body_h / 2)))
        foot = cq.Workplane("XY").box(PIN, 1.0, body_h / 2 + PIN / 2, centered=(True, False, False)).translate((x, pad_to_body - 3.0, 0))
        body = body.union(pin).union(foot)
    return body


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    for name, shape in ((f"Harwin_M20-791_1x03", socket(3)), (f"Harwin_M20-889_1x03", header(3))):
        cq.exporters.export(shape, str(OUT / f"{name}.step"))
        bb = shape.val().BoundingBox()
        print(f"{name}: x {bb.xmin:.2f}..{bb.xmax:.2f} y {bb.ymin:.2f}..{bb.ymax:.2f} z {bb.zmin:.2f}..{bb.zmax:.2f}")

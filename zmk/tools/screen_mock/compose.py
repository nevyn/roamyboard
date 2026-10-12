#!/usr/bin/env python3
"""Composes the PGM scenes that mock.c writes into the PNGs that run.sh lists."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from cat_frames import write_png  # noqa: E402

SCENES = [
    "central-startup",
    "central-startup-unknown",
    "central-7cols-walk",
    "central-1col-sit",
    "central-0cols-sleep",
    "central-noterm-blink",
    "central-unknown-walk",
    "central-open-profile",
    "central-empty-name",
    "central-long-name",
    "central-bootloader",
    "central-ota",
    "peripheral-startup",
    "peripheral-7cols",
    "peripheral-no-central",
    "peripheral-noterm",
    "peripheral-bootloader",
    "peripheral-ota",
]


def read_pgm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(maxsplit=4)
    if parts[0] != b"P5":
        raise ValueError(f"{path} is not a binary PGM")
    width, height = int(parts[1]), int(parts[2])
    return width, height, parts[4][: width * height]


def compose(images, scale, gap, path, vertical=False):
    """Places images side by side (or one per row) on a grey background, each pixel scale x scale."""
    if vertical:
        width = max(w for w, _, _ in images) + 2 * gap
        height = sum(h for _, h, _ in images) + gap * (len(images) + 1)
    else:
        width = sum(w for w, _, _ in images) + gap * (len(images) + 1)
        height = max(h for _, h, _ in images) + 2 * gap
    out_w, out_h = width * scale, height * scale
    pixels = bytearray([0x80]) * (out_w * out_h)
    offset = gap
    for w, h, data in images:
        ox, oy = (gap, offset) if vertical else (offset, gap)
        for y in range(h):
            for x in range(w):
                value = data[y * w + x]
                for sy in range(scale):
                    start = ((oy + y) * scale + sy) * out_w + (ox + x) * scale
                    pixels[start:start + scale] = bytes([value]) * scale
        offset += (h if vertical else w) + gap
    write_png(path, pixels, out_w, out_h)


# Rows of the mounted view that a contact sheet cell shows: the count box, and the host
# name's place, where mock.c writes the action and the time.
CELL_ROWS = list(range(18, 65)) + list(range(84, 130))


def leg_cell(image):
    w, _, data = image
    return w, len(CELL_ROWS), b"".join(data[y * w:(y + 1) * w] for y in CELL_ROWS)


def panel_cell(cell):
    """Turns a cell of the mounted view into the panel's own orientation."""
    w, h, data = cell
    return h, w, bytes(data[c * w + (w - 1 - r)] for r in range(w) for c in range(h))


def compose_grid(rows, scale, gap, path):
    """Places rows of images on a grey background, one row per list, each pixel scale x scale."""
    cell_w = max(w for row in rows for w, _, _ in row)
    width = max(len(row) for row in rows) * (cell_w + gap) + gap
    height = sum(max(h for _, h, _ in row) + gap for row in rows) + gap
    out_w, out_h = width * scale, height * scale
    pixels = bytearray([0x80]) * (out_w * out_h)
    oy = gap
    for row in rows:
        for i, (w, h, data) in enumerate(row):
            ox = gap + i * (cell_w + gap)
            for y in range(h):
                for x in range(w):
                    value = data[y * w + x]
                    for sy in range(scale):
                        start = ((oy + y) * scale + sy) * out_w + (ox + x) * scale
                        pixels[start:start + scale] = bytes([value]) * scale
        oy += max(h for _, h, _ in row) + gap
    write_png(path, pixels, out_w, out_h)


def compose_cat_actions(out):
    """Writes cat-actions-on-leg.png and cat-actions.png from the rows in cat-actions.txt."""
    with open(os.path.join(out, "cat-actions.txt")) as f:
        rows = [line.rstrip("\n").split("\t")[1].split() for line in f if line.strip()]
    leg = [[leg_cell(read_pgm(os.path.join(out, f"{n}-leg.pgm"))) for n in row] for row in rows]
    compose_grid(leg, 2, 4, os.path.join(out, "cat-actions-on-leg.png"))
    compose_grid([[panel_cell(c) for c in row] for row in leg], 2, 4,
                 os.path.join(out, "cat-actions.png"))
    print(f"wrote cat-actions.png, cat-actions-on-leg.png to {out}")


def main():
    out = sys.argv[1]
    panel = [read_pgm(os.path.join(out, f"{name}-panel.pgm")) for name in SCENES]
    leg = [read_pgm(os.path.join(out, f"{name}-leg.pgm")) for name in SCENES]
    compose(leg[:1], 4, 4, os.path.join(out, "screen.png"))
    compose(panel[:1], 4, 4, os.path.join(out, "screen-panel.png"))
    compose(panel, 3, 4, os.path.join(out, "states.png"), vertical=True)
    compose(leg, 3, 4, os.path.join(out, "states-on-leg.png"))
    print(f"wrote screen.png, screen-panel.png, states.png, states-on-leg.png to {out}")
    compose_cat_actions(out)


if __name__ == "__main__":
    main()

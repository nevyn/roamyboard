#!/usr/bin/env python3
"""Composes the PGM scenes that mock.c writes into screen.png and states.png."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from cat_frames import write_png  # noqa: E402

SCENES = [
    "central-7cols-walk",
    "central-1col-sit",
    "central-0cols-sleep",
    "central-noterm-blink",
    "central-unknown-walk",
    "central-bootloader",
    "peripheral-7cols-walk",
    "peripheral-noterm-sit",
    "peripheral-bootloader",
]


def read_pgm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(maxsplit=4)
    if parts[0] != b"P5":
        raise ValueError(f"{path} is not a binary PGM")
    width, height = int(parts[1]), int(parts[2])
    return width, height, parts[4][: width * height]


def compose(images, scale, gap, path):
    """Places images side by side on a grey background, each pixel scale x scale."""
    height = max(h for _, h, _ in images)
    width = sum(w for w, _, _ in images) + gap * (len(images) + 1)
    out_w, out_h = width * scale, (height + 2 * gap) * scale
    pixels = bytearray([0x80]) * (out_w * out_h)
    ox = gap
    for w, h, data in images:
        for y in range(h):
            for x in range(w):
                value = data[y * w + x]
                for sy in range(scale):
                    start = ((gap + y) * scale + sy) * out_w + (ox + x) * scale
                    pixels[start:start + scale] = bytes([value]) * scale
        ox += w + gap
    write_png(path, pixels, out_w, out_h)


def main():
    out = sys.argv[1]
    images = [read_pgm(os.path.join(out, f"{name}.pgm")) for name in SCENES]
    compose(images[:1], 4, 4, os.path.join(out, "screen.png"))
    compose(images, 3, 4, os.path.join(out, "states.png"))
    print(f"wrote {out}/screen.png and {out}/states.png")


if __name__ == "__main__":
    main()

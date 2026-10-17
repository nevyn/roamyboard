#!/usr/bin/env python3
"""Writes the screen mock's PDA scenes as PNGs: the PDA manual's screenshots, and a sheet.

    python3 zmk/tools/screen_mock/pda_screens.py MOCK_DIR [SCREENS_DIR] [--sheet SHEET.png]

MOCK_DIR is run.sh's output directory. Every scene peripheral-pda-NAME becomes
SCREENS_DIR/NAME.png (default docs/pda/screens), 68 x 160 as a person sees the
nice!view on the leg, one byte per pixel; the page scales them up without
smoothing. --sheet also writes all scenes side by side, three times the size.
"""

import glob
import os
import struct
import sys
import zlib

PREFIX = "peripheral-pda-"
SUFFIX = "-leg.pgm"


def read_pgm(path):
    data = open(path, "rb").read()
    parts = data.split(maxsplit=4)
    w, h = int(parts[1]), int(parts[2])
    return w, h, parts[4][: w * h]


def write_png(path, w, h, rows):
    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload)))

    raw = b"".join(b"\0" + bytes(r) for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main(argv):
    sheet = None
    if "--sheet" in argv:
        i = argv.index("--sheet")
        sheet = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    if not argv:
        sys.exit(__doc__)
    mock_dir = argv[0]
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = argv[1] if len(argv) > 1 else os.path.join(here, "..", "..", "..", "docs", "pda", "screens")
    os.makedirs(out_dir, exist_ok=True)

    scenes = []
    for path in sorted(glob.glob(os.path.join(mock_dir, PREFIX + "*" + SUFFIX))):
        name = os.path.basename(path)[len(PREFIX):-len(SUFFIX)]
        w, h, px = read_pgm(path)
        write_png(os.path.join(out_dir, name + ".png"), w, h,
                  [px[y * w:(y + 1) * w] for y in range(h)])
        scenes.append((name, w, h, px))
    if not scenes:
        sys.exit(f"No {PREFIX}*{SUFFIX} scenes in {mock_dir}: run zmk/tools/screen_mock/run.sh first")
    print(f"wrote {len(scenes)} screens to {out_dir}")

    if sheet:
        scale, gap = 3, 8
        w0 = max(w for _, w, _, _ in scenes)
        h0 = max(h for _, _, h, _ in scenes)
        cols = min(8, len(scenes))
        grid_rows = (len(scenes) + cols - 1) // cols
        W = cols * w0 * scale + (cols - 1) * gap
        H = grid_rows * h0 * scale + (grid_rows - 1) * gap
        canvas = bytearray([128]) * (W * H)
        for k, (_, w, h, px) in enumerate(scenes):
            ox = (k % cols) * (w0 * scale + gap)
            oy = (k // cols) * (h0 * scale + gap)
            for y in range(h * scale):
                row = px[(y // scale) * w:(y // scale + 1) * w]
                canvas[(oy + y) * W + ox:(oy + y) * W + ox + w * scale] = bytes(
                    v for v in row for _ in range(scale))
        write_png(sheet, W, H, [canvas[y * W:(y + 1) * W] for y in range(H)])
        print("sheet order: " + ", ".join(n for n, _, _, _ in scenes))


if __name__ == "__main__":
    main(sys.argv[1:])

#!/usr/bin/env python3
"""Generates the status screen's cat frames from the pixel art below.

Writes zmk/src/display/cat_frames.c and cat_frames.h, and renders every frame,
scaled up, to a PNG (default /tmp/roamy-screen/frames.png). Run it after
editing the art:

    python3 zmk/tools/cat_frames.py [frames.png]

'#' is ink (black on the nice!view), '.' is background. Every frame faces
right and has the same size. The C arrays hold each frame rotated the same
way that the status screen rotates its canvases, so that LVGL can draw them
without rotating at run time.
"""

import os
import struct
import sys
import zlib

WALK_BODY = [
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...............######.",
    "..............########",
    "..............####.###",
    "..............####.###",
    "....################..",
    "...################...",
    "...################...",
    "...################...",
    "....##############....",
]

TAIL_UP = [
    "..##..................",
    ".#....................",
    ".#....................",
    ".#....................",
    ".#....................",
    "..#...................",
    "...#..................",
]

TAIL_SWAY = [
    "#.....................",
    "#.....................",
    ".#....................",
    ".#....................",
    ".#....................",
    "..#...................",
    "...#..................",
]

WALK_LEGS = [
    [
        "....#..#......#..#....",
        "...#....#....#....#...",
        "..#......#..#......#..",
        "..##.....##.##.....##.",
    ],
    [
        "....#.#.......#.#.....",
        "....#.#.......#.#.....",
        "....#.#.......#.#.....",
        "....##.##.....##.##...",
    ],
    [
        "....#.#.......#.#.....",
        "...#...#.....#...#....",
        "...#...#.....#...#....",
        "..##..##....##..##....",
    ],
    [
        "....#.#.......#.#.....",
        "....#.#.......#.#.....",
        "....#.#.......#.#.....",
        "....##.##.....##.##...",
    ],
]

SIT = [
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...............######.",
    "..............########",
    "..............####.###",
    "..............####.###",
    "...............######.",
    "..............######..",
    ".............#######..",
    "............########..",
    "...........#########..",
    "..........##########..",
    "..........#####.###...",
    "..........#####.###...",
    "..#############.####..",
]

SLEEP = [
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "...............#...#..",
    ".....#########.##.##..",
    "...###########.#####..",
    "..############.#####..",
    "..############.##..#..",
    "..#################...",
    "...################...",
]

Z_BIG = [
    "................####..",
    "..................#...",
    ".................#....",
    "................####..",
]

Z_SMALL = [
    "............###.......",
    "..............#.......",
    ".............#........",
    "............###.......",
]

def overlay(base, layer, top):
    """Returns base with the ink of layer drawn over it, layer's first row at row top."""
    rows = [list(r) for r in base]
    for dy, line in enumerate(layer):
        for x, c in enumerate(line):
            if c == "#":
                rows[top + dy][x] = "#"
    return ["".join(r) for r in rows]


def close_eyes(frame):
    """Replaces the open eye (two background pixels in a column) with a closed one."""
    rows = [list(r) for r in frame]
    for y in range(len(rows) - 1):
        for x, c in enumerate(rows[y]):
            if c == "." and rows[y + 1][x] == "." and 14 < x < 21 and 3 < y < 7:
                rows[y][x] = "#"
                rows[y + 1][x - 1] = "."
                return ["".join(r) for r in rows]
    raise ValueError("no open eye found")


def walk(i):
    body = WALK_BODY + WALK_LEGS[i]
    return overlay(body, TAIL_UP if i % 2 == 0 else TAIL_SWAY, 1)


FRAMES = [
    ("CAT_WALK_0", walk(0)),
    ("CAT_WALK_1", walk(1)),
    ("CAT_WALK_2", walk(2)),
    ("CAT_WALK_3", walk(3)),
    ("CAT_SIT", SIT),
    ("CAT_BLINK", close_eyes(SIT)),
    ("CAT_SLEEP_0", overlay(SLEEP, Z_SMALL, 4)),
    ("CAT_SLEEP_1", overlay(overlay(SLEEP, Z_SMALL, 4), Z_BIG, 0)),
]

WIDTH = len(FRAMES[0][1][0])
HEIGHT = len(FRAMES[0][1])


def check(name, frame):
    if len(frame) != HEIGHT or any(len(r) != WIDTH for r in frame):
        raise ValueError(f"{name} is not {WIDTH} x {HEIGHT}")
    if any(c not in "#." for r in frame for c in r):
        raise ValueError(f"{name} has characters other than '#' and '.'")


def rotate(frame):
    """Rotates a frame like the status screen's canvases (lv_draw_sw_rotate, 270)."""
    h = len(frame)
    w = len(frame[0])
    # Source pixel (x, y) lands at (h - 1 - y, x).
    return ["".join(frame[h - 1 - rx][ry] for rx in range(h)) for ry in range(w)]


def i1_rows(frame):
    """Packs a frame into LVGL I1 rows, MSB first; bit set = palette index 1 = ink."""
    out = []
    for line in frame:
        row = bytearray((len(line) + 7) // 8)
        for x, c in enumerate(line):
            if c == "#":
                row[x // 8] |= 0x80 >> (x % 8)
        out.append(bytes(row))
    return out


C_HEADER = """/*
 * Generated by zmk/tools/cat_frames.py; edit the art there and run it again.
 *
 * SPDX-License-Identifier: MIT
 */
"""


def write_c(path_c, path_h):
    rotated_w = HEIGHT
    rotated_h = WIDTH
    stride = (rotated_w + 7) // 8

    h = [C_HEADER, "#pragma once\n", "#include <lvgl.h>\n"]
    h.append(f"/** Size of every cat frame as it appears on the status screen, upright. */")
    h.append(f"#define CAT_WIDTH {WIDTH}")
    h.append(f"#define CAT_HEIGHT {HEIGHT}\n")
    h.append("enum cat_frame {")
    for name, _ in FRAMES:
        h.append(f"    {name},")
    h.append("    CAT_FRAME_COUNT,")
    h.append("};\n")
    h.append("/** Every frame, rotated for the status screen: CAT_HEIGHT wide, CAT_WIDTH tall. */")
    h.append("extern const lv_image_dsc_t *const cat_frames[CAT_FRAME_COUNT];")

    c = [C_HEADER, '#include "cat_frames.h"\n']
    c.append("// Palette: index 0 is the background, index 1 the ink; ARGB8888 stored B, G, R, A.")
    c.append("#ifdef CONFIG_NICE_VIEW_WIDGET_INVERTED")
    c.append("#define CAT_PALETTE 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff")
    c.append("#else")
    c.append("#define CAT_PALETTE 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff")
    c.append("#endif\n")
    for name, frame in FRAMES:
        var = name.lower()
        rows = i1_rows(rotate(frame))
        c.append(f"static const uint8_t {var}_map[] = {{")
        c.append("    CAT_PALETTE,")
        for row in rows:
            c.append("    " + " ".join(f"0x{b:02x}," for b in row))
        c.append("};\n")
        c.append(f"static const lv_image_dsc_t {var} = {{")
        c.append("    .header.magic = LV_IMAGE_HEADER_MAGIC,")
        c.append("    .header.cf = LV_COLOR_FORMAT_I1,")
        c.append(f"    .header.w = {rotated_w},")
        c.append(f"    .header.h = {rotated_h},")
        c.append(f"    .header.stride = {stride},")
        c.append(f"    .data_size = sizeof({var}_map),")
        c.append(f"    .data = {var}_map,")
        c.append("};\n")
    c.append("const lv_image_dsc_t *const cat_frames[CAT_FRAME_COUNT] = {")
    for name, _ in FRAMES:
        c.append(f"    [{name}] = &{name.lower()},")
    c.append("};")

    with open(path_h, "w") as f:
        f.write("\n".join(h) + "\n")
    with open(path_c, "w") as f:
        f.write("\n".join(c) + "\n")


def write_png(path, pixels, width, height):
    """Writes 8-bit grayscale rows (bytes, one per pixel) as a PNG."""
    raw = b"".join(b"\x00" + bytes(pixels[y * width:(y + 1) * width]) for y in range(height))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def render_frames(path, scale=8, gap=2):
    """Draws every frame side by side, upright, each pixel as a scale x scale block."""
    cell_w = WIDTH + gap
    width = (len(FRAMES) * cell_w + gap) * scale
    height = (HEIGHT + 2 * gap) * scale
    pixels = bytearray([0xC0]) * (width * height)
    for i, (_, frame) in enumerate(FRAMES):
        ox = gap + i * cell_w
        for y in range(-1, HEIGHT + 1):
            for x in range(-1, WIDTH + 1):
                inside = 0 <= x < WIDTH and 0 <= y < HEIGHT
                value = (0x00 if frame[y][x] == "#" else 0xFF) if inside else 0xA0
                for sy in range(scale):
                    row = ((gap + y) * scale + sy) * width
                    start = row + (ox + x) * scale
                    pixels[start:start + scale] = bytes([value]) * scale
    write_png(path, pixels, width, height)


def main():
    for name, frame in FRAMES:
        check(name, frame)
    here = os.path.dirname(os.path.abspath(__file__))
    display = os.path.join(here, "..", "src", "display")
    write_c(os.path.join(display, "cat_frames.c"), os.path.join(display, "cat_frames.h"))
    png = sys.argv[1] if len(sys.argv) > 1 else "/tmp/roamy-screen/frames.png"
    os.makedirs(os.path.dirname(png), exist_ok=True)
    render_frames(png)
    print(f"wrote {os.path.normpath(display)}/cat_frames.[ch] and {png}")


if __name__ == "__main__":
    main()

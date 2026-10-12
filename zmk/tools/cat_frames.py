#!/usr/bin/env python3
"""Generates the status screen's cat frames from the pixel art below.

Writes zmk/src/display/cat_frames.c, cat_frames.h and cat_frame_images.h, and
renders every frame, scaled up, to a PNG (default /tmp/roamy-screen/frames.png):
the right-facing frames on the first row, their mirror images on the second.
Run it after editing the art:

    python3 zmk/tools/cat_frames.py [frames.png]

'#' is ink (black on the nice!view), '.' is background, which the screen draws
transparent. Every frame faces right, stands on the bottom row and has the same
size; the generator adds a left-facing mirror image of each. The C arrays hold
each frame rotated the same way that the status screen rotates its canvases,
so that LVGL can draw them without rotating at run time.
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

# Head and body two rows lower than walking, stretched, with the tail streaming back.
RUN_BODY = [
    "......................",
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...............######.",
    "..............########",
    "..............####.###",
    "..............####.###",
    "..################....",
    ".#################....",
    "..################....",
    "...#############......",
]

TAIL_STREAM = [
    "##....................",
    "..#...................",
]

TAIL_STREAM_LOW = [
    "......................",
    "###...................",
]

RUN_LEGS = [
    [
        "..##.........##.......",
        ".##...........##......",
        "##.............##.....",
        "#...............##....",
    ],
    [
        "...##........##.......",
        "...#.#.......#.#......",
        "..#...#.....#...#.....",
        ".##...##...##...##....",
    ],
    [
        ".....##.....##........",
        "......##...##.........",
        ".......##.##..........",
        "........#.#...........",
    ],
    [
        "...##........##.......",
        "....#.#......#.#......",
        "....#..#....#..#......",
        "...##..##..##..##.....",
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

# Sitting tall and facing the reader, ears up.
SIT_UP = [
    ".............#......#.",
    ".............##....##.",
    ".............########.",
    ".............########.",
    ".............#.####.#.",
    ".............#.####.#.",
    ".............########.",
    "..............######..",
    "...............####...",
    "..............######..",
    ".............########.",
    ".............########.",
    ".............########.",
    "......#......########.",
    ".....#.......##.##.##.",
    "......###############.",
]

# Flat on the floor, chin down.
CROUCH = [
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...............######.",
    "..............####.###",
    "#.####################",
    ".#####################",
]

JUMP_LEGS = [
    "...##...........##....",
    "..##............#.#...",
    ".##...................",
    "......................",
]

FALL_LEGS = [
    "....#.#.......#.#.....",
    "...#..#.......#..#....",
    "..#...#.......#...#...",
    "..#...#.......#...#...",
]

# Stretched out in the air, front paws reaching forward.
POUNCE = [
    "......................",
    "......................",
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...............######.",
    "..............########",
    "..............####.###",
    "##............####.###",
    "..################....",
    "..###################.",
    "...###############..##",
    "..##..................",
    ".##...................",
    "##....................",
    "......................",
]

# Rump up, chest and chin low, tail straight back: the hunting crouch.
HUNT = [
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "...............#....#.",
    "...####........##..##.",
    "..######.......######.",
    "#.########....########",
    ".##########...####.###",
    "..################.###",
    "...#################..",
    "...#.#.........##.##..",
    "..##.##........##.##..",
]

# The hunting crouch, head raised toward something above.
HUNT_UP = [
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...####........######.",
    "..######......####.###",
    "#.########....####.###",
    ".##########...######..",
    ".##################...",
    "..################....",
    "...###############....",
    "...#.#.........##.##..",
    "..##.##........##.##..",
]

STALK_LEGS = [
    [
        "...#.#........##.#....",
        "..##.##.......#..##...",
    ],
    [
        "....#.#......#.##.....",
        "...##.##....##..##....",
    ],
]

# Front low and rump high, ready to pounce; the wiggle shifts the rump and the tail.
HEAD = [
    ".#....#.",
    ".##..##.",
    ".######.",
    "########",
    "####.###",
    "####.###",
]

READY_RUMP_0 = [
    "......................",
    "......................",
    "......................",
    "......................",
    "...#####..............",
    "..########............",
    "..##########..........",
    "..############........",
    "...###########........",
    "...############.......",
    "....############......",
    "....##.##########.....",
    "....##....######......",
    "....##.......##.......",
    "....##.........##.##..",
    "...##.........##.##...",
]

READY_RUMP_1 = [
    "......................",
    "......................",
    "......................",
    "......................",
    "..#####...............",
    ".########.............",
    ".##########...........",
    ".############.........",
    "..###########.........",
    "..############........",
    "...############.......",
    "...##.###########.....",
    "...##.....######......",
    "...##........##.......",
    "...##..........##.##..",
    "..##..........##.##...",
]

READY_TAIL_0 = [
    "..#.",
    "..#.",
    "..#.",
    "...#",
]

READY_TAIL_1 = [
    "#...",
    ".#..",
    ".#..",
    "..#.",
]

# Paws tucked under, a loaf of cat.
LOAF = [
    "......................",
    "......................",
    "......................",
    "......................",
    "......................",
    "...............#....#.",
    "...............##..##.",
    "...............######.",
    "......################",
    "....##############.###",
    "...###############.###",
    "..#################...",
    "..##################..",
    "..##################..",
    "..##################..",
    "...################...",
]

# Rump up, front legs stretched forward along the floor.
STRETCH = [
    "......................",
    "...#..................",
    "...#..................",
    "...#..................",
    "....#.................",
    "....####..............",
    "...######.............",
    "...#######............",
    "...########...........",
    "...#########..#....#..",
    "....#########.##..##..",
    "....##########.######.",
    "....##..#############.",
    "....##......#####.###.",
    "....##.......####.####",
    "...##........#########",
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
    "####",
    "..#.",
    ".#..",
    "####",
]

Z_SMALL = [
    "###",
    "..#",
    ".#.",
    "###",
]


def overlay(base, layer, top, left=0):
    """Returns base with the ink of layer drawn over it, layer's top left pixel at (left, top)."""
    rows = [list(r) for r in base]
    for dy, line in enumerate(layer):
        for dx, c in enumerate(line):
            if c == "#":
                rows[top + dy][left + dx] = "#"
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


def run(i):
    return overlay(RUN_BODY + RUN_LEGS[i], TAIL_STREAM if i % 2 == 0 else TAIL_STREAM_LOW, 6)


def ready(i):
    rump = READY_RUMP_0 if i == 0 else READY_RUMP_1
    return overlay(overlay(rump, READY_TAIL_0 if i == 0 else READY_TAIL_1, 0), HEAD, 8, 14)


def stalk(i):
    return HUNT[:14] + STALK_LEGS[i]


# Each frame: name, the right-facing art, and glyphs (art, left, top) that are drawn over
# it unmirrored in both facings, so that a z still reads as a z.
FRAMES = [
    ("CAT_WALK_0", walk(0), []),
    ("CAT_WALK_1", walk(1), []),
    ("CAT_WALK_2", walk(2), []),
    ("CAT_WALK_3", walk(3), []),
    ("CAT_RUN_0", run(0), []),
    ("CAT_RUN_1", run(1), []),
    ("CAT_RUN_2", run(2), []),
    ("CAT_RUN_3", run(3), []),
    ("CAT_SIT", SIT, []),
    ("CAT_BLINK", close_eyes(SIT), []),
    ("CAT_SIT_UP", SIT_UP, []),
    ("CAT_CROUCH", CROUCH, []),
    ("CAT_JUMP", overlay(WALK_BODY + JUMP_LEGS, TAIL_UP, 1), []),
    ("CAT_FALL", overlay(WALK_BODY + FALL_LEGS, TAIL_SWAY, 1), []),
    ("CAT_POUNCE", POUNCE, []),
    ("CAT_HUNT", HUNT, []),
    ("CAT_HUNT_UP", HUNT_UP, []),
    ("CAT_STALK_0", stalk(0), []),
    ("CAT_STALK_1", stalk(1), []),
    ("CAT_READY_0", ready(0), []),
    ("CAT_READY_1", ready(1), []),
    ("CAT_LOAF", LOAF, []),
    ("CAT_STRETCH", STRETCH, []),
    ("CAT_SLEEP_0", SLEEP, [(Z_SMALL, 12, 4)]),
    ("CAT_SLEEP_1", SLEEP, [(Z_SMALL, 12, 4), (Z_BIG, 16, 0)]),
]

WIDTH = len(FRAMES[0][1][0])
HEIGHT = len(FRAMES[0][1])


def with_glyphs(art, glyphs, mirrored):
    if mirrored:
        art = [r[::-1] for r in art]
    for glyph, left, top in glyphs:
        if mirrored:
            left = WIDTH - left - len(glyph[0])
        art = overlay(art, glyph, top, left)
    return art


def right_frames():
    return [(name, with_glyphs(art, glyphs, False)) for name, art, glyphs in FRAMES]


def left_frames():
    return [(name + "_LEFT", with_glyphs(art, glyphs, True)) for name, art, glyphs in FRAMES]


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


def write_c(path_c, path_h, path_images_h):
    rotated_w = HEIGHT
    rotated_h = WIDTH
    stride = (rotated_w + 7) // 8
    right = right_frames()
    frames = right + left_frames()

    h = [C_HEADER, "#pragma once\n"]
    h.append("/** Size of every cat frame as it appears on the status screen, upright. */")
    h.append(f"#define CAT_WIDTH {WIDTH}")
    h.append(f"#define CAT_HEIGHT {HEIGHT}\n")
    h.append("/**")
    h.append(" * The cat's frames, facing right. CAT_FACING_LEFT plus a frame is the same frame")
    h.append(" * mirrored, facing left.")
    h.append(" */")
    h.append("enum cat_frame {")
    for name, _ in right:
        h.append(f"    {name},")
    h.append("    CAT_FACING_LEFT,")
    h.append(f"    CAT_FRAME_COUNT = 2 * CAT_FACING_LEFT,")
    h.append("};")

    hi = [C_HEADER, "#pragma once\n", "#include <lvgl.h>\n", '#include "cat_frames.h"\n']
    hi.append("/**")
    hi.append(" * Every frame and its mirror image, indexed by enum cat_frame, rotated for the")
    hi.append(" * status screen: CAT_HEIGHT wide, CAT_WIDTH tall. Index 0 of the palette is")
    hi.append(" * transparent, so that the cat's ink lies over whatever is below it.")
    hi.append(" */")
    hi.append("extern const lv_image_dsc_t *const cat_frames[CAT_FRAME_COUNT];")

    c = [C_HEADER, '#include "cat_frame_images.h"\n']
    c.append("// Palette: index 0 is transparent, index 1 the ink; ARGB8888 stored B, G, R, A.")
    c.append("#ifdef CONFIG_NICE_VIEW_WIDGET_INVERTED")
    c.append("#define CAT_PALETTE 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff")
    c.append("#else")
    c.append("#define CAT_PALETTE 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff")
    c.append("#endif\n")
    for name, frame in frames:
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
    for name, _ in right:
        c.append(f"    [{name}] = &{name.lower()},")
    for name, _ in right:
        c.append(f"    [CAT_FACING_LEFT + {name}] = &{name.lower()}_left,")
    c.append("};")

    for path, lines in ((path_h, h), (path_images_h, hi), (path_c, c)):
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")


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


def render_frames(path, scale=6, gap=2):
    """Draws every frame upright, each pixel as a scale x scale block: one row per facing."""
    rows = [right_frames(), left_frames()]
    count = len(rows[0])
    cell_w = WIDTH + gap
    cell_h = HEIGHT + gap
    width = (count * cell_w + gap) * scale
    height = (len(rows) * cell_h + gap) * scale
    pixels = bytearray([0xC0]) * (width * height)
    for r, frames in enumerate(rows):
        oy = gap + r * cell_h
        for i, (_, frame) in enumerate(frames):
            ox = gap + i * cell_w
            for y in range(-1, HEIGHT + 1):
                for x in range(-1, WIDTH + 1):
                    inside = 0 <= x < WIDTH and 0 <= y < HEIGHT
                    value = (0x00 if frame[y][x] == "#" else 0xFF) if inside else 0xA0
                    for sy in range(scale):
                        row = ((oy + y) * scale + sy) * width
                        start = row + (ox + x) * scale
                        pixels[start:start + scale] = bytes([value]) * scale
    write_png(path, pixels, width, height)


def main():
    for name, frame in right_frames() + left_frames():
        check(name, frame)
    here = os.path.dirname(os.path.abspath(__file__))
    display = os.path.join(here, "..", "src", "display")
    write_c(os.path.join(display, "cat_frames.c"), os.path.join(display, "cat_frames.h"),
            os.path.join(display, "cat_frame_images.h"))
    png = sys.argv[1] if len(sys.argv) > 1 else "/tmp/roamy-screen/frames.png"
    os.makedirs(os.path.dirname(png), exist_ok=True)
    render_frames(png)
    print(f"wrote {os.path.normpath(display)}/cat_frames.[ch], cat_frame_images.h and {png}")


if __name__ == "__main__":
    main()

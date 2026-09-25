"""Soldering jig for the v5 key module (electronics/KeyModule).

The board lies front side down in a pocket, so the passives soldered first on the
front sink into pockets and the back is flush with the jig top. Beyond the board's
edges, open-topped slots hold the three socket mouths and the nine header pins
where they must end up to mate with the neighbouring modules; the connector bodies
follow the pins. header_tilt_deg > 0 holds the header pins at the joint angle, so
the header is soldered tilted instead of having its pins bent afterwards; the pin
foot then only touches its pad at the inboard end and the body floats, see the
numbers this script prints.

Run with ~/cq-editor/bin/python; writes build/roamy_solder_jig_<tilt>deg.stl.
"""
import math
import cadquery as cq

# Board (KeyModule.kicad_pcb frame, mm): outline, connector rows, front passives
board_x0, board_x1, board_y0, board_y1 = 70.55, 87.4, 40.0, 140.0
board_t = 1.6
rows = [58.1, 77.1, 115.1]
socket_x, header_x = 79.7, 83.3                       # pad rows
passives = [(83.6, 60.0, 0), (72.3, 81.0, 90), (76.25, 99.4, 0), (79.6, 99.9, 90), (82.95, 99.4, 0), (76.0, 118.4, 0)]

# Connectors (electronics/tools/harwin_footprints.py, Electronics.md)
socket_w, socket_overhang = 7.87, 2.0
header_w, header_pad_len, header_pin_sq, header_pin_len = 7.62, 3.17, 0.64, 6.0
pin_axis_z = 1.25                                    # pin centreline above the board back
pitch = 2.54

fit = 0.15
pocket_fit = 0.15
passive_pocket = (3.6, 2.4, 1.2)                     # 0805 with fillets: length, width, depth
base_t = 3.0                                         # jig floor under the board
guide_h = 4.5                                        # connector guide walls above the board back

def build(header_tilt_deg):
    # Jig frame, board front down, seen from above: the KiCad front-view pattern (y down) turned 180 degrees,
    # so X = centre - x and Y = centre - y; z up = board back.
    cx, cy = (board_x0 + board_x1) / 2, (board_y0 + board_y1) / 2
    def X(x): return cx - x
    def Y(y): return cy - y
    bw, bl = board_x1 - board_x0, board_y1 - board_y0
    top = base_t + board_t                           # z of the board back
    edge_left, edge_right = X(board_x0), X(board_x1)  # +X and -X board edges in the jig frame

    plate_x0, plate_x1 = edge_right - header_pin_len - 3.0, edge_left + socket_overhang + 2.5
    plate_y0, plate_y1 = min(Y(board_y0), Y(board_y1)) - 4.0, max(Y(board_y0), Y(board_y1)) + 4.0
    plate = (cq.Workplane("XY").box(plate_x1 - plate_x0, plate_y1 - plate_y0, top, centered=False)
             .translate((plate_x0, plate_y0, 0)))
    board_pocket = (cq.Workplane("XY").box(bw + 2 * pocket_fit, bl + 2 * pocket_fit, board_t + 1, centered=(True, True, False))
                    .translate((0, 0, base_t)))
    plate = plate.cut(board_pocket)
    for x, y, rot in passives:
        l, w, d = passive_pocket
        if rot % 180: l, w = w, l
        plate = plate.cut(cq.Workplane("XY").box(l, w, d + 1, centered=(True, True, False)).translate((X(x), Y(y), base_t - d)))
    for yy in (Y(board_y0), Y(board_y1)):            # push the board out from below
        plate = plate.cut(cq.Workplane("XY").circle(5).extrude(base_t + 1).translate((0, yy, -0.5)))

    # Socket mouths beyond the +X edge: floor level with the board back, stop wall at the overhang
    guide = (cq.Workplane("XY").box(plate_x1 - edge_left - pocket_fit, plate_y1 - plate_y0, guide_h, centered=False)
             .translate((edge_left + pocket_fit, plate_y0, top)))
    for r in rows:
        slot = (cq.Workplane("XY").box(socket_overhang + 0.1 + pocket_fit, socket_w + 2 * fit, guide_h + 1, centered=(False, True, False))
                .translate((edge_left, Y(r), top - 0.05)))
        guide = guide.cut(slot)

    # Header pins beyond the -X edge: one slot per pin, pivoted about the pad's inboard end for the tilt
    comb = (cq.Workplane("XY").box(edge_right - pocket_fit - plate_x0, plate_y1 - plate_y0, guide_h, centered=False)
            .translate((plate_x0, plate_y0, top)))
    pivot_x = X(header_x - header_pad_len / 2)      # inboard end of the header pads
    floor_z = top + pin_axis_z - header_pin_sq / 2 - 0.05
    slot_len = pivot_x - (edge_right - header_pin_len - 0.5)
    for r in rows:
        for k in (-1, 0, 1):
            slot = (cq.Workplane("XY").box(slot_len, header_pin_sq + 2 * fit, guide_h + 4, centered=(False, True, False))
                    .translate((pivot_x - slot_len, Y(r) + k * pitch, floor_z)))
            slot = slot.rotate((pivot_x, 0, top), (pivot_x, 1, top), header_tilt_deg)
            comb = comb.cut(slot)

    jig = plate.union(guide).union(comb)
    def engrave(text, x, y):
        t = (cq.Workplane("XY").workplane(offset=top + guide_h - 0.6).center(x, y)
             .text(text, 2.5, 1.0, kind="bold", halign="center", valign="center"))
        return t
    comb_mid = (plate_x0 + edge_right) / 2
    jig = jig.cut(engrave("SW5", comb_mid, plate_y1 - 2.0)).cut(engrave("SW1", comb_mid, plate_y0 + 2.0))
    jig = jig.cut(engrave(f"{header_tilt_deg:g}", (edge_left + plate_x1) / 2, plate_y0 + 2.0))

    tip_rise = (pivot_x - (edge_right - header_pin_len)) * math.sin(math.radians(header_tilt_deg))
    body_rise = (pivot_x - edge_right) * math.sin(math.radians(header_tilt_deg))
    foot_gap = header_pad_len * math.sin(math.radians(header_tilt_deg))
    print(f"tilt {header_tilt_deg} deg: pin tip {tip_rise:.2f} mm higher than flat, header outer end {body_rise:.2f} mm off the board, "
          f"foot {foot_gap:.2f} mm off its pad at the body")
    return jig

if __name__ == "__main__":
    for tilt in (0, 8):
        cq.exporters.export(build(tilt), f"../build/roamy_solder_jig_{tilt}deg.stl")

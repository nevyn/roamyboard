import cadquery as cq
import math

# -------- Parameters --------
keys = 6
key_pitch_y = 19.05

col_w = 18.0
col_h = 12 + (keys - 1) * key_pitch_y + 12
thickness = 12.0 # in mm of the whole column module

column_angle_deg = 8.0  # curvature per module (enforced by join geometry)

wall = 2.0
floor = 2.0
ceiling = 1.3

# Very rough Choc v1-ish cutout (iterate later)
choc_cut = 13.8

# Join (simple rectangular tongue/groove, rotated to create the angle)
join_depth = 2.0 # how far the socket extends from the body
join_height = 8.0 # how high the socket is, in the same dimension as thickness
join_indent = 1.0 
join_clearance = 0.7 # how many mm of clearance to add to the tsocket to fit the groove

# -------- Build straight shell --------
outer = cq.Workplane("XY").box(col_w, col_h, thickness, centered=True)

inner = (
    cq.Workplane("XY")
    .box(col_w - 2*wall, col_h - 2*wall, thickness - floor - ceiling, centered=True)
    .translate((0, 0, (floor - ceiling)/2))  # keep a floor and a top wall
)

body = outer.cut(inner)


# -------- Key cutouts (through top) --------
y0 = -col_h/2 + 12.0

for i in range(keys):
    y = y0 + i * key_pitch_y
    body = body.cut(
        cq.Workplane("XY")
        .center(0, y)
        .rect(choc_cut, choc_cut)
        .extrude(ceiling)
        .translate((0, 0, thickness/2 - ceiling))  # start near top face
    )

# -------- Angled sides --------

# make a triangular prism, and add it to the right groove side of the body
#prism = cq.Workplane("front")
#    .lineTo(2.0, 0)
#    .lineTo(2.0, 1.0)
#    .threePointArc((1.0, 1.5), (0.0, 1.0))
#    .close()
#    .extrude(0.25)

#show_object(prism, name="prism")

# -------- Joining geometry (T socket and groove) --------

# T socket on the right edge
def make_tsocket(clearance):
    tsocket = cq.Workplane("XY").box(join_depth, col_h, join_height - clearance, centered=True)
    cutout = cq.Workplane("XY").box(join_indent + clearance, col_h, join_indent + clearance, centered=True)
    tsocket = tsocket.cut(cutout.translate((-join_depth/2 + join_indent/2, 0, join_height/2 - join_indent/2)))
    return tsocket.cut(cutout.translate((-join_depth/2 + join_indent/2, 0, -join_height/2 + join_indent/2)))
pos_tsocket = make_tsocket(join_clearance).translate((col_w/2 + join_depth/2, 0, 0))
body = body.union(pos_tsocket)

# Groove on the left edge
neg_tsocket = make_tsocket(0) # join_height*1.1, join_indent*join_tolerance
neg_tsocket = neg_tsocket.translate((-col_w/2 + join_depth/2, 0, 0))
body = body.cut(neg_tsocket)

show_object(body, name="body")
show_object(body.translate((col_w, 0, 0)), name="body2")


# Optional exports:
# cq.exporters.export(body, "choc_column_mockup_joined.step")
# cq.exporters.export(body, "choc_column_mockup_joined.stl")

import cadquery as cq
import math

# -------- Parameters --------
keys = 6
key_pitch_y = 19.05

col_w = 19.0 # width of the column module, viewed from the top
col_h = 12 + (keys - 1) * key_pitch_y + 12 # height of the column module, viewed from the top
thickness = 12.0 # in mm of the whole column module

column_angle_deg = 8.0  # curvature per module (enforced by join geometry)

wall = 2.2
floor = 2.0
ceiling = 1.3

# Very rough Choc v1-ish cutout (iterate later)
choc_cut = 13.8

# Join (simple rectangular tongue/groove, rotated to create the angle)
join_depth = 1.4 # how far the socket extends from the body
join_height = 6.0 # how high the socket is, in the same dimension as thickness
join_indent = 0.6 
join_clearance = 0.2 # how many mm of clearance to add to the tsocket to fit the groove

# -------- Build straight shell --------
# outer shell
outer = cq.Workplane("XY").box(col_w, col_h, thickness, centered=True)
# inner shell
inner = (
    cq.Workplane("XY")
    .box(col_w - 2*wall, col_h - 2*wall, thickness - floor - ceiling, centered=True)
    .translate((0, 0, (floor - ceiling)/2))  # keep a floor and a top wall
)
# hollow body with wall, floor and ceilings
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

# make a triangular prism, and add it to the right side of the body. This way, each added column 
# contributes to an overall curvature of the full keyboard.
prism_displacement = math.tan(math.radians(column_angle_deg)) * thickness
body = (body.faces("<Y").workplane()
    .center(col_w/2, 0)
    .moveTo(0, -thickness/2)
    .lineTo(0, thickness/2)
    .lineTo(prism_displacement, thickness/2)
    .close()
    .extrude(-col_h)
)

# -------- Joining geometry (T socket and groove) --------
locking_depth = 2.0
# T socket on the right edge
def make_tsocket(clearance):
    stopper_length = 2.0
    socket_length = col_h - stopper_length
    tsocket = cq.Workplane("XY").box(join_depth, socket_length, join_height - clearance, centered=True)
    cutout = cq.Workplane("XY").box(join_indent + clearance, socket_length, join_indent + clearance, centered=True)
    tsocket = tsocket.cut(cutout.translate((-join_depth/2 + join_indent/2, 0, join_height/2 - join_indent/2)))
    tsocket = tsocket.cut(cutout.translate((-join_depth/2 + join_indent/2, 0, -join_height/2 + join_indent/2)))
    tsocket = tsocket.translate((0, stopper_length/2, 0))
    return tsocket

pos_tsocket = (make_tsocket(join_clearance)
    # cut a hole for the locking mechanism to attach to
    .cut(cq.Workplane("XY").box(join_depth, locking_depth, join_height, centered=True).translate((0, col_h/2, 0)))
    # position the socket outside the prism
    .rotate((0, 0, 0), (0, 1, 0), column_angle_deg)
    .translate((col_w/2 + join_depth/2 + prism_displacement/2, 0, 0))
)
body = body.union(pos_tsocket)

# Groove on the left edge
neg_tsocket = make_tsocket(0)
neg_tsocket = neg_tsocket.translate((-col_w/2 + join_depth/2, 0, 0))
body = body.cut(neg_tsocket)

## Locking mechanism
# Add a small block that locks at the top of the socket. Cut a U-shaped groove out of the body so 
# the lock can be pushed back and release the next module.
u_groove_depth = 2.0
u_groove_length = 6.0
u_groove_inset = 1.0
u_groove = (cq.Workplane("XY")
    .box(u_groove_depth, u_groove_length, join_height - 2)
    .cut(cq.Workplane("XY")
        .box(u_groove_depth - u_groove_inset, u_groove_length - u_groove_inset + join_clearance, join_height - 2 - u_groove_inset)
        .translate((-u_groove_inset/2, -u_groove_inset/2 + join_clearance/2, 0))
    )
    .translate((-col_w/2 + u_groove_depth/2 + join_depth, col_h/2 - u_groove_length/2, 0))
)
body = body.cut(u_groove)
tongue = (cq.Workplane("XY")
    # sorry, I ran out of patience to not use magic numbers :/
    .box(locking_depth*0.7, 1, join_height - 2 - u_groove_inset)
    .cut(cq.Workplane("XY").box(2, 1, 3).rotate((0, 0, 0), (0, 0, 1), 45).translate((-1, 0.2, 0))) # angled cutout to allow next module to slide in
    .translate((-col_w/2 + 1.7, col_h/2 - 0.5 + join_clearance, 0))
)
body = body.union(tongue)

# -------- Screw holes for the top ---------
# Screw holes for attaching the top plate to the body bottom
screw_hole_inset = 2.2
body = (body.faces(">Z").workplane()
    .center(-col_w/2, col_h/2)
    .rect(col_w - screw_hole_inset - 7.0, col_h - screw_hole_inset, forConstruction=True)
    .vertices().tag("screw_holes")
    .hole(1.0, 5.0)
    .workplaneFromTagged("screw_holes").hole(2.0, ceiling/2)
)


# -------- Prepare for preview and print --------

# Split for easier printing
split_z = -1.7  # just under switch plate
top = body.faces(">Z").workplane(offset=split_z).split(keepTop=True)
bottom = body.faces(">Z").workplane(offset=split_z).split(keepBottom=True)
show_object(top, name="top")
show_object(bottom, name="bottom")

# Preview the full body with 6 columns
if False:
    for i in range(5):
        body = (body
            .rotate((-col_w/2, 0, 0), (-col_w/2, 1, 0), column_angle_deg)
            .translate((col_w + prism_displacement/2, 0, 0))
        )
        show_object(
            body, name="body"+str(i)
        )


# Optional exports:
# cq.exporters.export(body, "choc_column_mockup_joined.step")
cq.exporters.export(top, "../build/roamy_top.stl")
cq.exporters.export(bottom, "../build/roamy_bottom.stl")

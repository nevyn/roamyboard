import cadquery as cq
import math

# -------- Parameters --------
keys = 5
key_pitch_y = 19.05

col_w = 18.0 # width of the column module, viewed from the top
col_h = 12 + (keys - 1) * key_pitch_y + 12 # height of the column module, viewed from the top
thickness = 12.0 # in mm of the whole column module
column_angle_deg = 8.0  # curvature per module (enforced by join geometry)
prism_displacement = math.tan(math.radians(column_angle_deg)) * thickness
centerline_x_offset = prism_displacement/2 # offset inner cutout and key cutout to use the space used by prism

side_wall = 1.8
edge_wall = 4.0
floor = 2.0
ceiling = 1.3

# Very rough Choc v1-ish cutout (iterate later)
choc_cut = 13.8

# Join (simple rectangular tongue/groove, rotated to create the angle)
join_depth = 1.4 # how far the socket extends from the body
join_height = 6.0 # how high the socket is, in the same dimension as thickness
join_indent = 0.6 
join_clearance = 0.3 # how many mm of clearance to add to the tsocket to fit the groove

interconnect_width = 22.86 + join_clearance # width of https://www.electrokit.com/upload/quick/dc/ae/ae5d_41022086.pdf (along y axis)
interconnect_height = 2.5 + join_clearance # height (along z)
interconnect_distance_from_bottom = 6.0

########################################################
# Generic module body
########################################################
def make_module_body(col_w, left_socket = True, right_socket = True):
    # -------- Main body --------
    # outer shell
    body = (cq.Workplane("XY")
        .box(col_w, col_h, thickness, centered=True)
        .tag("box")
        .faces("<X").tag("right_mate")
        .workplaneFromTagged("box")
    )

    # -------- Angled sides --------
    # make a triangular prism, and add it to the right side of the body. This way, each added column 
    # contributes to an overall curvature of the full keyboard.
    body = (body.faces("<Y").workplane()
        .center(col_w/2, 0)
        .moveTo(0, -thickness/2)
        .lineTo(0, thickness/2)
        .lineTo(prism_displacement, thickness/2)
        .close()
        .extrude(-col_h)
        .faces(">X").tag("left_mate")
    )

    # -------- Inner body --------
    # inner shell, that makes the body hollow
    inner = (
        cq.Workplane("XY")
        .box(col_w - 2*side_wall, col_h - 2*edge_wall, thickness - floor - ceiling, centered=True)
        .translate((centerline_x_offset, 0, (floor - ceiling)/2))  # keep a floor and a top side_wall
    )
    # hollow out the body so it gets walls, floor and ceiling
    body = body.cut(inner)

    # -------- Joining geometry (T socket and groove) --------
    locking_depth = 2.0
    # T socket on the right edge
    def make_tsocket(clearance):
        stopper_length = 2.0
        socket_length = col_h - stopper_length
        tsocket = cq.Workplane("XY").box(join_depth, socket_length, join_height - clearance, centered=True)
        cutout = cq.Workplane("XY").box(join_indent + clearance, socket_length, join_indent + clearance*2 + 0.5, centered=True)
        tsocket = tsocket.cut(cutout.translate((-join_depth/2 + join_indent/2, 0, join_height/2 - join_indent/2)))
        tsocket = tsocket.cut(cutout.translate((-join_depth/2 + join_indent/2, 0, -join_height/2 + join_indent/2)))
        tsocket = tsocket.translate((0, stopper_length/2, 0))
        return tsocket

    if(right_socket):
        pos_tsocket = (make_tsocket(join_clearance)
            # cut a hole for the locking mechanism to attach to
            .cut(cq.Workplane("XY").box(join_depth, locking_depth, join_height, centered=True).translate((0, col_h/2, 0)))
            # position the socket outside the prism
            .rotate((0, 0, 0), (0, 1, 0), column_angle_deg)
            .translate((col_w/2 + join_depth/2 + prism_displacement/2, 0, 0))
        )
        body = body.union(pos_tsocket)

    if(left_socket):
        # Groove on the left edge
        neg_tsocket = make_tsocket(0)
        neg_tsocket = neg_tsocket.translate((-col_w/2 + join_depth/2, 0, 0))
        body = body.cut(neg_tsocket)

        ## Locking mechanism
        # Add a small block that locks at the top of the socket. Cut a U-shaped groove out of the body so 
        # the lock can be pushed back and release the next module.
        u_groove_depth = 2.2
        u_groove_length = 8.0
        u_groove_inset = 1.0
        u_groove = (cq.Workplane("XY")
            .box(u_groove_depth, u_groove_length, join_height - 2)
            .cut(cq.Workplane("XY")
                .box(u_groove_depth - u_groove_inset - 0.4, u_groove_length - u_groove_inset + join_clearance, join_height - 2 - u_groove_inset)
                .translate((-u_groove_inset/2, -u_groove_inset/2 + join_clearance/2, 0))
            )
            .translate((-col_w/2 + u_groove_depth/2 + join_depth - 0.4, col_h/2 - u_groove_length/2, 0))
        )
        body = body.cut(u_groove)
        tongue = (cq.Workplane("XY")
            # sorry, I ran out of patience to not use magic numbers :/
            .box(locking_depth*0.7, 1, join_height - 2 - u_groove_inset)
            .cut(cq.Workplane("XY").box(3, 1, 3).rotate((0, 0, 0), (0, 0, 1), 40).translate((-1, 0.2, 0))) # angled cutout to allow next module to slide in
            .translate((-col_w/2 + 1.3, col_h/2 - 0.5 + join_clearance, 0))
        )
        body = body.union(tongue)

    # -------- Screw holes for the top ---------
    # Screw holes for attaching the top plate to the body bottom
    screw_hole_inset = 5.0
    body = (body.faces(">Z").workplane()
        .center(-col_w/2 + centerline_x_offset, col_h/2)
        .rect(col_w - screw_hole_inset - 2.0, col_h - screw_hole_inset, forConstruction=True)
        .vertices().tag("screw_holes")
        .hole(2.0, 13.0) # m2 screw holes, 12mm screws (too long, but it's what I have at hand)
        .workplaneFromTagged("screw_holes")
            .cboreHole(2.5, 3.44, ceiling/2, ceiling) # m2 screw head counterbore, plus wider hole so screw holds lid down but isn't screwed into it.
    )

    # -------- Interconnect cutout --------
    interconnect_cutout = (cq.Workplane("ZY")
        .rect(interconnect_height, interconnect_width)
        .extrude(col_w* (2 if (left_socket and right_socket) else 1))
        .translate((
            col_w if right_socket else 0, 
            -col_h/2 + interconnect_width/2 + interconnect_distance_from_bottom, 
            0
        ))
    )
    body = body.cut(interconnect_cutout)

    return body

def split_body(body):
    split_z = -ceiling  # just under top plate
    top = body.faces(">Z").workplane(offset=split_z).split(keepTop=True)
    bottom = body.faces(">Z").workplane(offset=split_z).split(keepBottom=True)

    return top, bottom

########################################################
# Keyswitch module
########################################################
keys_module = make_module_body(col_w)

# -------- Key cutouts (through top) --------
y0 = -col_h/2 + 12.0
for i in range(keys):
    y = y0 + i * key_pitch_y
    keys_module = keys_module.cut(
        cq.Workplane("XY")
        .center(centerline_x_offset, y)
        .rect(choc_cut, choc_cut)
        .extrude(ceiling)
        .translate((0, 0, thickness/2 - ceiling))  # start near top face
    )

keys_assembly = cq.Assembly()
(keys_top, keys_bottom) = split_body(keys_module)
keys_assembly.add(keys_top, name="top", color=cq.Color("red"))
keys_assembly.add(keys_bottom, name="bottom", color=cq.Color("orange"))

########################################################
# MCU module
########################################################
mcu_assembly = cq.Assembly()

# Parameters
mcu_width = 18.0 + join_clearance
mcu_height = 33.4
mcu_depth = 1.5 + join_clearance
mcu_offset_from_bottom = 2.0
battery_width = 30.0
display_width = 14.2
display_height = 36.2
display_inner_width = 10.6
display_inner_height = 25.2

### Internals
mcu_module = make_module_body(battery_width, right_socket = False)
board_box = (cq.Workplane("XY")
    .box(mcu_width, mcu_height, mcu_depth)
    .translate((0, -col_h/2 + mcu_height/2 + 0.2, -thickness/2 + floor + mcu_offset_from_bottom))
)
mcu_module = mcu_module.cut(board_box)

# So that the MCU has something to rest on, but still has room underneath on the sides for solder joints etc
support_under_mcu = (cq.Workplane("XY")
    .box(mcu_width/2, mcu_height/2, mcu_offset_from_bottom)
    .translate((0, -col_h/2 + mcu_height/2, -thickness/2 + floor))
)
mcu_module = mcu_module.union(support_under_mcu)

usb_cutout = (cq.Workplane("XZ")
    .rect(9.0 + join_clearance, 3.3 + join_clearance, centered=True).extrude(-edge_wall)
    .edges("|Y").fillet(1.0)
    .translate((0, -col_h/2, -thickness/2 + floor + mcu_offset_from_bottom))
)
mcu_module = mcu_module.cut(usb_cutout)

# to keep the mcu in place
stopper = (cq.Workplane("XY")
    .box(mcu_width/2, 1.0, mcu_offset_from_bottom + mcu_depth/2)
    .translate((0, -col_h/2 + mcu_height + 0.5 + join_clearance, -thickness/2 + floor + mcu_offset_from_bottom/2))
)
mcu_module = mcu_module.union(stopper)

# Assembly
(mcu_top, mcu_bottom) = split_body(mcu_module)
mcu_assembly.add(mcu_top, name="top", color=cq.Color("blue"))
mcu_assembly.add(mcu_bottom, name="bottom", color=cq.Color("green"))

if False:
    nicenano = (cq.importers.importStep("../step/nice-nano-v2-1.snapshot.2/nice-nano_v2.step")
        .rotate((0, 0, 0), (0, 0, 1), 90)
        .translate((0, -col_h/2 + mcu_height/2 + 0.2, -thickness/2 + floor + mcu_offset_from_bottom - 0.75))
    )
    mcu_assembly.add(nicenano, name="nicenano")


########################################################
# Meta work: previewing and printing
########################################################

def export(assembly, name):
    cq.exporters.export(assembly.objects["top"].obj, "../build/roamy_"+name+"_top.stl")
    cq.exporters.export(assembly.objects["bottom"].obj, "../build/roamy_"+name+"_bottom.stl")

def show_recursive_assembly(asm: cq.Assembly, base_name: str = "", parent_loc = None):
    if parent_loc is None: parent_loc = cq.Location()
    for item in asm.children:
        name = item.name
        obj_name = f"{base_name}/{name}"
        loc = parent_loc * item.loc
        if len(item.children) > 0:
            show_recursive_assembly(item, obj_name, loc)
            continue
        item.loc = loc # this actually breaks the full_assembly, but we're not usign it so whatev 😅
        show_object(item, name=obj_name)

full_assembly = cq.Assembly()
full_assembly.add(keys_assembly, name="keys")
full_assembly.add(mcu_assembly, name="mcu")
full_assembly.constrain("keys/bottom?left_mate", "mcu/bottom?right_mate", "Plane")
full_assembly.constrain("keys/bottom?left_mate", "mcu/bottom?right_mate", "Axis")
full_assembly.solve()
show_recursive_assembly(full_assembly, "roamy")
export(keys_assembly, "keys")
export(mcu_assembly, "mcu")

# Preview the full body with 6 columns
if True:
    for i in range(5):
        keys_module = (keys_module
            .rotate((col_w/2, 0, 0), (col_w/2, 1, 0), -column_angle_deg)
            .translate((-col_w - prism_displacement/2, 0, 0))
        )
        show_object(
            keys_module, name="keys_"+str(i)
        )



import cadquery as cq
import math

# -------- Parameters --------
keys_per_column = 5
key_pitch_y = 19.05

col_w = 18.0 # width of the column module, viewed from the top
col_h = 12 + (keys_per_column - 1) * key_pitch_y + 12 # height of the column module, viewed from the top
thickness = 12.0 # in mm of the whole column module
column_angle_deg = 8.0  # curvature per module (enforced by joint geometry)
prism_displacement = math.tan(math.radians(column_angle_deg)) * thickness
centerline_x_offset = prism_displacement/2 # offset inner cutout and key cutout to use the space used by prism

side_wall = 1.8
edge_wall = 4.0
floor = 2.0
ceiling = 1.3

# Very rough Choc v1-ish cutout (iterate later)
choc_cutout_size = 13.8

# T-slot joint (tongue and groove, rotated to create the angle)
joint_depth = 1.4 # how far the socket extends from the body
joint_height = 8.0 # how high the socket is, in the same dimension as thickness
joint_indent = 0.6 
joint_clearance = 0.45 # how many mm of clearance to add to the tongue to fit the groove
general_clearance = 0.4 # standard clearance for everything else

interconnect_width = 22.86 + general_clearance # width of https://www.electrokit.com/upload/quick/dc/ae/ae5d_41022086.pdf (along y axis)
interconnect_height = 2.5 + general_clearance # height (along z)
interconnect_distance_from_bottom = 6.0

########################################################
# Generic module body
########################################################
def make_module_body(col_w, left_socket = True, right_socket = True, extra_screw_inset = 0.0):
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

    # -------- T-slot joint (tongue and groove) --------
    locking_depth = 2.0
    def make_t_slot_profile(clearance):
        stopper_length = 2.0 + clearance
        socket_length = col_h - stopper_length
        tsocket = cq.Workplane("XY").box(joint_depth, socket_length, joint_height - clearance, centered=True)
        cutout = cq.Workplane("XY").box(joint_indent + clearance, socket_length, joint_indent + clearance*2 + 1.3, centered=True)
        tsocket = tsocket.cut(cutout.translate((-joint_depth/2 + joint_indent/2, 0, joint_height/2 - joint_indent/2)))
        tsocket = tsocket.cut(cutout.translate((-joint_depth/2 + joint_indent/2, 0, -joint_height/2 + joint_indent/2)))
        tsocket = tsocket.translate((0, stopper_length/2, 0))
        return tsocket

    if(right_socket):
        tongue = (make_t_slot_profile(joint_clearance)
            # cut a hole for the locking mechanism to attach to
            .cut(cq.Workplane("XY").box(joint_depth, locking_depth, joint_height, centered=True).translate((0, col_h/2, 0)))
            # position the socket outside the prism
            .rotate((0, 0, 0), (0, 1, 0), column_angle_deg)
            .translate((col_w/2 + joint_depth/2 + prism_displacement/2, 0, 0))
        )
        body = body.union(tongue)

    if(left_socket):
        # Groove on the left edge
        groove = make_t_slot_profile(0)
        groove = groove.translate((-col_w/2 + joint_depth/2, 0, 0))
        body = body.cut(groove)

        ## Locking mechanism
        # Add a small block that locks at the top of the socket. Cut a U-shaped groove out of the body so 
        # the lock can be pushed back and release the next module.
        u_groove_depth = 2.8
        u_groove_length = 12.0
        u_groove_inset = 1.0
        u_groove = (cq.Workplane("XY")
            .box(u_groove_depth, u_groove_length, joint_height - 2)
            .cut(cq.Workplane("XY")
                .box(u_groove_depth - u_groove_inset - 0.4, u_groove_length - u_groove_inset + joint_clearance, joint_height - 2 - u_groove_inset)
                .translate((-u_groove_inset/2, -u_groove_inset/2 + joint_clearance/2, 0))
            )
            .translate((-col_w/2 + u_groove_depth/2 + joint_depth - 0.4, col_h/2 - u_groove_length/2, 0))
        )
        body = body.cut(u_groove)
        tongue = (cq.Workplane("XY")
            # sorry, I ran out of patience to not use magic numbers :/
            .box(locking_depth*1.1, 1, joint_height - 2 - u_groove_inset)
            .cut(cq.Workplane("XY").box(5, 1, 5).rotate((0, 0, 0), (0, 0, 1), 40).translate((-1, 0.2, 0))) # angled cutout to allow next module to slide in
            .translate((-col_w/2 + 1.5, col_h/2 - 0.5 + joint_clearance, 0))
        )
        body = body.union(tongue)

    # -------- Screw holes for the top ---------
    # Screw holes for attaching the top plate to the body bottom
    screw_hole_inset = 5.0
    body = (body.faces(">Z").workplane()
        .center(-col_w/2 + centerline_x_offset, col_h/2)
        .rect(col_w - screw_hole_inset - extra_screw_inset, col_h - screw_hole_inset, forConstruction=True)
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
keys_module = make_module_body(col_w, extra_screw_inset=4.0)

# -------- Key cutouts (through top) --------
y0 = -col_h/2 + 12.0
for i in range(keys_per_column):
    y = y0 + i * key_pitch_y
    keys_module = keys_module.cut(
        cq.Workplane("XY")
        .center(centerline_x_offset, y)
        .rect(choc_cutout_size, choc_cutout_size)
        .extrude(ceiling)
        .translate((0, 0, thickness/2 - ceiling))  # start near top face
    )

keys_assembly = cq.Assembly()
(keys_top, keys_bottom) = split_body(keys_module)
keys_assembly.add(keys_top, name="top", color=cq.Color("green1"))
keys_assembly.add(keys_bottom, name="bottom", color=cq.Color("orange1"))

########################################################
# MCU module
########################################################
mcu_assembly = cq.Assembly()

# Parameters
mcu_width = 18.0 + general_clearance*2
mcu_height = 33.4 + general_clearance
mcu_depth = 1.5 + general_clearance*4
mcu_offset_from_end = 0.8
mcu_offset_from_bottom = 2.0
battery_width = 30.5
display_width = 14.2
display_height = 36.2
display_inner_width = 10.6
display_inner_height = 25.2
mcu_module_width = battery_width + side_wall*2

### Internals
mcu_module = make_module_body(mcu_module_width, right_socket = False, extra_screw_inset=4.0)
mcu_pocket = (cq.Workplane("XY")
    .box(mcu_width, mcu_height, mcu_depth)
    .translate((0, -col_h/2 + mcu_height/2 + mcu_offset_from_end, -thickness/2 + floor + mcu_offset_from_bottom))
)
mcu_module = mcu_module.cut(mcu_pocket)

# So that the MCU has something to rest on, but still has room underneath on the sides for solder joints etc
support_under_mcu = (cq.Workplane("XY")
    .box(mcu_width/2, mcu_height/2, mcu_offset_from_bottom)
    .translate((0, -col_h/2 + mcu_height/2, -thickness/2 + floor))
)
mcu_module = mcu_module.union(support_under_mcu)

usb_cutout = (cq.Workplane("XZ")
    .rect(9.0 + general_clearance*2, 3.3 + general_clearance*2, centered=True).extrude(-edge_wall)
    .edges("|Y").fillet(1.0)
    .translate((0, -col_h/2, -thickness/2 + floor + mcu_offset_from_bottom))
)
mcu_module = mcu_module.cut(usb_cutout)

# to keep the mcu in place
stopper = (cq.Workplane("XY")
    .box(mcu_width/2, 1.0, mcu_offset_from_bottom + mcu_depth/2)
    .translate((0, -col_h/2 + mcu_height + mcu_offset_from_end + 1.0/2 - 0.2 , -thickness/2 + floor + mcu_offset_from_bottom/2))
)
mcu_module = mcu_module.union(stopper)


# Belt clip

clip_length = 30.0 # along y, the long side of the module
clip_width = 10.0 # along x, out from the edge of the module
clip_inset = 6.0 # how much space for the belt inside the handle
clip_depth = thickness - ceiling # along z, same as the module thickness
belt_clip = (cq.Workplane("XY")
    .rect(clip_width, clip_length) # outer handle
     # cutout for the belt
    .rect(clip_width - clip_inset, clip_length - clip_inset)
    .extrude(clip_depth)
)
mcu_clip = (belt_clip
    # grab the outermost edges and round them
    .edges(">X").fillet(2.0)
)
# attach the belt_clip to the >X face of the module
lower_clip = (mcu_clip
    .rotate((0, 0, 0), (0, 1, 0), column_angle_deg)
    .translate((mcu_module_width/2 + clip_width/2, -col_h/2 + clip_length/2, -thickness/2 - 0.6))
)
upper_clip = (mcu_clip
    .rotate((0, 0, 0), (0, 1, 0), column_angle_deg)
    .translate((mcu_module_width/2 + clip_width/2, +col_h/2 - clip_length/2, -thickness/2 - 0.6))
)
mcu_module = mcu_module.union(lower_clip)
mcu_module = mcu_module.union(upper_clip)

# Assembly
(mcu_top, mcu_bottom) = split_body(mcu_module)
mcu_assembly.add(mcu_top, name="top", color=cq.Color("green2"))
mcu_assembly.add(mcu_bottom, name="bottom", color=cq.Color("orange2"))

if False:
    nicenano = (cq.importers.importStep("../step/nice-nano-v2-1.snapshot.2/nice-nano_v2.step")
        .rotate((0, 0, 0), (0, 0, 1), 90)
        .translate((0, -col_h/2 + mcu_height/2 + mcu_offset_from_end, -thickness/2 + floor + mcu_offset_from_bottom - 0.75))
    )
    mcu_assembly.add(nicenano, name="nicenano")

########################################################
# Terminator module
########################################################
terminator_assembly = cq.Assembly()
terminator_width = 12.0

terminator_module = make_module_body(terminator_width, left_socket = False)

# Add belt clips to the terminator module as well
terminator_clip = (belt_clip
    # grab the outermost edges and round them
    .edges("<X").fillet(2.0)
)
lower_clip = (terminator_clip
    .translate((-terminator_width/2 - clip_width/2, -col_h/2 + clip_length/2, -thickness/2))
)
upper_clip = (terminator_clip
    .translate((-terminator_width/2 - clip_width/2, +col_h/2 - clip_length/2, -thickness/2))
)
terminator_module = terminator_module.union(lower_clip)
terminator_module = terminator_module.union(upper_clip)


# Assembly
(terminator_top, terminator_bottom) = split_body(terminator_module)
terminator_assembly.add(terminator_top, name="top", color=cq.Color("green3"))
terminator_assembly.add(terminator_bottom, name="bottom", color=cq.Color("orange3"))


########################################################
# Pin bend jig
########################################################
# Bends the pins of a Harwin M20-889 1x3 header (electronics/Electronics.md) to the joint
# angle before the header is soldered. The header body sits in a pocket, its pins lie on a
# flat land for jig_bend_offset mm, then the ram presses them down onto the sloped floor.
jig_body_w, jig_body_d, jig_body_h = 7.62, 2.50, 2.50  # header body: x along the pin row, y along the pins, z height
jig_pin_sq, jig_pin_len, jig_tail_len = 0.64, 6.0, 3.0
jig_bend_offset = 2.0   # straight pin length before the bend: the case wall thickness at PCB height
jig_overbend_deg = 2.0  # brass springs back a little; bend past the joint angle by this much
jig_fit = 0.15

def make_pin_bend_jig():
    base_w, base_d, base_h = 20.0, 18.0, 12.0
    land_z = 6.0                                        # flat land under the straight pin part
    pocket_w = jig_body_w + 2*jig_fit
    body_floor_z = land_z - (jig_body_h - jig_pin_sq)/2  # so the pin underside rests on the land
    y_face = -3.0                                       # header body front face; pins start here
    y_bend = y_face + jig_bend_offset
    ramp_len = jig_pin_len - jig_bend_offset + 1.0
    ramp_drop = ramp_len * math.tan(math.radians(column_angle_deg + jig_overbend_deg))

    base = cq.Workplane("XY").box(base_w, base_d, base_h, centered=(True, True, False))
    body_pocket = (cq.Workplane("XY")
        .box(pocket_w, jig_body_d + 2*jig_fit, base_h, centered=(True, False, False))
        .translate((0, y_face - jig_body_d - 2*jig_fit, body_floor_z))
    )
    tail_relief = (cq.Workplane("XY")
        .box(pocket_w, jig_tail_len + 1.0, base_h, centered=(True, False, False))
        .translate((0, y_face - jig_body_d - 2*jig_fit - jig_tail_len - 1.0, body_floor_z - 2.0))
    )
    ram_slot = (cq.Workplane("XY")
        .box(pocket_w, y_bend + ramp_len - y_face, base_h, centered=(True, False, False))
        .translate((0, y_face, land_z))
    )
    ramp = (cq.Workplane("YZ")
        .polyline([(y_bend, land_z), (y_bend + ramp_len, land_z), (y_bend + ramp_len, land_z - ramp_drop)]).close()
        .extrude(pocket_w/2, both=True)
    )
    base = base.cut(body_pocket).cut(tail_relief).cut(ram_slot).cut(ramp)

    # Ram: flat over the land, sloped over the ramp, always one pin thickness above the floor.
    top_z = base_h + 5.0
    y0, y1 = y_face + 0.25, y_bend + ramp_len - 0.25
    ram = (cq.Workplane("YZ")
        .polyline([
            (y0, land_z + jig_pin_sq),
            (y_bend, land_z + jig_pin_sq),
            (y1, land_z + jig_pin_sq - (y1 - y_bend) * math.tan(math.radians(column_angle_deg + jig_overbend_deg))),
            (y1, top_z),
            (y0, top_z),
        ]).close()
        .extrude(pocket_w/2 - jig_fit, both=True)
    )
    return base, ram

jig_base, jig_ram = make_pin_bend_jig()
jig_assembly = cq.Assembly()
jig_assembly.add(jig_base, name="base", color=cq.Color("gray"))
jig_assembly.add(jig_ram.translate((0, 0, 10.0)), name="ram", color=cq.Color("blue"))


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
full_assembly.add(terminator_assembly, name="terminator")
full_assembly.constrain("keys/bottom?left_mate", "mcu/bottom?right_mate", "Plane")
full_assembly.constrain("keys/bottom?left_mate", "mcu/bottom?right_mate", "Axis")
full_assembly.constrain("keys/bottom?right_mate", "terminator/bottom?left_mate", "Plane")
full_assembly.constrain("keys/bottom?right_mate", "terminator/bottom?left_mate", "Axis")
full_assembly.solve()
show_recursive_assembly(full_assembly, "roamy")
export(keys_assembly, "keys")
export(mcu_assembly, "mcu")
export(terminator_assembly, "terminator")
cq.exporters.export(jig_base, "../build/roamy_pin_jig_base.stl")
cq.exporters.export(jig_ram, "../build/roamy_pin_jig_ram.stl")
show_recursive_assembly(jig_assembly, "jig")

# Preview the full body with 6 columns
if False:
    for i in range(5):
        keys_module = (keys_module
            .rotate((col_w/2, 0, 0), (col_w/2, 1, 0), -column_angle_deg)
            .translate((-col_w - prism_displacement/2, 0, -0.2))
        )
        show_object(
            keys_module, name="keys_"+str(i)
        )



So what I'm thinking is that I will need a hard enclosure for the electronics, because a flexible keyboard is very hard to type on (that's an earlier failed experiment of mine). So I want to basically build a top plate with roughly the same layout as lily58, but curved (I'm thinking flat sections with a slight bend in between each column). And then I can make a bottom case with the same curvature to mount battery, controller etc in.

# Version 3
![[Modular sketch.png]]
Before even finishing version 2, Bengan wanted in on the project, but focused on just a single half and for gaming.
That made me thinking: What if roamyboard is made out of modular columns? That you can snap together to make it as narrow or wide as you want. By making the edges sloped, you create curvature when snapping them together. Each column could then also have its own PCB.

This iteration is designed in CadQuery and the file can be found in roamy-v3.py. Read more in [[Modular Design (v3)]].

## 3.0
![[proto 3.0.png]]
Learnings:
- [x] Print standing up, not laying down. The supports going into the module are terrible.
- [x]  Make the T socket smaller. The tolerances won't allow it to go in
- [x]  Keys are too wide apart. Make the groove negative, and remove some margins on the main body

## 3.1
![[proto 3.1.png]]
![[proto 3.1 irl.png]]
* Standing up print helped a lot!
* 0.275 top Z distance helped a lot to make supports easy to remove
* My clearance math is bad so I immediately rewrote it for 3.2, so we'll see if it actually works then too
* Forgot to make the keys less wide apart
## 3.2
![[proto 3.2.png]]
* Nicer math for clearance
## 3.3
![[proto 3.3.png]]
![[proto 3.3 irl.png]]
* Curvature!

Learnings:
- [x] Too much clearance. The pieces don't snap together anymore.
- [x] If I make two pieces that need to be glued together, there needs to be a groove or something to guide alignment when putting them back together
	- I made screw holes. Let's avoid glue.
- Being open on the top is actually pretty nice. I liked the initial idea of sliding the PCB in from the short end, but it will make debugging so much harder. OTOH, with a top plate, I will need a lot of ugly screws...
- [x] The columns are now so narrow that the plastic clips on the switches don't engage and the switches just fall out. Make it wider so the clips can engage.
- At the same time, the keys are a little bit too widely apart compared to a normal keyboard...

Todo:
- [x] Design locking and stopping mechanism for the socket
	- [x] Stopping
	- [x] Locking
- [ ] Order
	- [x] Shift registers
	- [x] Pogo pins 
	- [ ] Pogo pads
	- [ ] kailh sockets
- [ ] Design the brain module
- [ ] Design the terminator module
- [ ] Learn KiCad and design the PCBs :O

## 3.4
![[proto 3.4.png]]
![[proto 3.4 irl.png]]

Learnings/todo
- [x] The locking mechanism shouldn't have a hole to the body, and doesn't need that much x-space behind it
- [x] Cutout for connection between modules
- [x] The right wall (by the t socket) is needlessly thick due to the prism. Can I make it thinner? Move the prism "inside" the body? Cut out the inner hollow _after_ joining with the prism and shift it to the right?
After print:
- [x] ok now clearance is too LOW :/ I can't even force the pieces together more than a third of the way.
- [x] Screw holes are too small. M2 is 2 mm, so aim for that.
- [x] Screw hole indents on the top plate must not have supports because that clogs the hole, so lay them upwards on the build plate 
- [x] and remove the ridges on the underside of the top plate (move the cut to JUST below the ceiling)
- [x] Actually, it DOES need that much x-space behind it! Re-add it please
## 3.5
![[proto 3.5.png]]
![[proto 3.5 irl.png]]
Learnings/todo for next version
- [x] Modularize so we can create different kind of module boxes
	- [x] Fix exports
	- [x] Fix showing subassemblies in a way that can be enabled/disabled
- [ ] Use this to create the MCU module
	- [x] Make it not have a right socket
	- [x] USB cutout
	- [x] MCU attachment
After print:
- [x] The hole in the top plate should be wider than the screw's 2mm so it only grabs onto the bottom
- [x] Clearance still isn't great. Look into having an asymmetrical clearance the way it was in 3.1
- [x] The tongue doesn't quite engage, and the bottoms don't quite align. Add clearance to the bottom of the groove, by the stopper! Maybe make the stopper 1% thinner on the receiving side.
## 3.6
Let's try this weird clearance which is more like 3.1.
![[proto 3.6 clearance.png]]
![[proto 3.6.png]]
![[proto 3.6 irl.jpeg]]

Learnings
- [x] The rails are STILL too tight. Loosen clearance again. Maybe make the wings even bigger?
- [ ] The supports are killing me. Small details keep breaking. Figure out a better printing solution.
- [x] The tongue is too weak, it breaks. Make it thicker.
- [x] The USB/MCU slot is too narrow, so the MCU won't fit.
- [x] The battery doesn't fit. Make the enclosure wider.
Todos
- [x] Create the terminator module
	- [x] Make it not have a left groove
	- [x] Harness attachment
- [ ] Continue on the MCU module
	- [x] Harness attachment
	- [ ] Display window
- [ ] Order
	- [ ] M2 6mm screws
## 3.7

![[proto 3.7.png]]
![[proto 3.7 close.png]]
![[proto 3.7 irl.jpeg]]
Learnings
- [x] WTF, I changed clearance to 0.4 and it's STILL too tight‽ I can just barely make it work by shaving off debris
- [x] Make the tongue longer so it attaches better
- [x] About 2mm TOO MUCH clearance for the battery
- [x] MCU is a LITTLE loose. I could make the distance to the stopper maybe 0.2mm shorter.
- [x] Tree supports are great. Everything came off super easily without breaking.

## 3.8
![[proto 3.8 irl.png]]
Learnings
- [ ] It's perfect.
- [ ] Actually, I could add a rounding at the bottom of the T socket so that it slides into the groove more easily.
- [ ] And... I need more depth into the column to fit all the electronics :S I might have to print new lower halves.

# Version 2
Curvature. How much is the right curvature? I could math it. Or wing it. Still working in the same OnShape file. Here's V1 compared to wingin' it for v2:
![[curvature1.png]]
![[curvature2.png]]

- [x] Wider curvature
- [x] Narrower between keys
- [x] Make it more variable driven
- [ ] Another column to fit the microcontroller, screen and battery
- [ ] Use the profile of actual switch to make a fitting that hooks into the switch
	- [ ] Sink the top of the switch into the part
	- [ ] Make it snug
- [ ] Close up the two unused holes
- [ ] Fix screw posts
- [ ] On the bottom case, add something for fastening to the harness
- [ ] Try making the base an actual curve and just the key seats square! 
	- [ ] Probably means remaking the model from scratch.
	- [ ] Can I project a sketch onto a shape to pattern the holes?

## Version 1
We're [designing `roamyboard` in OnShape](https://cad.onshape.com/documents/851b4428f77cf5984ffec433/w/909caff9b78525c7946543d4/e/0bc4b4ece8e92a1460095204).

![[roamyboard_v1.png]]
See [[Parts]] to see what key caps etc were used.

Learnings:
* Slightly higher curvature; it's too tight on my leg
* Slightly narrower between keys
* A good position on the leg is out towards the side, roughly 45° leaning, not centered on leg. So right below/atop the pocket.
* The plate is actually too thick for the keys to anchor properly! Use the key model as a prop to make a perfect little hole for the keys.
* One more column for the screen basically
* Column offsets? The leftmost and rightmost columns aren't very easy to reach... I hope this is fixed by just moving the keys closer together :S
* I'm going to have to order more buttons and caps I think
* Interestingly, it needs to be located at pocket height when standing up, and just above the knee when stitting down. So it'll need an adjustment system to move it up and down the leg
* I forgot to order "homing" keycaps, for finding the home row by touch.



## Tools 
Raj recommends doing it manually in OnShape. 

There are also generators:
```embed
title: "Cosmos Keyboard"
image: "https://ryanis.cool/cosmos/alien.svg"
description: "Custom-Build A Keyboard Fit To You"
url: "https://ryanis.cool/cosmos/"
```

```embed
title: "Ergogen"
image: "https://github.com/madebyperce.png"
description: "Web UI for the ergogen tool"
url: "https://ergogen.xyz/#"
```

## Process
So we're making a top case with holes for switches, similar to Choczard:
![[choczard grid.png]]

... but with the rough layout of lily58:
[![Lily58Lite-Pic](https://user-images.githubusercontent.com/6285554/84393842-13960900-ac37-11ea-811e-65db2948ca73.jpg)](https://user-images.githubusercontent.com/6285554/84393842-13960900-ac37-11ea-811e-65db2948ca73.jpg)
... so what we're looking at on each half **is a 6x4 grid, plus an extra row of 4-ish thumb keys**.

Here's Gergo for reference:
[![Ortholinear Keyboard Poll | Drop](https://massdrop-s3.imgix.net/img_poll/1573329558104.086624807538320047948280-md_img.jpg?auto=format&fm=jpg&fit=fill&w=400&h=400&bg=FFFFFF&dpr=1&q=70)![Ortholinear Keyboard Poll | Drop](https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTB-r2tiqPlEdZ_vJLD98Y08Sk4slpzM84dsg&s)](https://www.google.com/url?sa=i&url=https%3A%2F%2Fdrop.com%2Fvote%2FOrtholinear-Keyboard3&psig=AOvVaw1hnNe-H3Z-tfUHhFxrLA26&ust=1739209569608000&source=images&cd=vfe&opi=89978449&ved=0CBQQjRxqFwoTCNDH4N-St4sDFQAAAAAdAAAAABAE)

As for metrics, Raj says:
> I’d recommend not trying to design in every little detail. Maybe start with a rough shape with holes for switches. The choc keys are 14x14 square. Should be 1mm deep wall for the clips to work

## 4.0
Redesigned from scratch in Cadova (Swift): `case/roamy-v4/`, see its README. The v3 script stays for reference; its T-slot, prism and clearance work does not carry over.

What the key module is now (first-iteration track, `case-v4-first-iteration`):
- **Joint geometry.** The tilted header fixes the neighbour's pose: its socket bore lies on this module's pin line, mouth 2.0 mm past the header board edge (0.39 mm from the header body, 5.6 mm pin insertion). That pose is one 8° rotation about a centre 149 mm below the board. For outer edges to meet, the module is a keystone about that centre: both side faces radial, top and floor perpendicular to their bisector. Because the tilted header seats the neighbour 1.1 mm lower at the seam, the bisector leans 2.93° from the board normal: the whole outline is skewed against the board, the top falls 1.1 mm toward the header side, and neighbours' top and bottom edges meet exactly. Faces lean −1.07° (socket) and +6.93° (header) from the board normal.
- **Top.** One plane; each switch sits in a 15.3 mm pocket whose floor is the 1.3 mm plate parallel to the board, 0 deep on the header side, 0.78 on the socket side. The opening's top edge has a 0.4 mm 45° seat chamfer, which leaves the top housing 0.25 mm of seat per side. Switch opening 13.7 (coupon 2026-09-25). `choc-cutout-coupon` has openings 13.5 to 14.2 in a plate-thick strip; reprint it after a printer or filament change.
- **Board** drops into the upturned shell; wall slots at the connector rows are open toward the floor (socket side up to the board, header side up to the tilted body's top + 0.3). Held between ceiling ledges (full width at the ends, continuous on the header side, tabs between housings on the socket side) and four floor pillars under its bare end margins.
- **Stack** at the socket face / header face: floor 1.5, floor top 2.80 / 3.85 under the board (0.3 under the lowest part: socket body and hotswap sockets on the socket side, the tilted header body on the header side), board 1.6, 0.9 to the ceiling, plate 1.3; 8.94 mm perpendicular to the top. Connector numbers as measured 2026-09-26 (Electronics.md): bodies on the board, axis 1.25.
- **Clamp pads and rear stops**, one L-shaped block on the floor under each connector body (`ClampPadsAndRearStops`). The clamp pad rests on the body's face away from the board, so the body can't tip toward the floor. It touches with zero interference: the header is soldered tilted 8°, touching its pad only at the inboard end with 0.32 mm under the foot's other end, and a pad that pressed it flat would bend the joints that it protects, so the header's pad follows the tilted face. The rear stop bears on the body's rear face below the tails, so slide-on pushes the body into the floor instead of shearing its joints. On the socket it covers the rear face from the top to 0.3 mm under the tails (0.75 mm), standing perpendicular to the floor (0.04 mm off the face at its top), so the floor can go on along the board normal or its own. On the header it meets the body only along the edge between the rear face and the top: the rear face leans 8° with the tilt and overhangs the floor's move, so the stop stands along the board normal. Stops are 1.2 mm thick along the pin axis and keep 0.3 mm from the tails and 0.6 mm from the feet for their solder fillets; the space 0.5 mm beside each body's long sides stays free.
- **Separation** pulls each body away from its pad and stop, and there is no room for a pull stop: the header body and the neighbour's socket mouth are 0.39 mm apart at the seam. Staking carries it instead: two-component epoxy along both long sides of each body, between the body and the board. The board then bears on its side wall after its 0.2 mm float.
- **Joint hardware**, at each column end, in 6.6 mm end walls (module 113.6 mm long):
  - Guide pin on the header face, along the connector pins: 2.2 wide with flat sides and 45° gables (3.6 tall), 11.3 mm reach, 1 mm taper. It enters its hole 5.7 mm before the connector pins reach the socket mouths, so the pins arrive aligned in y and z; 0.1 clearance.
  - Spring flap: the outer 1.0 mm skin of the neighbour's end wall over the hole, hinged near the socket face, free for 10.2 mm. A tooth on its inside, 2.0 long, 0.6 engagement and the hole's full 3.8 height, 45° lead-in, clicks into a notch in the pin as the faces close; it rides the last 2.3 mm. Tooth 8.6 from the hinge: strain about 1.4 % at 0.7 deflection, bending within the layers; about 8 N to deflect. `toothCatchAngle` 45° pulls apart by hand; 90° locks until the flap is pried out at its free-edge slot.
  - Floor: two M2s per end, in the end walls' corners behind the guide hole and pin; 1.8 pilots in the shell, 2.4 clearance in the floor (1.6 was too tight to self-tap). Counterbores bridged in two one-layer steps (slot, then square) so they print bottom-down cleanly. Tabs 1.0 mm tall along both side walls between the connector rows and along the end walls locate the floor with 0.1 mm clearance; they stay under the Choc legs.
- Replaces the cantilever hooks: no part sticks out on its own except the guide pins, and nothing else needs support.

Checked on the generated meshes (`tools/check.py`, manifold booleans): board positions match KiCad's own 3D export; no overlap between any pair of parts of two joined modules; header pins overlap the neighbour's socket bodies by exactly 9 × 5.6 mm (coaxial); sliding the neighbour on along its board plane collides only where the tooth rides the pin, the last 2.3 mm; the bare board rises out of the shell along its normal without touching it; each connector body's clamp pad and rear stop touch it without overlap (moving the floor 0.05 mm toward the board, or the body 0.05 mm along the pin axis into the module, overlaps), keep clear of the tails, the feet's fillets and the staking room, and the floor goes on along the board normal or its own normal without touching a body, tail or part before its last 0.05 mm.

Printing: shell top-down (`key-module-print` places it on the top plane). Paint slicer support under the two guide pins only (tree, build plate only, top Z distance about 0.275); their lowest ridge is an 11 mm cantilever over the bed. Floor bottom-down. `tools/overhang.py` slices both print poses and fails on anything that droops outside the painted-support zone.

The pocket floor is parallel to the board and the print bed to the top, so in the print pose each seat is tilted 2.9° to the bed, 0 to 0.78 mm above it, and hangs over the empty pocket. Without the chamfer it reaches 0.8 mm past the layer below, and r3 drooped strands along the opening edges. The chamfer cuts that to 0.57 mm, which slicers print as a slowed overhang wall. The Choc clips catch the plate's underside, so the chamfer doesn't change their grip; it only narrows the seat. A one-layer membrane across the opening (r4, r5) made it worse: tilted like the seat, it sliced into 3.9 mm bands with nothing on top, which printed as loose strands. So did 0.45 mm break-away fins under the guide pins, one extrusion wide.

Untested: whether the 0.57 mm seat overhang prints clean, the 0.25 mm seat's hold on the switch, flap stiffness and tooth hold in PLA vs PETG, the 0.1 guide clearance on this printer, pocket depth feel.

## Soldering jig

`SolderJig` in `case/roamy-v4` (models `solder-jig` with the board mock-up, `solder-jig-print` to print), built in the module frame from the same numbers as the case, so connectors land where the case expects them. Replaces the CadQuery jig, which held the header pins at the misread 2.3 mm axis and had no stop along the pins.

Solder the front passives first, then lay the board front down in the pocket (SW5 and SW1 engraved at the matching ends, header side toward the labels) and solder the back.
- Sockets: nest floor in the board plane (the body rests on the board), 0.1 mm side fit, stop against the mouth at the 2.0 mm overhang. Push each socket against its stop and tack one pad.
- Headers: a cradle under the body tilted 8° about the inboard end of the pads, a slot under each pin (0.08 fit), and a stop touching the body's outer edge on its board side. Lay the header in, push it outward against the stop, tack. The foot then touches its pad at the inboard end with 0.32 mm under its other end; pin tips sit 1.85 mm lower than flat. The joint is a wedge fillet, which must not carry the connector's load: the floor's rear stop takes the slide-on push, its clamp pad keeps the body from tipping, and staking takes separation (key module, above).
- Front passive pockets 4.4 × 3.0 × 1.6 mm, room for the solder fillets (r1's 3.6 × 2.4 × 1.2 was too tight).
- Engraved "8° jig r4"; `Revision` in the package numbers every printed part.
- Every cut is open toward the component side, so the soldered board lifts straight out.

Checked on the meshes: no overlap with the populated board mock-up; lifting it along its normal touches nothing; moving it 0.05 mm against either stop or into the jig overlaps, so the stops and floors are in contact. Prints base down with no overhangs.

## Terminator module

`TerminatorModule` in `case/roamy-v4` (models `terminator` with its neighbour, `terminator-print`). The header side of a key module's outline, printed solid in one piece top-down, about 12 mm wide at the board plane; the joint's header side (`HeaderSideJoint`), and a lug at each column end of the free face.

Electrically it only ties the neighbour's DATA_IN to +3.3V, so that the neighbour's 74HC165 shifts in ones after its own byte and the MCU reads the end-of-chain sentinel ([[Electronics]]). Two headers sit where a key module's would: row 1 (+3.3V on its middle pin, board y 58.1) and row 2 (DATA on pin 3, board y 79.64). A wire joins those two pins' feet.

Assembly, by embed pause:
1. Solder the wire between the two feet: row 1 middle pin, row 2 pin 3 (the pin toward row 3). Insulated wire up to 1.3 mm.
2. Print; the slicer pauses at the height that `swift run` prints (7.40 mm today, on a layer boundary).
3. Drop both headers into their pockets, bodies down, pins out through the header face; lay the wire in its groove.
4. Resume. 1.5 mm of print closes over the pockets. The wire's groove stays open on the underside, since covering it would take a 26 mm bridge.

Checked on the meshes: the header side meets the JointSide contract; headers and wire fit their pockets and lie below the pause; the header pins sit in the neighbour's sockets like a key module's; no overlap with the neighbour, and slide-on touches only at the latch. The neighbour's socket mouths reach 0.17 mm past the seam, so the header face has reliefs there, as the key module's header wall has slots.

## MCU module

`MCUShell`, `MCUFloor` in `case/roamy-v4` (models `mcu-module` with the last key module, `mcu-module-print`). The last key module's neighbour: the joint's socket side (`SocketSideJoint`), no header side, so no pins stand exposed. Shell and screwed-on floor like the key module, 52.4 mm wide at the board plane.

- **Assembly**, into the upturned shell: the nice!view, then the socket board, then the battery, the nano, the switch and the button; the jack on its floor seat; close with the floor.
- **Board**: the socket board (a spare key module board with only its three sockets, soldered in the solder jig) sits exactly where a key module's board does, so the sockets meet the neighbour's headers. End ledges, the socket-side ledge tabs and floor pillars hold it as in the key module; on its bay side, where a key module has a wall, four board-side tabs (a ledge over its edge and a leg 0.2 mm beside it) stop it sliding into the bay, and wires from its pads and from the view cross the edge between them. The MCU board, later, keeps that socket edge.
- **Socket bodies**: clamp pads and rear stops on the floor, and staking for separation, as in the key module.
- **nice!view** over the socket board, header edge toward the nano, 0.2 mm above the board, under a window 0.4 mm past its active area with a 0.6 mm lip over the glass. It drops into its pocket from below before the board goes in; a wall hangs from the plate at its flex end and corner posts at its header end, so it stays put and its wires can leave toward the nano. The MCU module's plate is 1.4 mm, a whole number of layers, since it lies parallel to the top the shell prints on. To fit the view, its top lies 1.53 mm above the key modules' (10.48 mm thick, against 8.94); a local bezel would stand proud of the top, which the shell prints on.
- **Bay**: the battery (electrokit 41016063, 48 × 30 × 5, 0.5 mm slack each side and above) at the SW1 end between floor stops; the nice!nano at the SW5 end (`P.usbEnd`), port through the end wall with a recess for the plug's overmould. Everything that locates the parts sideways hangs from the plate, so they go into the upturned shell before the floor closes it: guides at the nano's sides that reach 0.6 mm down its board edge, a stop 0.1 mm behind it, a post 0.1 mm over its USB-C shell, a prop 0.1 mm over its tallest part under its middle so it lies level, and stops at the battery's ends; the socket board's edge stops the battery toward the board. The port's slot in the end wall is open down to the rim so the nano drops in; a tongue on the floor closes it to 0.1 mm under the port, short of the overmould's recess, and a 2 mm rib on the floor under the nano's middle (room for wires underneath) presses it against the post. Between them the battery jack (JST S2B-PH-K-S) on a seat, mating face toward the battery, its tails through the floor.
- **Free face**, between the lugs: the power switch (Alps SSSS811101, slider 0.5 mm proud) and the reset button (Panasonic EVQPUC02K, actuator 0.2 mm recessed), each in a pocket in the free wall's inner face, open toward the floor, which holds them. Behind each, a chamber hangs from the plate: a wall 0.1 mm behind the body takes the press, walls beyond its terminals stop it along the column, and its open corners let the wires out (the switch's back wall stops 0.5 mm above the rim, clear of its signal terminals). Their pegs sit in reliefs in the floor.
- **Screws**: the key module's four corner screws in the socket side's end walls, and two more in bosses at the free face's corners.
- **Lugs**: as on the terminator, their bottoms turned down by the joint angle to follow the leg.

Dimensions: nice!nano measured (18.15 × 33.40, 34.26 with the port; 2.75 tall, 3.30 at the port); nice!view, switch, button and jack from their vendors' drawings (nicekeyboards.com, typeractive.xyz datasheet links); battery from electrokit.com.

Checked on the meshes: the socket side meets the JointSide contract; shell, floor, socket board and parts don't overlap; joined to a key module, nothing overlaps and the header pins sit in the sockets; slide-on touches only at the latch; the socket board drops into the shell; the nano is held in every direction; the switch and the button are held when pressed and along the column; every part drops into the upturned shell; the floor's tongue holds the port from below; the sockets' clamp pads and rear stops meet the key module's checks; six screws clear the floor and bite into the shell; a USB-C plug's overmould clears the end wall. Both parts print without drooping overhangs (`tools/overhang.py`).

Untested: all of it on the printer.


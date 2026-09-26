# Glossary

One term per concept. Use the bold term in code, docs, commits and conversation; the terms under "not" are discouraged synonyms.

![Parts of the joint](images/joint-parts.svg)

## Modules

- **module**: One printed unit of the keyboard that couples to its neighbours side by side. Kinds: key module, MCU module, terminator module. Not: block, piece.
- **key module**: A module carrying one column of five keys: shell, floor and board. `KeyModule*` in `case/roamy-v4`. Not: column module, column (for the part).
- **column**: The line of keys that one key module carries, as a typing concept. The part is a key module.
- **neighbour**: The module on a module's header side. In a pair, A is the module carrying the guide pins and headers, and B is A's neighbour, carrying the guide holes, flaps and sockets that meet them. Not: next module, left/right module.
- **header side**, **socket side**: The two long sides of a module, named by the connector each carries. Not: left/right, male/female side.
- **SW1 end**, **SW5 end**: The two ends of a key module, named by the switch nearest each. **Column end** when either will do. Not: top/bottom end, front/back.

## Shell and floor

- **shell**: The top part of a module: plate, side walls and end walls, open at the bottom. Prints top-down. Not: case top, lid, top case.
- **floor**: The bottom part, screwed into the shell from below. Prints bottom-down. Not: bottom plate, base, lid.
- **board**: The key module's PCB. It drops into the upturned shell along its normal.
- **plate**: The 1.3 mm top of the shell that the switches clip into. Not: top plate (confused with the shell).
- **switch pocket**: The 15.3 mm recess in the plate's top that each switch's top housing sits in. **Switch opening**: the 13.7 mm hole through the plate at the pocket's floor.
- **side wall**: The long wall on the header side or socket side, 1.8 mm at the board plane. Its outer face is the **header face** or **socket face**.
- **end wall**: The 6.6 mm wall at each column end. It holds the guide hole and flap on the socket side, the guide pin's root on the header side, and two corner screws. Its outer face is the **end face**.
- **top**, **rim**, **bottom**: The three planes of a module's outline: the shell's upper surface, where shell meets floor, and the floor's underside. Each lies perpendicular to the bisector of the header and socket faces.
- **keystone**: The module's outline in section. The header and socket faces run radially to the joint centre, so neighbours' tops and rims meet.
- **corner screw**: One of four M2 screws holding the floor, in the end walls' corners.
- **ledge**: A strip on the shell's underside that stops the board from rising. **Ledge tab**: a short ledge between switch housings on the socket side.
- **locating tab**: A 1.0 mm rib on the floor that locates it in the shell's opening.
- **pillar**: A post on the floor that presses the board against the ledges.

## Connectors

- **header**: The male 3-pin connector on the board's back, header side. Soldered tilted 8° in the solder jig. Its **header pins** leave along the neighbour's board plane. Not: plug, male.
- **socket**: The female 3-pin connector on the board's back, socket side. Its **mouth** lies in the socket face, 2.0 mm past the board edge. Not: receptacle, female.
- **connector row**: One header-and-socket position along the column; three per module, at board y 58.1, 77.1 and 115.1 mm.
- **pin axis**: The line the header pins and socket bores share, 1.25 mm below the board.

## Joint

- **joint**: Everything that couples a module to its neighbour: the keystone faces, the connectors, the guide pins and the latches.
- **seam**: The plane where a module's header face meets its neighbour's socket face.
- **joint angle**: The 8° between neighbouring boards. **Joint centre**: the axis 149 mm below the board that the neighbour's pose rotates about.
- **slide-on**: The straight move that couples B onto A, along B's board plane (8° down in A's frame). Its last 5.6 mm is **insertion** of the header pins into the sockets. Not: snap on, push in.
- **guide pin**: The gabled bar that protrudes from A's header face at each column end and enters B's guide hole before the header pins reach the sockets; it aligns the pair across the column and vertically. Its tip is the **taper**. Not: peg, dowel, prong, tongue (v3's T rail).
- **guide hole**: The hole in B's socket-side end wall that takes the guide pin.
- **latch**: The flap, tooth and notch together; it holds a pair against pulling apart. There are two per seam, one at each column end.
- **flap**: The outer 1.0 mm skin of B's end wall over the guide hole, free to bend outward. It is cut free by the **slit** behind it (0.3 mm, above and below the guide hole, through top and rim) and fixed at its **hinge** near the socket face. Its other end is the **free edge**, with the **flap slot** (a fingernail gap) beyond it. Not: pull tab, tab (tabs are the floor's locating tabs and the ledge tabs), clip, lever.
- **tooth**: The bump on the flap's inner face that drops into the notch. Its **lead-in face** points toward the seam; the guide pin pushes it outward during slide-on. Its **catch face** points away from the seam; it bears on the barb when the pair is pulled apart, and its angle is `toothCatchAngle`. Not: hook, catch (for the whole tooth).
- **notch**: The recess in the guide pin's outer face that the tooth drops into.
- **barb**: The ridge of the guide pin between the notch and the taper. Its notch-side face meets the tooth's catch face; the barb and the tooth are the two halves of the latch's hold. Not: hook, catch, pin tip (the tip is the taper).
- **release**: Moving the tooth out of the notch.
- **separation**: Moving B off A, the reverse of slide-on. **Ejection**: separation driven by a mechanism rather than by hand.

## Printing

- **fin**: A break-away web under each guide pin in the print pose, so the pin doesn't droop. Snap it off after printing.
- **membrane**: A one-layer skin over each switch opening in the print pose, so the pocket rim prints on it. Cut it out after printing.
- **revision**: The `r<n>` engraved on each printed part, from `Revision` in `Parameters.swift`.

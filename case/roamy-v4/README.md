# roamy-v4 case

Cadova (Swift) model of the v4 case: one printable module per `Model`, parameters in `Sources/roamy-v4/Parameters.swift`, board and connector numbers from `electronics/Electronics.md`.

```
swift run            # writes build/roamy-v4/*.3mf and build/roamy-v4/check/*.stl
```

Open the 3MF files in Cadova Viewer (https://github.com/tomasf/CadovaViewer); it reloads when `swift run` rewrites them. `key-module` shows shell, floor and a board mock-up in place; `key-module-print` has the shell plate-down and the floor beside it; `three-modules` has two neighbours attached at the joint angle. The startup line prints the joint centre, skew and heights. The STL copies exist for headless checks (sections and point probes) and are not for printing.

Joint hardware (guide pins, guide holes, latches) sits behind `JointSide` in `JointSide.swift`; a module hosts a header side, a socket side or both, and keeps its own features out of the joint side's reserved blocks and keep-out.

Mesh checks: `python3 tools/check.py` after `swift run`. It compares the board against KiCad's own 3D export (a mirrored model fails it), and checks the joined modules, the board drop-in, the slide-on, the jig fit and the `JointSide` contract. Run it before printing anything.

Printability: `python3 tools/overhang.py` slices the print-pose STLs at 0.2 mm and lists what the layer below doesn't carry. Bridges pass, and so do overhangs up to about 1.5 extrusion widths; anything that droops farther fails unless it is in the painted-support zone. Needs `trimesh` and `shapely`.

Headless renders (cloud sessions, CI): `python3 tools/render.py [view ...]` writes PNGs of the check STLs to `build/roamy-v4/render/`; views are listed in the script. Needs `trimesh` (pip) and `f3d`, `xvfb` (apt); `scripts/setup-cloud.sh` installs them, Swift and KiCad in a fresh cloud session.

Each printed part carries its revision (`Revision` in `Parameters.swift`: shell on the SW1 end wall's inner face, floor on its inside face, jig and coupon on top). Bump a part's number whenever its geometry changes.

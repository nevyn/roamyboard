# roamy-v4 case

Cadova (Swift) model of the v4 case: one printable module per `Model`, parameters in `Sources/roamy-v4/Parameters.swift`, board and connector numbers from `electronics/Electronics.md`.

```
swift run            # writes build/roamy-v4/*.3mf and build/roamy-v4/check/*.stl
```

Open the 3MF files in Cadova Viewer (https://github.com/tomasf/CadovaViewer); it reloads when `swift run` rewrites them. `key-module` shows shell, floor and a board mock-up in place; `key-module-print` has the shell plate-down and the floor beside it; `three-modules` has two neighbours attached at the joint angle. The startup line prints the joint centre, skew and heights. The STL copies exist for headless checks (sections and point probes) and are not for printing.

Headless renders (cloud sessions, CI): `python3 tools/render.py [view ...]` writes PNGs of the check STLs to `build/roamy-v4/render/`; views are listed in the script. Needs `trimesh` (pip) and `f3d`, `xvfb` (apt); `scripts/setup-cloud.sh` installs them, Swift and KiCad in a fresh cloud session.

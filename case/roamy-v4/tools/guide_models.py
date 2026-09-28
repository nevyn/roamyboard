"""Models for the assembly guide: python3 tools/guide_models.py (after `swift run`).

Converts the 3MF files in build/roamy-v4 into GLB models in docs/assembly/models, one node per part and colour
(`<part>#<rrggbb[aa]>`), so the guide's viewer can colour, hide and explode them. The GLBs are in Git LFS.
`data.js` carries the joint's neighbour pose, the terminator's pause height and the part revisions. The MCU module's parts are one object
in the 3MF; they are split into bodies and named by matching the check STLs. Print models get the painted-support
zones and the terminator's embedded headers from the check STLs. Needs trimesh and numpy.
"""
import json, pathlib, re, sys, zipfile
import xml.etree.ElementTree as ET
import numpy as np, trimesh

ROOT = pathlib.Path(__file__).resolve().parents[3]
BUILD, CHECK, OUT = ROOT / "build/roamy-v4", ROOT / "build/roamy-v4/check", ROOT / "docs/assembly/models"
NS = {"c": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02",
      "m": "http://schemas.microsoft.com/3dmanufacturing/material/2015/02"}

# model: extra check STLs added as parts, name -> (stl, colour)
EXTRAS = {
    "key-module-print": {"Painted support": ("shell-print-painted-support", "ff3b30")},
    "terminator-print": {"Painted support": ("terminator-print-painted-support", "ff3b30"),
                         "Headers": ("terminator-embedded-print", "ffa500")},
}
MODELS = ["key-module", "key-module-print", "mcu-module", "mcu-module-print", "terminator", "terminator-print",
          "solder-jig", "solder-jig-print", "choc-cutout-coupon"]
MCU_PARTS = ["nano", "view", "switch", "reset"]   # check STLs mcu-<name>; the rest of "Parts" is the battery and jack


def objects(path):
    """(name, vertices, faces, per-face hex colour) for every mesh object in a 3MF."""
    z = zipfile.ZipFile(path)
    for member in z.namelist():
        if not member.endswith(".model"):
            continue
        root = ET.fromstring(z.read(member))
        groups = {g.get("id"): [c.get("color")[1:].lower() for c in g.findall("m:color", NS)]
                  for g in root.iter(f"{{{NS['m']}}}colorgroup")}
        for obj in root.iter(f"{{{NS['c']}}}object"):
            mesh = obj.find("c:mesh", NS)
            if mesh is None:
                continue
            v = np.array([[float(e.get(k)) for k in "xyz"] for e in mesh.find("c:vertices", NS)])
            tris = mesh.find("c:triangles", NS)
            f = np.array([[int(t.get(k)) for k in ("v1", "v2", "v3")] for t in tris])
            default = groups.get(obj.get("pid"), [None])[int(obj.get("pindex") or 0)] if obj.get("pid") else None
            colours = []
            for t in tris:
                pid = t.get("pid")
                colours.append(groups[pid][int(t.get("p1"))] if pid in groups and t.get("p1") else default)
            name = obj.get("name") or pathlib.Path(member).stem
            yield name, v, f, colours


def neighbour_transform():
    """The joint's neighbour pose, recovered from the shell and its neighbour-placed copy (Kabsch fit)."""
    a = trimesh.load(CHECK / "shell.stl", process=False).vertices
    b = trimesh.load(CHECK / "right-shell.stl", process=False).vertices
    ca, cb = a.mean(axis=0), b.mean(axis=0)
    u, _, vt = np.linalg.svd((a - ca).T @ (b - cb))
    d = np.diag([1, 1, np.sign(np.linalg.det(vt.T @ u.T))])
    r = vt.T @ d @ u.T
    t = np.eye(4)
    t[:3, :3], t[:3, 3] = r, cb - r @ ca
    if np.abs(trimesh.transform_points(a, t) - b).max() > 1e-3:
        sys.exit("right-shell.stl is not a rigid copy of shell.stl")
    return t


def split_mcu_parts(mesh):
    """Names the bodies of the MCU module's "Parts" object by their overlap with the check STLs."""
    refs = {n: trimesh.load(CHECK / f"mcu-{n}.stl") for n in MCU_PARTS}
    back = np.linalg.inv(neighbour_transform())   # the 3MF places the MCU module as the key module's neighbour
    named, rest = {}, []
    for body in mesh.split(only_watertight=False):
        c = trimesh.transform_points([body.bounds.mean(axis=0)], back)[0]
        hit = [n for n, r in refs.items() if np.all(r.bounds[0] - 0.5 <= c) and np.all(c <= r.bounds[1] + 0.5)]
        if hit:
            named.setdefault(hit[0], []).append(body)
        else:
            rest.append(body)
    rest.sort(key=lambda b: -b.volume)   # battery, then the jack
    if len(rest) < 2 or set(named) != set(MCU_PARTS):
        sys.exit(f"MCU parts: matched {sorted(named)}, {len(rest)} unmatched bodies; expected {MCU_PARTS} + battery, jack")
    out = {f"MCU {n}": trimesh.util.concatenate(b) for n, b in named.items()}
    out["MCU battery"] = rest[0]
    out["MCU jack"] = trimesh.util.concatenate(rest[1:])
    return out


def convert(model):
    scene = trimesh.Scene()
    def add(name, mesh, colour):
        mesh.visual = trimesh.visual.TextureVisuals(material=trimesh.visual.material.PBRMaterial(
            name=colour, baseColorFactor=[int(colour[i:i + 2], 16) for i in (0, 2, 4)] + [int(colour[6:8] or "ff", 16)]))
        scene.add_geometry(mesh, node_name=f"{name}#{colour}", geom_name=f"{name}#{colour}")
    for name, v, f, colours in objects(BUILD / f"{model}.3mf"):
        colours = np.array([c or "c8b48c" for c in colours])
        for colour in dict.fromkeys(colours):
            sub = trimesh.Trimesh(v, f[colours == colour])
            sub.remove_unreferenced_vertices()
            if name == "Parts" and model == "mcu-module":
                for part, mesh in split_mcu_parts(sub).items():
                    add(part, mesh, colour)
            else:
                add(name, sub, colour)
    for name, (stl, colour) in EXTRAS.get(model, {}).items():
        add(name, trimesh.load(CHECK / f"{stl}.stl"), colour)
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"{model}.glb"
    path.write_bytes(scene.export(file_type="glb"))
    print(f"{path.relative_to(ROOT)}: {', '.join(sorted(scene.graph.nodes_geometry))}")


if __name__ == "__main__":
    for m in sys.argv[1:] or MODELS:
        convert(m)
    params = (ROOT / "case/roamy-v4/Sources/roamy-v4/Parameters.swift").read_text()
    revisions = dict(re.findall(r"static let (\w+) = (\d+)", params.split("enum Revision")[1].split("}")[0]))
    data = {"neighbour": np.round(neighbour_transform(), 6).T.flatten().tolist(),   # column-major, as three.js
            "terminatorPause": float((CHECK / "terminator-pause.txt").read_text()),
            "revisions": {k: int(v) for k, v in revisions.items()}}
    (OUT / "data.js").write_text(f"window.ROAMY_DATA = {json.dumps(data)};\n")

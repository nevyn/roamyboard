"""Headless renders of the check STLs: python3 tools/render.py [view ...] (after `swift run`).

Composes coloured GLB scenes with trimesh and renders them with F3D under Xvfb into build/roamy-v4/render/.
Needs: pip install trimesh numpy; apt install f3d xvfb.
"""
import subprocess, sys, pathlib, trimesh

ROOT = pathlib.Path(__file__).resolve().parents[3]
CHECK, OUT = ROOT / "build/roamy-v4/check", ROOT / "build/roamy-v4/render"
COLOURS = {"shell": (210, 190, 140), "right-shell": (230, 150, 80), "floor": (90, 130, 210), "right-floor": (90, 130, 210),
           "board": (40, 140, 70), "bare-board": (40, 140, 70), "right-board": (40, 140, 70), "shell-print": (210, 190, 140), "jig": (200, 200, 205), "jig-print": (200, 200, 205)}

# name: parts, camera direction (from camera toward the model), up, optional y-clip (keep y below)
VIEWS = {
    "pair":        (["shell", "floor", "board", "right-shell", "right-floor", "right-board"], "0.4,1,-0.5", "+Z", None),
    "end":         (["shell", "floor", "right-shell", "right-floor"], "0.15,1,-0.25", "+Z", None),
    "end-close":   (["shell", "floor", "right-shell", "right-floor"], "0.3,1,-0.6", "+Z", 14),
    "underside":   (["shell", "bare-board"], "0.3,0.6,1", "+Z", None),
    "under-end":   (["shell", "bare-board", "right-shell"], "0.2,0.8,1", "+Z", 16),
    "top":         (["shell", "right-shell"], "0,0.3,-1", "+Y", None),
    "print":       (["shell-print"], "0.4,1,-0.6", "+Z", None),
    "jig":         (["jig-print"], "0.35,0.8,-1", "+Z", None),
    "jig-header":  (["jig", "bare-board"], "0.5,0.6,1", "+Z", 30),
}

def render(name):
    parts, direction, up, clip = VIEWS[name]
    scene = trimesh.Scene()
    for p in parts:
        m = trimesh.load(CHECK / f"{p}.stl")
        if clip is not None:
            m = m.slice_plane([0, clip, 0], [0, -1, 0], cap=True)
        m.visual.face_colors = (*COLOURS[p], 255)
        scene.add_geometry(m, node_name=p)
    OUT.mkdir(parents=True, exist_ok=True)
    glb = OUT / f"{name}.glb"
    scene.export(glb)
    subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1600x1100x24", "f3d", str(glb), f"--output={OUT / name}.png",
                    "--resolution=1400,1000", f"--camera-direction={direction}", f"--up={up}",
                    "--anti-aliasing", "--ambient-occlusion"], check=True)
    print(OUT / f"{name}.png")

if __name__ == "__main__":
    for v in sys.argv[1:] or VIEWS:
        render(v)

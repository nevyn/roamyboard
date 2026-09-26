#!/usr/bin/env bash
# Installs the tools this repo needs in a fresh Ubuntu 24.04 cloud session (run as root):
# Swift (for case/roamy-v4), KiCad 10 (kicad-cli), F3D + Xvfb (headless renders), and the Python
# packages for mesh checks. Idempotent: steps already done are skipped.
#
#   scripts/setup-cloud.sh
set -euo pipefail

SWIFT_PREFIX=${SWIFT_PREFIX:-/opt/swift}
export DEBIAN_FRONTEND=noninteractive
step() { printf '\n== %s\n' "$*"; }

# swift.org serves some files gzip-encoded; --compressed stores them decoded.
fetch() { curl -sSfL --compressed "$@"; }

step "apt packages: KiCad 10, F3D, Xvfb"
if ! command -v kicad-cli >/dev/null || ! kicad-cli version | grep -q '^10\.'; then
    # add-apt-repository fails here (python3 is not the system Python), so add the PPA by hand
    fetch "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x245D5502FAD7A805" | gpg --dearmor --yes -o /usr/share/keyrings/kicad.gpg
    echo "deb [signed-by=/usr/share/keyrings/kicad.gpg] https://ppa.launchpadcontent.net/kicad/kicad-10.0-releases/ubuntu noble main" \
        > /etc/apt/sources.list.d/kicad.list
    apt-get update -q
    apt-get install -y -q --no-install-recommends kicad
fi
if ! command -v f3d >/dev/null || ! command -v xvfb-run >/dev/null; then
    apt-get update -q
    apt-get install -y -q --no-install-recommends f3d xvfb xauth libgl1-mesa-dri
fi

step "Swift"
if ! command -v swift >/dev/null; then
    tag=$(fetch https://www.swift.org/api/v1/install/releases.json | python3 -c 'import json,sys; print(json.load(sys.stdin)[-1]["tag"])')
    file="$tag-ubuntu24.04.tar.gz"
    url="https://download.swift.org/${tag,,}/ubuntu2404/$tag/$file"
    tmp=$(mktemp -d)
    fetch https://www.swift.org/keys/all-keys.asc | gpg --import 2>/dev/null
    fetch -o "$tmp/$file" "$url"
    fetch -o "$tmp/$file.sig" "$url.sig"
    gpg --verify "$tmp/$file.sig" "$tmp/$file"
    mkdir -p "$SWIFT_PREFIX"
    tar xzf "$tmp/$file" -C "$SWIFT_PREFIX" --strip-components=1
    rm -rf "$tmp"
    ln -sf "$SWIFT_PREFIX"/usr/bin/* /usr/local/bin/
fi

step "Python packages"
# Debian's python3-pil is built for the system Python; install a matching Pillow for this one
pip install -q --root-user-action=ignore --ignore-installed pillow
pip install -q --root-user-action=ignore numpy scipy trimesh manifold3d matplotlib shapely networkx rtree

step "Versions"
swift --version 2>&1 | head -1
echo "KiCad $(kicad-cli version)"
f3d --version | head -1
python3 -c 'import trimesh, manifold3d; print("trimesh", trimesh.__version__)'

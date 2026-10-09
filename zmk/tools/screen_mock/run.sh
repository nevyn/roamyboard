#!/bin/sh
# Renders the status screen on the host with LVGL from a ZMK west workspace.
# Usage: zmk/tools/screen_mock/run.sh [WEST_WORKSPACE [OUTPUT_DIR]]
# Writes to OUTPUT_DIR, every scene in the order that mock.c renders them:
#   states.png          the panel's own image, line 1 at the top, one scene per row
#   states-on-leg.png   what a person sees on the mounted nice!view, side by side
#   screen.png          the central in use, as a person sees it
#   screen-panel.png    the same, as the panel's own image
# and two PGMs per scene. ROTATE_180=0 renders without CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
zmk_dir=$(cd "$here/../.." && pwd)
ws=${1:-$HOME/Library/Caches/roamyboard-west}
out=${2:-/tmp/roamy-screen}
lvgl="$ws/modules/lib/gui/lvgl"
zephyr="$ws/zephyr/include"
zmk_app="$ws/zmk/app/include"
for dir in "$lvgl/src" "$zephyr" "$zmk_app"; do
    [ -d "$dir" ] || { echo "Missing $dir: pass a ZMK west workspace" >&2; exit 1; }
done

build="$out/build"
mkdir -p "$build/lvgl"
# LVGL is compiled once per output directory.
if [ ! -f "$build/liblvgl.a" ]; then
    export CC="${CC:-cc}" here lvgl build
    find "$lvgl/src" -name '*.c' | xargs -n 1 -P 8 sh -c '
        obj="$build/lvgl/$(echo "$1" | sed "s|^$lvgl/||; s|/|_|g").o"
        $CC -c -O1 -w -DLV_CONF_INCLUDE_SIMPLE -I"$here" -I"$lvgl" "$1" -o "$obj"' sh
    ar rcs "$build/liblvgl.a" "$build"/lvgl/*.o
fi

display="$zmk_dir/src/display"
for role in central peripheral; do
    defines=""
    [ "${ROTATE_180:-1}" = 1 ] && defines="-DCONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180=1"
    [ "$role" = peripheral ] && defines="$defines -DCONFIG_ZMK_SPLIT=1"
    ${CC:-cc} -std=c11 -Wall -Wextra -Werror -Wno-missing-field-initializers $defines \
        -DLV_CONF_INCLUDE_SIMPLE -I"$here" -I"$lvgl" -I"$display" -I"$zmk_dir/include" \
        -I"$zephyr" -I"$zmk_app" \
        "$here/mock.c" "$display/screen.c" "$display/util.c" "$display/bolt.c" \
        "$display/cat.c" "$display/cat_frames.c" "$build/liblvgl.a" -lm -o "$build/mock-$role"
    "$build/mock-$role" "$out"
done
python3 "$here/compose.py" "$out"

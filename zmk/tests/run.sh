#!/bin/sh
# Builds and runs the host tests for the pure logic with the system C compiler.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
kscan="$here/../drivers/kscan"
display="$here/../src/display"
pda="$here/../src/pda"
out=$(mktemp -d "${TMPDIR:-/tmp}/roamyboard-tests.XXXXXX")
trap 'rm -f "$out/chain_test" "$out/cat_test" "$out/pda_test" "$out/pda_shell_test" "$out/pda_clock_test" "$out/pda_apps_test" && rmdir "$out"' EXIT
cflags="-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined"
${CC:-cc} $cflags -I"$kscan" "$here/chain_test.c" "$kscan/chain.c" -o "$out/chain_test"
${CC:-cc} $cflags -I"$display" "$here/cat_test.c" "$display/cat.c" -o "$out/cat_test"
${CC:-cc} $cflags -I"$pda" "$here/pda_test.c" "$pda/keys.c" "$pda/note.c" -o "$out/pda_test"
apps="$pda/shell.c $pda/icons.c $pda/apps.c $pda/app_clock.c $pda/app_timer.c $pda/app_calc.c $pda/app_dice.c $pda/app_kana.c $pda/game_breakout.c $pda/game_tetris.c $pda/game_snake.c
    $pda/game_words.c $pda/game_rain.c $pda/words.c $pda/gfx.c $pda/note.c $pda/keys.c $pda/clock.c"
# shellcheck disable=SC2086
${CC:-cc} $cflags -I"$pda" -I"$here/../include" "$here/pda_shell_test.c" "$here/pda_text_stub.c" \
    $apps -lm -o "$out/pda_shell_test"
# shellcheck disable=SC2086
${CC:-cc} $cflags -I"$pda" -I"$here/../include" "$here/pda_apps_test.c" "$here/pda_text_stub.c" \
    $apps -lm -o "$out/pda_apps_test"
${CC:-cc} $cflags -I"$pda" "$here/pda_clock_test.c" "$pda/clock.c" -o "$out/pda_clock_test"
"$out/chain_test"
"$out/cat_test"
"$out/pda_test"
"$out/pda_shell_test"
"$out/pda_clock_test"
"$out/pda_apps_test"

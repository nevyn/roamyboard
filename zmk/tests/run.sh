#!/bin/sh
# Builds and runs the host tests for the pure logic with the system C compiler.
# --write-log-fixture rewrites roamy-log's frame fixture from the C encoder instead of checking it.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
kscan="$here/../drivers/kscan"
input="$here/../drivers/input"
display="$here/../src/display"
pda="$here/../src/pda"
midi="$here/../src/midi"
log="$here/../src/log"
fixture="$here/../tools/log/Tests/RoamyLogTests/Fixtures/log_frames.txt"
out=$(mktemp -d "${TMPDIR:-/tmp}/roamyboard-tests.XXXXXX")
trap 'rm -f "$out/chain_test" "$out/cat_test" "$out/pda_test" "$out/pda_shell_test" "$out/pda_clock_test" "$out/pda_apps_test" "$out/midi_state_test" "$out/iqs5xx_test" "$out/log_test" "$out/log_frames.txt" "$out/anarch.o" && rmdir "$out"' EXIT
cflags="-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined"
${CC:-cc} $cflags -I"$kscan" "$here/chain_test.c" "$kscan/chain.c" -o "$out/chain_test"
${CC:-cc} $cflags -I"$input" "$here/iqs5xx_test.c" "$input/iqs5xx_logic.c" -o "$out/iqs5xx_test"
${CC:-cc} $cflags -I"$display" "$here/cat_test.c" "$display/cat.c" -o "$out/cat_test"
${CC:-cc} $cflags -I"$pda" "$here/pda_test.c" "$pda/keys.c" "$pda/note.c" -o "$out/pda_test"
# Anarch's SFG_getItemSprite() forms an out-of-range pointer before it picks another.
${CC:-cc} $cflags -fno-sanitize=bounds -I"$pda" -c "$pda/anarch.c" -o "$out/anarch.o"
apps="$out/anarch.o $pda/shell.c $pda/midi_view.c $pda/icons.c $pda/apps.c $pda/app_clock.c $pda/app_timer.c $pda/app_calc.c $pda/app_dice.c $pda/app_kana.c $pda/app_cat.c $pda/cat_sprites.c $pda/game_breakout.c $pda/game_tetris.c $pda/game_snake.c
    $pda/game_words.c $pda/game_rain.c $pda/game_anarch.c $pda/anarch_inline.c $pda/words.c $pda/gfx.c $pda/note.c $pda/docs.c $pda/help.c $pda/keys.c $pda/clock.c"
# shellcheck disable=SC2086
${CC:-cc} $cflags -I"$pda" -I"$here/../include" "$here/pda_shell_test.c" "$here/pda_text_stub.c" \
    $apps -lm -o "$out/pda_shell_test"
# shellcheck disable=SC2086
${CC:-cc} $cflags -I"$pda" -I"$here/../include" "$here/pda_apps_test.c" "$here/pda_text_stub.c" \
    $apps -lm -o "$out/pda_apps_test"
${CC:-cc} $cflags -I"$pda" "$here/pda_clock_test.c" "$pda/clock.c" -o "$out/pda_clock_test"
${CC:-cc} $cflags -I"$midi" -I"$here/../include" "$here/midi_state_test.c" "$midi/state.c" -o "$out/midi_state_test"
${CC:-cc} $cflags -I"$log" "$here/log_test.c" "$log/ring.c" "$log/frame.c" "$log/command.c" -o "$out/log_test"
"$out/chain_test"
"$out/iqs5xx_test"
"$out/cat_test"
"$out/pda_test"
"$out/pda_shell_test"
"$out/pda_clock_test"
"$out/pda_apps_test"
"$out/midi_state_test"
"$out/log_test"
if [ "${1:-}" = --write-log-fixture ]; then
    "$out/log_test" --fixture > "$fixture"
    echo "run.sh: wrote $fixture"
    exit 0
fi
"$out/log_test" --fixture > "$out/log_frames.txt"
if ! cmp -s "$out/log_frames.txt" "$fixture"; then
    diff "$fixture" "$out/log_frames.txt" >&2 || true
    echo "run.sh: $fixture differs from what the C encoder writes now. If the frame format changed on purpose, run zmk/tests/run.sh --write-log-fixture and update roamy-log." >&2
    exit 1
fi

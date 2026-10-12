#!/bin/sh
# Builds and runs the host tests for the pure logic with the system C compiler.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
kscan="$here/../drivers/kscan"
display="$here/../src/display"
pda="$here/../src/pda"
out=$(mktemp -d "${TMPDIR:-/tmp}/roamyboard-tests.XXXXXX")
trap 'rm -f "$out/chain_test" "$out/cat_test" "$out/pda_test" "$out/pda_shell_test" && rmdir "$out"' EXIT
cflags="-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined"
${CC:-cc} $cflags -I"$kscan" "$here/chain_test.c" "$kscan/chain.c" -o "$out/chain_test"
${CC:-cc} $cflags -I"$display" "$here/cat_test.c" "$display/cat.c" -o "$out/cat_test"
${CC:-cc} $cflags -I"$pda" "$here/pda_test.c" "$pda/keys.c" "$pda/note.c" -o "$out/pda_test"
${CC:-cc} $cflags -I"$pda" -I"$here/../include" "$here/pda_shell_test.c" "$pda/shell.c" "$pda/apps.c" \
    "$pda/gfx.c" "$pda/note.c" "$pda/keys.c" -o "$out/pda_shell_test"
"$out/chain_test"
"$out/cat_test"
"$out/pda_test"
"$out/pda_shell_test"

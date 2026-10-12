#!/bin/sh
# Builds and runs the host tests for the pure logic with the system C compiler.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
kscan="$here/../drivers/kscan"
display="$here/../src/display"
out=$(mktemp -d "${TMPDIR:-/tmp}/roamyboard-tests.XXXXXX")
trap 'rm -f "$out/chain_test" "$out/cat_test" && rmdir "$out"' EXIT
cflags="-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined"
${CC:-cc} $cflags -I"$kscan" "$here/chain_test.c" "$kscan/chain.c" -o "$out/chain_test"
${CC:-cc} $cflags -I"$display" "$here/cat_test.c" "$display/cat.c" -o "$out/cat_test"
"$out/chain_test"
"$out/cat_test"

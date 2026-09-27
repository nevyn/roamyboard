#!/bin/sh
# Builds and runs the host tests for the chain logic with the system C compiler.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
kscan="$here/../drivers/kscan"
out=$(mktemp -d "${TMPDIR:-/tmp}/roamyboard-tests.XXXXXX")
trap 'rm -f "$out/chain_test" && rmdir "$out"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I"$kscan" "$here/chain_test.c" "$kscan/chain.c" -o "$out/chain_test"
"$out/chain_test"

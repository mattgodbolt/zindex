#!/bin/bash
# End-to-end check that zq accepts the same file: URI for --index-file that
# zindex does. Usage: uri_index_e2e.sh <zindex> <zq>
set -euo pipefail

ZINDEX=$(realpath "$1")
ZQ=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

printf 'k1 v\nk2 w\n' | gzip >u.gz
"$ZINDEX" --regex '(k[0-9])' --index-file "file:$PWD/u.zindex" u.gz
test -f u.zindex

actual=$("$ZQ" u.gz --index-file "file:$PWD/u.zindex" k2)
if [ "$actual" != "k2 w" ]; then
    echo "FAIL: expected 'k2 w' got '$actual'"
    exit 1
fi

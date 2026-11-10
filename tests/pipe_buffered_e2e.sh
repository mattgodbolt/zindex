#!/bin/bash
# End-to-end check that `zindex --pipe` works with a command that buffers all
# its output until EOF, which used to deadlock (issue #65).
# Usage: pipe_buffered_e2e.sh <zindex> <zq>
set -euo pipefail

ZINDEX=$(realpath "$1")
ZQ=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

# Enough input to overflow the pipe buffers many times over.
python3 -I -c "
for i in range(1,30001):
    print('row\tid%d\tpadding %s' % (i, 'x' * 60))" | gzip >data.gz

# awk only writes once it has read everything, whatever stdbuf says.
BUFFERING="awk -F'\t' '{ l[NR] = \$2 } END { for (i = 1; i <= NR; i++) print l[i] }'"

"$ZINDEX" --pipe "$BUFFERING" data.gz
for n in 1 777 30000; do
    actual=$("$ZQ" data.gz "id$n")
    expected=$(gzip -dc data.gz | sed -n "${n}p")
    if [ "$expected" != "$actual" ]; then
        echo "FAIL: id$n: expected '$expected' got '$actual'"
        exit 1
    fi
done

# --sparseLineOffsets decides which lines to keep once all keys are known.
"$ZINDEX" -o --pipe "$BUFFERING" data.gz
if [ "$("$ZQ" data.gz id5)" != "$(gzip -dc data.gz | sed -n 5p)" ]; then
    echo "FAIL: -o with a buffering command"
    exit 1
fi

# --sparse cannot wait for a buffering command, but a prompt one still works.
PROMPT="awk -F'\t' '{ print \$2; fflush() }'"
"$ZINDEX" -s --pipe "$PROMPT" data.gz
if [ "$("$ZQ" data.gz id5)" != "$(gzip -dc data.gz | sed -n 5p)" ]; then
    echo "FAIL: -s with a prompt command"
    exit 1
fi
# The same, configured by a config file.
python3 -I -c "
import json, sys
print(json.dumps({'indexes': [{'type': 'pipe', 'command': sys.argv[1],
                               'delimiter': ' '}]}))" "$BUFFERING" >pipe.json
"$ZINDEX" --config pipe.json data.gz
if [ "$("$ZQ" data.gz id777)" != "$(gzip -dc data.gz | sed -n 777p)" ]; then
    echo "FAIL: --config pipe with a buffering command"
    exit 1
fi

# An error in a key must name the line it came from, however late the command's
# reply to that line arrives.
expect_error() {
    local expected=$1
    shift
    local out
    if out=$("$ZINDEX" "$@" 2>&1); then
        echo "FAIL: expected an error from $*"
        exit 1
    fi
    if ! grep -qF -- "$expected" <<<"$out"; then
        echo "FAIL: expected '$expected' in: $out"
        exit 1
    fi
}
python3 -I -c "
for i in range(1,20001):
    print('x' if i == 5000 else i)" | gzip >numeric.gz
printf '1 x 3\n' | gzip >numeric3.gz
printf 'k1\nk2\nk1\nk3\n' | gzip >dup.gz
BUFFER_ALL="awk '{ l[NR] = \$0 } END { for (i = 1; i <= NR; i++) print l[i] }'"
for pipe in "cat" "$BUFFER_ALL"; do
    expect_error "Failed to index line 5000: 'x' - Non-numeric" \
        -n --pipe "$pipe" numeric.gz
    expect_error "Failed to index line 1: '1 x 3' - Non-numeric" \
        -n --pipe "$pipe" numeric3.gz
    expect_error "Failed to index line 3: 'k1' - " \
        --unique --pipe "$pipe" dup.gz
done
echo "OK"

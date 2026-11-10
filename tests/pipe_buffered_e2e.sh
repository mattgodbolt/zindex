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
echo "OK"

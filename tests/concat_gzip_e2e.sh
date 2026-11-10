#!/bin/bash
# Differential check: files made of concatenated gzip members, split at random
# places (so lines often span a member boundary), must give the same lines from
# `zq -l` as `gzip -dc | sed -n`, with and without -o (sparse line offsets).
# Usage: concat_gzip_e2e.sh <zindex> <zq>
set -euo pipefail

ZINDEX=$(realpath "$1")
ZQ=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

# Not every platform has timeout(1); it only guards against a hang.
if command -v timeout >/dev/null 2>&1; then
    guard() { timeout 20 "$@"; }
else
    guard() { "$@"; }
fi

for seed in 1 2 3 4 5 6; do
    # Lines are never empty and there is no empty member mid-file: those are
    # separate known problems. An empty member at the very end is fine.
    python3 -I -c "
import random, subprocess, sys
rnd = random.Random($seed)
lines = [''.join(rnd.choice('abcdefgh') for _ in range(rnd.randint(1, 300)))
         for _ in range(rnd.randint(150, 400))]
for i in range(0, len(lines), rnd.randint(7, 40)):
    lines[i] = 'k' + lines[i]
data = ('\n'.join(lines) + '\n').encode()
if rnd.random() < 0.5:
    data = data[:-1]
cuts = sorted(rnd.sample(range(1, len(data)), rnd.randint(2, 15)))
out = open('f$seed.gz', 'wb')
for a, b in zip([0] + cuts, cuts + [len(data)]):
    out.write(subprocess.run(['gzip', '-c'], input=data[a:b],
                             stdout=subprocess.PIPE, check=True).stdout)
"
    if [ $((seed % 2)) -eq 0 ]; then
        gzip -c </dev/null >>"f$seed.gz"
    fi
    gzip -dc "f$seed.gz" >"f$seed.txt"
    # grep -c '' also counts a final line with no newline.
    total=$(grep -c '' "f$seed.txt")
    # Spot-check lines around a spread of positions rather than all of them.
    lines="$(seq 1 7 "$total") $((total - 1)) $total"

    for flags in "" "-o"; do
        # shellcheck disable=SC2086
        "$ZINDEX" $flags --checkpoint-every 2048 --regex '^(k[a-h]*)' \
            "f$seed.gz" >/dev/null 2>&1
        for n in $lines; do
            expected=$(sed -n "${n}p" "f$seed.txt")
            if ! actual=$(guard "$ZQ" "f$seed.gz" -l "$n"); then
                echo "FAIL: seed $seed flags '$flags': zq failed on line $n"
                exit 1
            fi
            if [ "$expected" != "$actual" ]; then
                echo "FAIL: seed $seed flags '$flags' line $n"
                exit 1
            fi
        done
        # Several lines in one request, ascending then descending (the former
        # reuses the previous lookup's decompressor, which may need to skip
        # across a member boundary).
        for order in "sort -nu" "sort -rnu"; do
            # shellcheck disable=SC2046
            set -- $(echo $lines | tr ' ' '\n' | $order | head -12)
            expected=$(for n in "$@"; do
                echo "$(sed -n "${n}p" "f$seed.txt")"
            done)
            if ! actual=$(guard "$ZQ" "f$seed.gz" -l "$@"); then
                echo "FAIL: seed $seed flags '$flags': multi-line zq failed"
                exit 1
            fi
            if [ "$expected" != "$actual" ]; then
                echo "FAIL: seed $seed flags '$flags': multi-line mismatch"
                exit 1
            fi
        done
    done
done
echo "OK"

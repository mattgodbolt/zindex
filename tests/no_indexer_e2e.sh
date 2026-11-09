#!/bin/bash
# End-to-end check that `zindex` with no indexing option still stores every
# line offset, so `zq -l N` works. Usage: no_indexer_e2e.sh <zindex> <zq>
set -euo pipefail

ZINDEX=$(realpath "$1")
ZQ=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

# Compare every STEP-th line (plus the first and last) from zq against zcat.
check_lines() {
    local file=$1 total=$2 step=$3 extra=${4:-}
    local n
    zcat "$file" >"$file.txt"
    for n in 1 $(seq "$step" "$step" "$total") "$total"; do
        # shellcheck disable=SC2086
        expected=$(sed -n "${n}p" "$file.txt")
        actual=$("$ZQ" $extra "$file" -l "$n")
        if [ "$expected" != "$actual" ]; then
            echo "FAIL: $file line $n: expected '$expected' got '$actual'"
            exit 1
        fi
    done
}

python3 -I -c "
for i in range(1,11):
    print('line %d' % i)" | gzip >small.gz
python3 -I -c "
for i in range(1,300001):
    print(('id:%d row %d' % (i, i)) if i%10==0 else 'filler row %d' % i)" |
    gzip >big.gz

"$ZINDEX" small.gz
check_lines small.gz 10 1

"$ZINDEX" big.gz
check_lines big.gz 300000 7919

# Re-running over the existing index, and the options that interact with it.
"$ZINDEX" big.gz
check_lines big.gz 300000 19997
"$ZINDEX" -o big.gz
check_lines big.gz 300000 19997
"$ZINDEX" --skip-first 5 big.gz
check_lines big.gz 300000 19997
"$ZINDEX" --index-file custom.idx big.gz
check_lines big.gz 300000 19997 "--index-file custom.idx"

# Concatenated gzip members.
cat small.gz big.gz small.gz >cat.gz
"$ZINDEX" cat.gz
check_lines cat.gz 300020 19997

# A key query on a line-only index must fail cleanly, not crash or hang.
if "$ZQ" big.gz "id:10" >out.txt 2>err.txt; then
    echo "FAIL: key query on a line-only index unexpectedly succeeded"
    exit 1
fi
[ ! -s out.txt ] || { echo "FAIL: key query printed output"; exit 1; }
grep -q . err.txt || { echo "FAIL: key query gave no error message"; exit 1; }
echo "OK"

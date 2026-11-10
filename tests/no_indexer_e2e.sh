#!/bin/bash
# End-to-end check that `zindex` with no indexing option still stores every
# line offset, so `zq -l N` works. Usage: no_indexer_e2e.sh <zindex> <zq>
set -euo pipefail

ZINDEX=$(realpath "$1")
ZQ=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

# Compare every STEP-th line (plus the first and last) from zq against the
# decompressed file. gzip -dc rather than zcat: Apple's zcat expects .Z files.
check_lines() {
    local file=$1 total=$2 step=$3
    shift 3
    local zq_args=("$@") n expected actual
    gzip -dc "$file" >"$file.txt"
    for n in 1 $(seq "$step" "$step" "$total") "$total"; do
        expected=$(sed -n "${n}p" "$file.txt")
        if ! actual=$("$ZQ" ${zq_args[@]+"${zq_args[@]}"} "$file" -l "$n"); then
            echo "FAIL: zq exited non-zero for $file line $n"
            exit 1
        fi
        if [ "$expected" != "$actual" ]; then
            echo "FAIL: $file line $n: expected '$expected' got '$actual'"
            exit 1
        fi
    done
}

# A small checkpoint interval forces many checkpoints even in a modest file, so
# lookups have to cross several of them.
zindex() { "$ZINDEX" --checkpoint-every 65536 "$@"; }

python3 -I -c "
for i in range(1,11):
    print('line %d' % i)" | gzip >small.gz
python3 -I -c "
for i in range(1,60001):
    print(('id:%d row %d' % (i, i)) if i%10==0 else 'filler row %d' % i)" |
    gzip >big.gz

zindex small.gz
check_lines small.gz 10 1

zindex big.gz
check_lines big.gz 60000 3989

# Re-running over the existing index, and the options that interact with it.
zindex big.gz
check_lines big.gz 60000 9973
zindex -o big.gz
check_lines big.gz 60000 9973
zindex --skip-first 5 big.gz
check_lines big.gz 60000 9973
zindex --index-file custom.idx big.gz
check_lines big.gz 60000 9973 --index-file custom.idx

# Concatenated gzip members.
cat small.gz big.gz small.gz >cat.gz
zindex cat.gz
check_lines cat.gz 60020 9973

# A key query on a line-only index must fail cleanly, not crash or hang.
if "$ZQ" big.gz "id:10" >out.txt 2>err.txt; then
    echo "FAIL: key query on a line-only index unexpectedly succeeded"
    exit 1
fi
[ ! -s out.txt ] || { echo "FAIL: key query printed output"; exit 1; }
grep -q . err.txt || { echo "FAIL: key query gave no error message"; exit 1; }
echo "OK"

#!/bin/bash
# End-to-end check that bad index names and missing indexes give clear errors
# rather than raw SQL ones. Usage: index_names_e2e.sh <zindex> <zq>
set -euo pipefail

ZINDEX=$(realpath "$1")
ZQ=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

printf 'a 1\nb 2\n' | gzip >n.gz

# expect_failure <message-substring> <command...>: must fail, mention the
# substring, and not leak SQL.
expect_failure() {
    local want=$1
    shift
    if "$@" >out.txt 2>err.txt; then
        echo "FAIL: '$*' unexpectedly succeeded"
        exit 1
    fi
    grep -q -- "$want" err.txt || {
        echo "FAIL: '$*' error lacks '$want':"
        cat err.txt
        exit 1
    }
    if grep -q -i "syntax error\|no such table\|SELECT\|CREATE TABLE" err.txt; then
        echo "FAIL: '$*' leaked SQL:"
        cat err.txt
        exit 1
    fi
}

for name in my-idx "two words" 'a;b'; do
    printf '{"indexes":[{"type":"field","delimiter":" ","fieldNum":1,"name":"%s"}]}' \
        "$name" >bad.json
    expect_failure "Invalid index name" "$ZINDEX" --config bad.json n.gz
done

echo '{"indexes":[{"type":"field","delimiter":" ","fieldNum":1,"name":"first"}]}' \
    >c.json
"$ZINDEX" --config c.json n.gz
expect_failure "it contains: first" "$ZQ" n.gz a
[ "$("$ZQ" -i first n.gz a)" = "a 1" ] || { echo "FAIL: -i first"; exit 1; }
expect_failure "no index named 'nope'" "$ZQ" -i nope n.gz a

# A rejected config must leave a previously good index untouched.
cp n.gz.zindex good.zindex
for cfg in '"name":"my-idx"' '"name":"a"},{"type":"field","delimiter":" ","fieldNum":2,"name":"A"'; do
    printf '{"indexes":[{"type":"field","delimiter":" ","fieldNum":1,%s}]}' \
        "$cfg" >bad.json
    expect_failure "ndex name" "$ZINDEX" --config bad.json -o n.gz
    cmp good.zindex n.gz.zindex || {
        echo "FAIL: rejected config damaged the existing index"
        exit 1
    }
done

# Names SQLite accepts unquoted keep working, and -i is case-insensitive.
printf '{"indexes":[{"type":"field","delimiter":" ","fieldNum":1,"name":"caf\\u00e9"}]}' \
    >u.json
"$ZINDEX" --config u.json -o n.gz
[ "$("$ZQ" -i "$(printf 'caf\303\251')" n.gz a)" = "a 1" ] || {
    echo "FAIL: non-ASCII name"
    exit 1
}
"$ZINDEX" --config c.json -o n.gz
[ "$("$ZQ" -i FIRST n.gz a)" = "a 1" ] || { echo "FAIL: -i FIRST"; exit 1; }

# An index built with no indexer has no named indexes at all.
"$ZINDEX" -o n.gz
expect_failure "no indexes" "$ZQ" n.gz a
echo "OK"

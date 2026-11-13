#!/bin/sh
# Exercise the real suite runner with small fixtures instead of the full suite.
set -eu
binary=$(realpath "${1:?Usage: sh test-runner-exit-status.sh REBOL_BINARY}")
tests=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
fixture=$(mktemp -d)
trap 'rm -rf "$fixture"' EXIT
cp "$tests/run-tests.r3" "$tests/quick-test-module.r3" "$fixture/"
mkdir "$fixture/units"
sed -n 's/^[[:space:]]*%\(units\/[^ ]*\.r3\).*$/\1/p' "$tests/run-tests.r3" |
while IFS= read -r unit; do
    cat > "$fixture/$unit" <<'REBOL'
Rebol [Needs: [%../quick-test-module.r3]]
~~~start-file~~~ "Fixture"
--test-- "Passing assertion"
--assert true
~~~end-file~~~
REBOL
done

check_status() {
    expected=$1
    label=$2
    actual=0
    "$binary" -s "$fixture/run-tests.r3" > "$fixture/output.log" 2>&1 || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        cat "$fixture/output.log"
        echo "$label: expected exit $expected, got $actual" >&2
        exit 1
    fi
    grep -q 'Finished.*All tests' "$fixture/output.log"
    echo "$label: exit $actual"
}

check_status 0 'Passing suite'
cat > "$fixture/units/bbcode-test.r3" <<'REBOL'
Rebol [Needs: [%../quick-test-module.r3]]
~~~start-file~~~ "Fixture"
--test-- "Failing assertion"
--assert false
~~~end-file~~~
REBOL
check_status 1 'Failed assertion'
grep -q 'TEST FAILURES' "$fixture/output.log"

cat > "$fixture/units/bbcode-test.r3" <<'REBOL'
Rebol []
do make error! "Intentional unit loading failure"
REBOL
check_status 1 'Failed unit file'
grep -q 'FAILED units files:' "$fixture/output.log"

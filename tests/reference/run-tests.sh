#!/bin/bash
# Compares the Haiku port's engine with ElectroPortis' decompiled ep.c on
# Linux x86_64: both draw thousands of frames with the same random seeds and
# must issue identical OpenGL calls, bit for bit. Also runs the engine alone
# under AddressSanitizer and UBSan.
#
# usage: tests/reference/run-tests.sh
# Needs gcc/g++, python3, curl (once, to fetch ep.c) and the GL headers.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=$ROOT/out/reference
mkdir -p "$OUT"

# ep.c from ElectroPortis, at the commit the port was made from.
COMMIT=882261c51803fa1bfe719984aa32fd4e16a97481
SHA256=7558c992e4c2c94daab2ae658d770b4540639469e111f696387955e14a77c15b
EP=$OUT/ep.c
if [ ! -f "$EP" ]; then
	curl -sfL "https://raw.githubusercontent.com/drvink/electroportis/$COMMIT/ElectroPortis/src/ep.c" -o "$EP.tmp"
	mv "$EP.tmp" "$EP"
fi
echo "$SHA256  $EP" | sha256sum -c --quiet

python3 -I "$HERE/patch-ep.py" "$EP" "$OUT/ep_ref.c"

# No PIE: ep.c's static data must lie below 2 GB. No FMA contraction on
# either side, so that both round every operation the same way.
FLAGS="-O2 -fno-pie -ffp-contract=off"
gcc $FLAGS -fpermissive -w -I"$HERE" -c "$OUT/ep_ref.c" -o "$OUT/ep_ref.o"
gcc $FLAGS -Wall -c "$HERE/lowmem.c" -o "$OUT/lowmem.o"
g++ $FLAGS -Wall -Wextra -Werror -I"$ROOT/src" -I"$HERE" -o "$OUT/epcompare" \
	"$HERE/compare.cpp" "$HERE/glrecord.cpp" "$ROOT/src/ElectroPaintEngine.cpp" \
	"$OUT/ep_ref.o" "$OUT/lowmem.o" -no-pie -lm

g++ -O2 -ffp-contract=off -Wall -Wextra -Werror -I"$ROOT/src" -I"$HERE" -o "$OUT/enginetrace" \
	"$HERE/enginetrace.cpp" "$HERE/glrecord.cpp" "$ROOT/src/ElectroPaintEngine.cpp" -lm

g++ -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
	-ffp-contract=off -Wall -Wextra -Werror -I"$ROOT/src" -I"$HERE" \
	-o "$OUT/enginecheck" "$HERE/enginecheck.cpp" "$HERE/glrecord.cpp" \
	"$ROOT/src/ElectroPaintEngine.cpp" -lm

FAILED=0
check() {
	echo "--- $*"
	if ! "$@"; then
		FAILED=$((FAILED + 1))
	fi
}

# The built-in script: 10 minutes of animation per seed at 60 frames/s.
for seed in 1 12345 1696862400 4294967295; do
	check "$OUT/epcompare" --seed $seed --frames 36000 --min-colors 1000
done
# Frames skipped by Display(false) must not change the animation.
check "$OUT/epcompare" --seed 99 --frames 12000 --skip 3 --min-colors 300
check "$OUT/epcompare" --seed 98 --frames 12000 --skip 7 --min-colors 100
# Commands the built-in script does not use.
check "$OUT/epcompare" --seed 7 --frames 12000 --script "$HERE/scripts/commands.eps" --min-colors 100
check "$OUT/epcompare" --seed 8 --frames 12000 --script "$HERE/scripts/wrap.eps" --min-colors 100

check "$OUT/enginecheck" --frames 6000 "$HERE/scripts/commands.eps" \
	"$HERE/scripts/wrap.eps" "$HERE/scripts/kill.eps"

# The golden trace that tests/haiku/vm-test.sh checks the Haiku build against.
echo "--- enginetrace (tests/reference/enginetrace.expected)"
{ "$OUT/enginetrace" --seed 2026 --frames 12000
  "$OUT/enginetrace" --seed 1 --frames 12000; } > "$OUT/enginetrace.txt"
if ! cmp -s "$OUT/enginetrace.txt" "$HERE/enginetrace.expected"; then
	echo "enginetrace output differs from enginetrace.expected:"
	diff "$HERE/enginetrace.expected" "$OUT/enginetrace.txt" | head -5
	FAILED=$((FAILED + 1))
fi

if [ $FAILED -ne 0 ]; then
	echo "FAILED: $FAILED check(s)"
	exit 1
fi
echo "All reference checks passed."

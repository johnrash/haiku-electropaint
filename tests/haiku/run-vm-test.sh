#!/bin/bash
# Copies the project to a Haiku machine, runs tests/haiku/vm-test.sh there,
# fetches its screenshots into out/shots, which check-shots.py checks, and
# then builds and installs the package with tests/haiku/package-test.sh (it
# stays installed).
#
# usage: tests/haiku/run-vm-test.sh user@host
# The Haiku machine needs sshd with key login, haiku_devel and mesa_devel.
set -u
VM=${1:?usage: $0 user@host}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
mkdir -p out/shots

tar cf - --exclude='objects*' Makefile README.md LICENSE.md src tests package \
	| ssh -o BatchMode=yes "$VM" \
		"mkdir -p ElectroPaint && cd ElectroPaint && tar xmf -" || exit 1

ssh -o BatchMode=yes "$VM" \
	"cd ElectroPaint && bash tests/haiku/vm-test.sh /tmp/electropaint-shots"
RESULT=$?

rm -f out/shots/*
scp -q "$VM:/tmp/electropaint-shots/*" out/shots/ 2>/dev/null
python3 -I tests/haiku/check-shots.py out/shots || RESULT=1

ssh -o BatchMode=yes "$VM" "cd ElectroPaint && bash tests/haiku/package-test.sh" \
	|| RESULT=1
scp -q "$VM:ElectroPaint/out/electropaint-*.hpkg" out/ 2>/dev/null

[ $RESULT -eq 0 ] && echo "Haiku tests passed." || echo "Haiku tests FAILED."
exit $RESULT

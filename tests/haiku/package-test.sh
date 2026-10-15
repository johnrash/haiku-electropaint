#!/bin/bash
# Runs on the Haiku machine, from the project directory: builds the package,
# installs it with pkgman (replacing an installed electropaint package) and
# runs the screen saver from the installed location. The package stays
# installed.
#
# usage: tests/haiku/package-test.sh
set -u
cd "$(dirname "$0")/../.."
OUT=out/haiku
mkdir -p "$OUT"

PASSED=0
FAILED=0
check() {
	local name=$1
	shift
	if "$@"; then
		echo "ok    $name"
		PASSED=$((PASSED + 1))
	else
		echo "FAIL  $name"
		FAILED=$((FAILED + 1))
	fi
}

if ! bash package/build-package.sh > "$OUT/package.log" 2>&1; then
	cat "$OUT/package.log"
	exit 1
fi
HPKG=$(ls out/electropaint-*.hpkg)
package list -i "$HPKG" > "$OUT/package-info.txt" 2>&1
CONTENTS=$OUT/package-contents
rm -rf "$CONTENTS"
mkdir -p "$CONTENTS"
package extract -C "$CONTENTS" "$HPKG"

check "package contains the add-on" \
	test -x "$CONTENTS/add-ons/Screen Savers/ElectroPaint"
check "package contains the ElectroPaint license" \
	test -f "$CONTENTS/data/licenses/ElectroPaint"
check "package contains README.md and LICENSE.md" \
	test -f "$CONTENTS/documentation/packages/electropaint/README.md" \
		-a -f "$CONTENTS/documentation/packages/electropaint/LICENSE.md"
check "package is not a system package (HaikuDepot shows Install)" \
	sh -c "! grep -q -i system_package $OUT/package-info.txt"

pkgman uninstall -y electropaint > "$OUT/pkgman.log" 2>&1
pkgman install -y "$HPKG" >> "$OUT/pkgman.log" 2>&1
check "pkgman installed the package" \
	sh -c "pkgman search -i electropaint | grep -q -i electropaint"
SYSTEM_ADDON="/boot/system/add-ons/Screen Savers/ElectroPaint"
check "add-on in $SYSTEM_ADDON" test -f "$SYSTEM_ADDON"
check "no non-packaged copy hides it" \
	test ! -e "$(finddir B_USER_NONPACKAGED_ADDONS_DIRECTORY)/Screen Savers/ElectroPaint"

if [ ! -x "$OUT/saverhost" ]; then
	g++ -Wall -Werror -o "$OUT/saverhost" tests/haiku/saverhost.cpp -lbe \
		-lscreensaver || exit 1
fi
"$OUT/saverhost" "$SYSTEM_ADDON" 6 320 240 > "$OUT/saverhost-package.txt" 2>&1
RESULT=$?
grep '^tick' "$OUT/saverhost-package.txt"
check "the installed add-on runs" test $RESULT -eq 0

echo "$PASSED passed, $FAILED failed"
[ $FAILED -eq 0 ]

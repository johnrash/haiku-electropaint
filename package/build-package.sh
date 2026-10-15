#!/bin/bash
# Builds the screen saver on Haiku and creates out/electropaint-*.hpkg.
# Needs haiku_devel and mesa_devel.
set -e
cd "$(dirname "$0")/.."
ROOT=$(pwd)
NAME=electropaint
VERSION=$(grep '^version' package/PackageInfo | awk '{print $2}')
ARCH=$(getarch)

make

PKG=$ROOT/out/package
rm -rf "$PKG"
mkdir -p "$PKG/add-ons/Screen Savers" "$PKG/data/licenses" \
	"$PKG/documentation/packages/$NAME"

cp objects.*/ElectroPaint "$PKG/add-ons/Screen Savers/ElectroPaint"
cp package/ElectroPaint.license "$PKG/data/licenses/ElectroPaint"
cp README.md LICENSE.md "$PKG/documentation/packages/$NAME/"
sed "s/^architecture.*/architecture\t$ARCH/" package/PackageInfo \
	> "$PKG/.PackageInfo"

OUT=$ROOT/out/$NAME-$VERSION-$ARCH.hpkg
rm -f "$OUT"
(cd "$PKG" && package create -q "$OUT")
echo "Created $OUT"
echo "Install with: pkgman install $OUT"

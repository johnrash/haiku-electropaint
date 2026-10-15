#!/bin/bash
# Runs on the Haiku machine, from the project directory (see run-vm-test.sh):
# builds the add-on and the test tools, checks the engine against the golden
# trace from Linux, installs the add-on into the user's non-packaged
# "Screen Savers" folder and runs it in a window, in screen_blanker (full
# screen, direct mode) and in the ScreenSaver preferences' preview, taking
# screenshots. ScreenSaver_settings and the non-packaged add-ons folder are
# restored afterwards.
#
# usage: tests/haiku/vm-test.sh [SCREENSHOT-DIR]
set -u
cd "$(dirname "$0")/../.."
SHOTS=${1:-/tmp/electropaint-shots}
OUT=out/haiku
mkdir -p "$SHOTS" "$OUT"
rm -f "$SHOTS"/*

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

kill_team() {
	local team
	for team in $(roster | grep -i "$1" | awk '{print $1}'); do
		kill "$team"
	done
}

# roster shortens long paths, so look for the signature.
team_running() {
	roster | grep -q "($1)"
}

pixel_check() {
	"$OUT/pixelcompare" "$@" > "$OUT/pixelcompare.txt" 2>&1
	local result=$?
	grep -v '^OpenGL\|^Gallium' "$OUT/pixelcompare.txt"
	return $result
}

no_crash_dialog() {
	local count
	count=$(hey debug_server count Window 2>/dev/null \
		| sed -n 's/.*"result" (B_INT32_TYPE) : \([0-9]*\).*/\1/p')
	[ "${count:-0}" = 0 ]
}

# --- build
if ! make > "$OUT/make.log" 2>&1; then
	cat "$OUT/make.log"
	exit 1
fi
ADDON=$(ls objects.*/ElectroPaint)
g++ -O2 -ffp-contract=off -Wall -Werror -Isrc -Itests/reference \
	-o "$OUT/enginetrace" tests/reference/enginetrace.cpp \
	tests/reference/glrecord.cpp src/ElectroPaintEngine.cpp || exit 1
g++ -Wall -Werror -o "$OUT/savermodule" tests/haiku/savermodule.cpp -lbe \
	|| exit 1
g++ -Wall -Werror -o "$OUT/saverhost" tests/haiku/saverhost.cpp -lbe \
	-lscreensaver || exit 1
g++ -O2 -ffp-contract=off -Wall -Werror -Isrc -o "$OUT/pixelcompare" \
	tests/haiku/pixelcompare.cpp src/ElectroPaintEngine.cpp -lOSMesa || exit 1

check "build has no warnings" sh -c "! grep -q -i warning $OUT/make.log"
check "add-on has its signature and version" \
	sh -c "xres -l $ADDON | grep -q BEOS:APP_SIG && xres -l $ADDON | grep -q BEOS:APP_VERSION"
check "add-on exports instantiate_screen_saver" \
	sh -c "nm -D $ADDON | grep -q ' T instantiate_screen_saver'"

# --- the engine computes the same animation as the Linux build
{ "$OUT/enginetrace" --seed 2026 --frames 12000
  "$OUT/enginetrace" --seed 1 --frames 12000; } > "$OUT/enginetrace.txt"
check "engine output identical to tests/reference/enginetrace.expected" \
	cmp -s "$OUT/enginetrace.txt" tests/reference/enginetrace.expected

# --- leaving out the invisible copies changes no pixel (Haiku's Mesa)
for run in "--seed 2026 --frames 1800 --size 640 480" \
		"--seed 1 --frames 600 --size 1280 800" \
		"--seed 7 --frames 1800 --size 120 90"; do
	check "pixel identical without invisible copies ($run)" \
		pixel_check $run
done

# --- install into non-packaged for the tests; removed again at the end so
# that it does not hide an installed package (the runner prefers it)
INSTALLED="$(finddir B_USER_NONPACKAGED_ADDONS_DIRECTORY)/Screen Savers/ElectroPaint"
if [ -f "$INSTALLED" ]; then
	cp -p "$INSTALLED" "$OUT/ElectroPaint.saved"
else
	rm -f "$OUT/ElectroPaint.saved"
fi
make install > "$OUT/install.log" 2>&1
check "installed into $INSTALLED" cmp -s "$ADDON" "$INSTALLED"

# --- settings: keep the user's
SETTINGS="$(finddir B_USER_SETTINGS_DIRECTORY)/ScreenSaver_settings"
if [ -f "$SETTINGS" ]; then
	cp -p "$SETTINGS" "$OUT/ScreenSaver_settings.saved"
else
	rm -f "$OUT/ScreenSaver_settings.saved"
fi
restore() {
	if [ -f "$OUT/ScreenSaver_settings.saved" ]; then
		cp -p "$OUT/ScreenSaver_settings.saved" "$SETTINGS"
	else
		rm -f "$SETTINGS"
	fi
	if [ -f "$OUT/ElectroPaint.saved" ]; then
		cp -p "$OUT/ElectroPaint.saved" "$INSTALLED"
	else
		rm -f "$INSTALLED"
	fi
}
trap restore EXIT
# Select ElectroPaint, opening the preferences on the module list. Time flags
# 0: the idle VM would otherwise get input_server to start screen_blanker in
# the middle of the tests.
"$OUT/savermodule" --tab 1 --timeflags 0 ElectroPaint
sleep 1
kill_team screen_blanker

# --- in a window, the way the runner drives it
( "$OUT/saverhost" "$INSTALLED" 12 640 480 > "$OUT/saverhost.txt" 2>&1
  echo $? > "$OUT/saverhost.rc" ) &
waitfor "w>ElectroPaint test"
sleep 6
screenshot -s -f png "$SHOTS/window.png" > /dev/null 2>&1
wait
cat "$OUT/saverhost.txt"
RATE=$(sed -n 's/.*, \([0-9.]*\) drawn per second.*/\1/p' "$OUT/saverhost.txt")
check "saverhost exited cleanly" grep -q '^0$' "$OUT/saverhost.rc"
check "60 frames per second in a window (${RATE:-?})" \
	awk -v rate="${RATE:-0}" 'BEGIN { exit !(rate >= 57 && rate <= 61) }'
check "no crash dialog" no_crash_dialog

# --- screen_blanker: full screen, direct mode
/boot/system/bin/screen_blanker > "$OUT/screen_blanker.log" 2>&1 &
waitfor -m application/x-vnd.Haiku.screenblanker
sleep 8
screenshot -s -f png "$SHOTS/screen_blanker.png" > /dev/null 2>&1
check "screen_blanker still running after 8 s" \
	team_running application/x-vnd.Haiku.screenblanker
check "screen_blanker found the add-on" \
	sh -c "! grep -q -i 'unable\|failed' $OUT/screen_blanker.log"
check "screen_blanker loaded ElectroPaint" \
	sh -c "listimage \$(roster | grep -i screen_blanker | awk '{print \$1}') | grep -q 'Screen Savers/ElectroPaint'"
kill_team screen_blanker
check "no crash dialog" no_crash_dialog

# --- the preferences' preview and info panel
/boot/system/preferences/ScreenSaver > /dev/null 2>&1 &
waitfor -m application/x-vnd.Haiku-ScreenSaver
waitfor "w>ScreenSaver"
sleep 5
screenshot -s -f png "$SHOTS/preferences.png" > /dev/null 2>&1
check "preferences still running" \
	team_running application/x-vnd.Haiku-ScreenSaver
hey application/x-vnd.Haiku-ScreenSaver get Frame of Window 0 2>/dev/null \
	| grep BRect > "$SHOTS/preferences-frame.txt"
hey application/x-vnd.Haiku-ScreenSaver quit > /dev/null 2>&1
waitfor -e ScreenSaver
check "no crash dialog" no_crash_dialog

echo "$PASSED passed, $FAILED failed"
[ $FAILED -eq 0 ]

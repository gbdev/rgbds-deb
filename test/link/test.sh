#!/usr/bin/env bash

export LC_ALL=C
set -o pipefail

otemp="$(mktemp)"
gbtemp="$(mktemp)"
gbtemp2="$(mktemp)"
outtemp="$(mktemp)"
rc=0

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "rm -f ${otemp@Q} ${gbtemp@Q} ${gbtemp2@Q} ${outtemp@Q}" EXIT

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBASM=../../rgbasm
RGBLINK=../../rgblink

startTest () {
	echo "$bold$green${i%.asm}...$rescolors$resbold"
}

tryDiff () {
	if ! diff -u --strip-trailing-cr "$1" "$2"; then
		echo "${bold}${red}${i%.asm}.out mismatch!${rescolors}${resbold}"
		false
	fi
}

tryCmp () {
	if ! cmp "$1" "$2"; then
		../../contrib/gbdiff.bash "$1" "$2"
		echo "${bold}${red}${i%.asm}.out.bin mismatch!${rescolors}${resbold}"
		false
	fi
}
tryCmpRom () {
	# `printf` lets us keep only the first returned word from `wc`.
	rom_size=$(printf %s $(wc -c <"$1"))
	dd if="$gbtemp" count=1 bs="$rom_size" >"$otemp" 2>/dev/null
	tryCmp "$1" "$otemp"
}

rgblinkQuiet () {
	out="$(env $RGBLINK "$@")" || return $?
	if [[ -n "$out" ]]; then
		echo "$bold${red}Linking shouldn't produce anything on stdout!$rescolors$resbold"
		false
	fi
}

for i in *.asm; do
	startTest
	"$RGBASM" -o "$otemp" "$i"

	# Some tests have variants depending on flags
	ran_flag=false
	for flag in '-d' '-t' '-w'; do
		if [ -f "${i%.asm}-no${flag}.out" ]; then
			rgblinkQuiet -o "$gbtemp" "$otemp" >"$outtemp" 2>&1
			tryDiff "${i%.asm}-no${flag}.out" "$outtemp"
			(( rc = rc || $? ))
			ran_flag=true
		fi
		if [ -f "${i%.asm}${flag}.out" ]; then
			rgblinkQuiet ${flag} -o "$gbtemp" "$otemp" >"$outtemp" 2>&1
			tryDiff "${i%.asm}${flag}.out" "$outtemp"
			(( rc = rc || $? ))
			ran_flag=true
		fi
	done
	if "$ran_flag"; then
		continue
	fi

	# Other tests have several linker scripts
	while read -rd '' script; do
		rgblinkQuiet -l "$script" -o "$gbtemp" "$otemp" >"$outtemp" 2>&1
		tryDiff "${script%.link}.out" "$outtemp"
		(( rc = rc || $? ))
		ran_flag=1
	done < <(find . -name "${i%.asm}*.link" -print0)
	if [ -n "$ran_flag" ]; then
		continue
	fi

	# The rest of the tests just links a file, and maybe checks the binary
	rgblinkQuiet -o "$gbtemp" "$otemp" >"$outtemp" 2>&1
	if [ -f "${i%.asm}.out" ]; then
		tryDiff "${i%.asm}.out" "$outtemp"
		(( rc = rc || $? ))
	fi

	bin=${i%.asm}.out.bin
	if [ -f "$bin" ]; then
		tryCmpRom "$bin"
		(( rc = rc || $? ))
	fi
done

# These tests do their own thing

i="bank-const.asm"
startTest
"$RGBASM" -o "$otemp" bank-const/a.asm
"$RGBASM" -o "$gbtemp2" bank-const/b.asm
rgblinkQuiet -o "$gbtemp" "$gbtemp2" "$otemp" >"$outtemp" 2>&1
tryDiff bank-const/out.err "$outtemp"
(( rc = rc || $? ))

for i in fragment-align/*; do
	startTest
	"$RGBASM" -o "$otemp" "$i"/a.asm
	"$RGBASM" -o "$gbtemp2" "$i"/b.asm
	rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2" 2>"$outtemp"
	tryDiff "$i"/out.err "$outtemp"
	(( rc = rc || $? ))
	if [[ -f "$i"/out.gb ]]; then
		tryCmpRom "$i"/out.gb
		(( rc = rc || $? ))
	fi
done

i="high-low.asm"
startTest
"$RGBASM" -o "$otemp" high-low/a.asm
rgblinkQuiet -o "$gbtemp" "$otemp"
"$RGBASM" -o "$otemp" high-low/b.asm
rgblinkQuiet -o "$gbtemp2" "$otemp"
tryCmp "$gbtemp" "$gbtemp2"
(( rc = rc || $? ))

i="overlay.asm"
startTest
"$RGBASM" -o "$otemp" overlay/a.asm
rgblinkQuiet -o "$gbtemp" -t -O overlay/overlay.gb "$otemp" >"$outtemp" 2>&1
tryDiff overlay/out.err "$outtemp"
(( rc = rc || $? ))
# This test does not trim its output with 'dd' because it needs to verify the correct output size
tryCmp overlay/out.gb "$gbtemp"
(( rc = rc || $? ))

i="scramble-romx.asm"
startTest
"$RGBASM" -o "$otemp" scramble-romx/a.asm
rgblinkQuiet -o "$gbtemp" -S romx=3 "$otemp" >"$outtemp" 2>&1
tryDiff scramble-romx/out.err "$outtemp"
(( rc = rc || $? ))
# This test does not compare its exact output with 'tryCmpRom' because no scrambling order is guaranteed
rom_size=$(printf %s $(wc -c <"$gbtemp"))
test "$rom_size" = 65536 # Check for exactly 3 ROMX banks
(( rc = rc || $? ))

i="section-fragment/jr-offset.asm"
startTest
"$RGBASM" -o "$otemp" section-fragment/jr-offset/a.asm
"$RGBASM" -o "$gbtemp2" section-fragment/jr-offset/b.asm
rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2"
tryCmpRom section-fragment/jr-offset/ref.out.bin
(( rc = rc || $? ))

i="section-union/good.asm"
startTest
"$RGBASM" -o "$otemp" section-union/good/a.asm
"$RGBASM" -o "$gbtemp2" section-union/good/b.asm
rgblinkQuiet -o "$gbtemp" -l section-union/good/script.link "$otemp" "$gbtemp2"
tryCmpRom section-union/good/ref.out.bin
(( rc = rc || $? ))

i="section-union/fragments.asm"
startTest
"$RGBASM" -o "$otemp" section-union/fragments/a.asm
"$RGBASM" -o "$gbtemp2" section-union/fragments/b.asm
rgblinkQuiet -o "$gbtemp" "$otemp" "$gbtemp2"
tryCmpRom section-union/fragments/ref.out.bin
(( rc = rc || $? ))

for i in section-union/*.asm; do
	startTest
	"$RGBASM" -o "$otemp" "$i"
	"$RGBASM" -o "$gbtemp2" "$i" -DSECOND
	if rgblinkQuiet "$otemp" "$gbtemp2" 2>"$outtemp"; then
		echo -e "${bold}${red}$i didn't fail to link!${rescolors}${resbold}"
		rc=1
	fi
	echo --- >>"$outtemp"
	# Ensure RGBASM also errors out
	cat "$i" - "$i" <<<'def SECOND equs "1"' | "$RGBASM" - 2>>"$outtemp"
	tryDiff "${i%.asm}.out" "$outtemp"
	(( rc = rc || $? ))
done

exit $rc

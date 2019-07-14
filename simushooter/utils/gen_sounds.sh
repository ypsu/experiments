#!/bin/bash

# Sound encoding: 22050 Hz 16 bit signed single channel

echo -n "" > assets/sounds.desc
echo -n "" > assets/sounds.raw

error=0
for file in assets/raw_sounds/*.wav; do
	echo -e "\tProcessing $file"

	# Verify the format and the number of samples
	error=existence
	if ! test -f "$file"; then break; fi
	error=name_length
	if test "${#file}" -gt 48; then break; fi
	error=frequency
	if test "$(soxi -r $file)" != 22050; then break; fi
	error=channels
	if test "$(soxi -c $file)" != 1; then break; fi
	error=bps
	if test "$(soxi -b $file)" != 16; then break; fi
	error=encoding
	if test "$(soxi -e $file)" != "Signed Integer PCM"; then break; fi
	error=

	echo -n "$(basename $file) " >> assets/sounds.desc
	soxi -s $file >> assets/sounds.desc
	sox "$file" -t raw - >> assets/sounds.raw
done

if test "$error" != ""; then
	echo "Error with $file: wrong $error."
	exit 1
fi

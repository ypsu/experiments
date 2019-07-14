#!/bin/bash

if (($# == 0)); then
	echo $0 [picture-filename]
	echo
	echo Converts [picture-filename] to poster.bmp with correct size.
	exit
fi

convert -resize 720x1080 "$1" poster.bmp

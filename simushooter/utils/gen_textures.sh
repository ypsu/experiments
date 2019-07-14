#!/bin/bash

echo -n > textures.mvg
echo "viewbox 0 0 1024 1024" >> textures.mvg
echo "fill rgba(255, 0, 255, 0)" >> textures.mvg
echo "color 0 0 floodfill" >> textures.mvg
echo "" >> textures.mvg

echo -n > hitmap.mvg
echo "viewbox 0 0 1024 1024" >> hitmap.mvg
echo "fill black" >> hitmap.mvg
echo "color 0 0 floodfill" >> hitmap.mvg
echo "" >> hitmap.mvg

while read; do
	line=($REPLY)
	tex=assets/raw_textures/${line[0]}
	x=${line[1]}
	y=${line[2]}
	w=${line[3]}
	h=${line[4]}

	echo -e "\tProcessing $(basename $tex)."
	# Check whether the dimensions are correct
	if ! identify $tex | grep -q "$tex PNG ${w}x$h"; then
		echo -e "\tError processing $tex"
		exit 1
	fi
	echo "image src-over $x $y $w $h $tex" >> textures.mvg

	hitmap=assets/raw_hitmaps/$(basename ${tex%.png}.pgm)
	if ! test -a $hitmap; then
		continue;
	fi
	echo -e "\tProcessing $(basename $hitmap)."
	# Check whether the dimensions are correct
	if ! identify $hitmap | grep -q "$hitmap PGM ${w}x$h"; then
		echo -e "\tError processing $hitmap"
		exit 1
	fi
	echo "image src-over $x $y $w $h $hitmap" >> hitmap.mvg
done < assets/textures.desc

echo -e "\tWriting textures.pam."
if ! convert -depth 8 textures.mvg assets/textures.pam; then
	echo -e "\tConversion error"
	exit 1
fi
rm textures.mvg

echo -e "\tWriting hitmap.pgm."
if ! convert -depth 8 hitmap.mvg assets/hitmap.pgm; then
	echo -e "\tConversion error"
	exit 1
fi
rm hitmap.mvg

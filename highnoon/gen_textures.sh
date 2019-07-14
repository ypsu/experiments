#!/bin/bash

# In the picture 1 pixel is 1 cm. Thus to create a 1 m long object, make it 100
# pixel long.

echo -n > textures.mvg
echo "viewbox 0 0 1024 1024" >> textures.mvg
echo "fill rgba(0, 0, 0, 0)" >> textures.mvg
echo "color 0 0 floodfill" >> textures.mvg
echo "" >> textures.mvg

while read; do
	line=($REPLY)
	tex=data/${line[0]}
	x=${line[1]}
	y=${line[2]}
	w=${line[3]}
	h=${line[4]}

	echo -e "\tProcessing $tex"

	# Check whether the dimensions are correct
	if ! identify $tex | grep -q "$tex PNG ${w}x$h"; then
		echo -e "\tError processing $tex"
		exit 1
	fi

	echo "image src-over $x $y $w $h $tex" >> textures.mvg
done < textures.desc

echo -e "\tWriting textures.tga"
if ! convert textures.mvg textures.tga; then
	echo -e "\tConversion error"
	exit 1
fi
rm textures.mvg

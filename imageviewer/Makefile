all: iview image-selector

iview: iview.c
	gcc -g -O2 -Wall -Wextra -std=c99 iview.c -o iview -lm -lIL -lILU -lexif -lSDL -lpthread

image-selector: image-selector.c
	gcc -g -Wall -Wextra -std=c99 image-selector.c -o image-selector

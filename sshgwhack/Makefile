.PHONY: all clean

all: loc rem

clean:
	rm -f loc rem

%: %.c headers.h common.c
	gcc -std=c99 -Wall -Wextra -Werror -g -O2 $< common.c -o $@

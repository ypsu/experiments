CFLAGS := -std=c99 -Wall -Wextra -Werror -rdynamic -g

all: test libtracking_malloc.so

test: test.c
	gcc $(CFLAGS) $< -o $@

libtracking_malloc.so: tracking_malloc.c
	gcc -shared -fPIC $(CFLAGS) $< -o $@

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	malloc(1234);
	void *p = strdup("hello world");
	p = realloc(p, 5);
	puts(p);
	if (getenv("LD_PRELOAD") != NULL)
		free((void*)-1);
	free(p);
	return 0;
}

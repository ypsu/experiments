#include "headers.h"

float
maxf(float a, float b)
{
	return (a>b) ? a : b;
}

float
minf(float a, float b)
{
	return (a<b) ? a : b;
}

float
sqrf(float x)
{
	return x*x;
}

float
clampf(float value, float lo, float hi)
{
	if (value < lo)
		return lo;
	if (value > hi)
		return hi;
	return value;
}

int
clampi(int value, int lo, int hi)
{
	if (value < lo)
		return lo;
	if (value > hi)
		return hi;
	return value;
}

float
randf(void)
{
	int r = rand();
	return (float)r / (RAND_MAX-1);
}

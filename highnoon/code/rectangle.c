#include "headers.h"

void r_inc(rect_t *r, const vec_t *v)
{
	v_inc(&r->a, v);
	v_inc(&r->b, v);
}

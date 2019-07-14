#include "headers.h"

const struct vector v_zero = { 0.0f, 0.0f };

void v_inc(vec_t *r, const vec_t *a)
{
	r->x += a->x;
	r->y += a->y;
}

void v_dec(vec_t *r, const vec_t *a)
{
	r->x -= a->x;
	r->y -= a->y;
}

void v_add(vec_t *r, const vec_t *a, const vec_t *b)
{
	r->x = a->x + b->x;
	r->y = a->y + b->y;
}

void v_sub(vec_t *r, const vec_t *a, const vec_t *b)
{
	r->x = a->x - b->x;
	r->y = a->y - b->y;
}

void v_rotate(vec_t *r, float angle)
{
	float cosa = cosf(angle);
	float sina = sinf(angle);

	vec_t v = *r;
	v.x = r->x*cosa - r->y*sina;
	v.y = r->x*sina + r->y*cosa;

	*r = v;
}

void v_scale(vec_t *r, float f)
{
	r->x *= f;
	r->y *= f;
}

void v_normalize(vec_t *r)
{
	v_scale(r, 1.0f / v_length(r));
}

float v_magn(const vec_t *v)
{
	return sqrf(v->x) + sqrf(v->y);
}

float v_length(const vec_t *v)
{
	return sqrtf(v_magn(v));
}

float v_dot(const vec_t *a, const vec_t *b)
{
	return a->x*b->x + a->y*b->y;
}

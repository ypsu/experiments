#include "headers.h"

const vector_t
v_zero   = {{0.0f, 0.0f, 0.0f, 0.0f}},
v_origin = {{0.0f, 0.0f, 0.0f, 1.0f}},
v_up     = {{0.0f, 0.0f, 1.0f, 0.0f}};

void
v_set(vector_t *v, float x, float y, float z, float w)
{
	v->v[0] = x;
	v->v[1] = y;
	v->v[2] = z;
	v->v[3] = w;
}

void
v_inc(vector_t *v, const vector_t *w)
{
	for (int i = 0; i < 4; i++) {
		v->v[i] += w->v[i];
	}
}

void
v_dec(vector_t *v, const vector_t *w)
{
	for (int i = 0; i < 4; i++) {
		v->v[i] -= w->v[i];
	}
}

void
v_add(vector_t *v, const vector_t *a, const vector_t *b)
{
	*v = *a;
	v_inc(v, b);
}

void
v_sub(vector_t *v, const vector_t *a, const vector_t *b)
{
	*v = *a;
	v_dec(v, b);
}

void
v_scale(vector_t *v, float f)
{
	for (int i = 0; i < 4; i++) {
		v->v[i] *= f;
	}
}

float
v_dot(const vector_t *a, const vector_t *b)
{
	float p = 0.0f;
	for (int i = 0; i < 4; i++) {
		p += a->v[i] * b->v[i];
	}
	return p;
}

float
v_magn(const vector_t *v)
{
	assert(v->v[3] == 0.0f && "Are sure this is a vector and not a point?");

	float s = 0.0f;
	for (int i = 0; i < 4; i++) {
		s += sqrf(v->v[i]);
	}
	return s;
}

void
v_normalize(vector_t *v)
{
	assert(v->v[3] == 0.0f && "Are sure this is a vector and not a point?");

	float n = sqrtf(v_magn(v));
	assert(n > 1.0e-9f);
	v_scale(v, 1.0f / n);
}

const char *
v_tostr(const vector_t *v)
{
	const char *fmt = "[%6.2f, %6.2f, %6.2f, %6.2f]";
	return qprintf(fmt, v->v[0], v->v[1], v->v[2], v->v[3]);
}

void
v_transform(vector_t *out, const matrix_t *m, const vector_t *v)
{
	assert(out != v);

	for (int i = 0; i < 4; i++) {
		float s = 0.0f;
		for (int j = 0; j < 4; j++) {
			s += m->m[i][j] * v->v[j];
		}
		out->v[i] = s;
	}
}

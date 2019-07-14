#include "headers.h"

const matrix_t
m_zero;

const matrix_t
m_id = {
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f },
	}
};

void
m_mult(matrix_t *out, const matrix_t *a, const matrix_t *b)
{
	assert(a != out && b != out);

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			float s = 0.0f;
			for (int k = 0; k < 4; k++) {
				s += a->m[i][k] * b->m[k][j];
			}
			out->m[i][j] = s;
		}
	}
}

void
m_transpose(matrix_t *m)
{
	for (int i = 0; i < 4; i++) {
		for (int j = i+1; j < 4; j++) {
			float t = m->m[i][j];
			m->m[i][j] = m->m[j][i];
			m->m[j][i] = t;
		}
	}
}

void
m_scale(matrix_t *m, float f)
{
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			m->m[i][j] *= f;
		}
	}
}

void
m_set_xrot(matrix_t *m, float angle)
{
	float cosa = cosf(angle);
	float sina = sinf(angle);
	*m = m_id;
	m->m[1][1] =  cosa;
	m->m[1][2] = -sina;
	m->m[2][1] =  sina;
	m->m[2][2] =  cosa;
}

void
m_set_yrot(matrix_t *m, float angle)
{
	float cosa = cosf(angle);
	float sina = sinf(angle);
	*m = m_id;
	m->m[0][0] =  cosa;
	m->m[0][2] = -sina;
	m->m[2][0] =  sina;
	m->m[2][2] =  cosa;
}

void
m_set_zrot(matrix_t *m, float angle)
{
	float cosa = cosf(angle);
	float sina = sinf(angle);
	*m = m_id;
	m->m[0][0] =  cosa;
	m->m[0][1] = -sina;
	m->m[1][0] =  sina;
	m->m[1][1] =  cosa;
}

void
m_set_translate(matrix_t *m, const vector_t *v)
{
	*m = m_id;
	m->m[0][3] = v->v[0];
	m->m[1][3] = v->v[1];
	m->m[2][3] = v->v[2];
}

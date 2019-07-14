#include "headers.h"

const int
font_x = 8,
font_y = 16;

static struct {
	float s0, t0, ds, dt;
}
_v;

static void
_get_texcoord(int ch, float *s0, float *t0, float *u1, float *v1)
{
	CHECK(ch >= 32 && ch <= 127);
	ch -= 32;
	int row = ch / 16;
	int col = ch % 16;
	*s0 = _v.s0 + col * _v.ds;
	*t0 = _v.t0 + row * _v.dt;
	*u1 = *s0 + _v.ds;
	*v1 = *t0 + _v.dt;
}

void
font_init(void)
{
	puts("Initializing the system font.");

	_v.s0 = 0.0f, _v.t0 = 0.0f;
	tex_get("font.png", &_v.s0, &_v.t0);
	float u1 = 1.0f, v1 = 1.0f;
	tex_get("font.png", &u1, &v1);
	_v.ds = (u1 - _v.s0) / 16.0f;
	_v.dt = (v1 - _v.t0) / 6.0f;
}

void
font_stop(void)
{
}

void
font_draw(int x, int y, const vector_t *color, const char *str)
{
	vector_t col = *color;
	int len = strlen(str);
	assert(len < MAXSTR);

	struct vertex *v = ge_get_ortho_vertices(len);

	int nx = x + font_x;
	int ny = y + font_y;
	for (int i = 0; i < len; i++) {
		int j = 4*i;
		float s0, t0, u1, v1;
		_get_texcoord((uint8_t) str[i], &s0, &t0, &u1, &v1);

		v[j + 0] = (struct vertex) {
			{{ x, y, 0.0f, 1.0f }},
			col,
			s0, t0,
		};
		v[j + 1] = (struct vertex) {
			{{ x, ny, 0.0f, 1.0f }},
			col,
			s0, v1,
		};
		v[j + 2] = (struct vertex) {
			{{ nx, ny, 0.0f, 1.0f }},
			col,
			u1, v1,
		};
		v[j + 3] = (struct vertex) {
			{{ nx, y, 0.0f, 1.0f }},
			col,
			u1, t0,
		};

		x += font_x;
		nx += font_x;
	}
}

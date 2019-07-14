#include "headers.h"

float font_x;
float font_y;

static float _u0, _v0, _du, _dv;

static void _get_texcoord(int ch, float *u0, float *v0, float *u1, float *v1)
{
	ch -= 32;
	int row = ch / 16;
	int col = ch % 16;
	*u0 = _u0 + col * _du;
	*v0 = _v0 + row * _dv;
	*u1 = *u0 + _du;
	*v1 = *v0 + _dv;
}

void font_init(void)
{
	puts("Initializing the font subsystem");

	font_x = 16 * (screen_width / video_width);
	font_y = 32 * (screen_height / video_height);

	_u0 = 0.0, _v0 = 0.0;
	tex_get("font.png", &_u0, &_v0);
	float u1 = 1.0, v1 = 1.0;
	tex_get("font.png", &u1, &v1);
	_du = (u1 - _u0) / 16.0f;
	_dv = (v1 - _v0) / 6.0f;
}

void font_destroy(void)
{
}

void font_draw(float x, float y, const struct color *color, const char *str)
{
	int len = strlen(str);

	struct graphics_rect_desc d;
	d.color = color;
	rect_t r = { { 0.0f, 0.0f }, { font_x, -font_y } };
	d.rect = &r;
	vec_t v = { x, y };
	d.pos = &v;
	for (int i = 0; i < len; ++i) {
		float u0, v0, u1, v1;
		_get_texcoord((uint8_t) str[i], &u0, &v0, &u1, &v1);
		rect_t uv = { { u0, v0 }, { u1, v1 } };
		d.uv = &uv;
		graphics_draw_rect(&d);
		v.x += font_x;
	}
}

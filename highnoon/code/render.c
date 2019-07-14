#include "headers.h"

struct _texcoords {
	float u0, v0;
	float u1, v1;
};

static rect_t _tex_crosshair;
static struct _texcoords _tex_dot;
static struct _texcoords _tex_blood;
static struct _texcoords _tex_flash;

static void _get_coords(struct _texcoords *tex, const char *texname)
{
	*tex = (struct _texcoords) { 0.0f, 0.0f, 1.0f, 1.0f };
	tex_get(texname, &tex->u0, &tex->v0);
	tex_get(texname, &tex->u1, &tex->v1);
}

void render_init(void)
{
	puts("Initializing the rendering subsystem");
	tex_get_rect("crosshair.png", &_tex_crosshair);
	_get_coords(&_tex_dot, "dot.png");
	_get_coords(&_tex_blood, "blood.png");
	_get_coords(&_tex_flash, "muzzle_flash.png");
}

void render_destroy(void)
{
}

void render(void)
{
	struct vertex *vertices;
	struct graphics_rect_desc d;
	struct color col;
	rect_t uv, r;
	float x, y, nx, ny;
	float u0, v0, u1, v1;
	u0 = _tex_dot.u0;
	v0 = _tex_dot.v0;
	u1 = _tex_dot.u1;
	v1 = _tex_dot.v1;
	u0 = (u0 + u1) / 2.0f;
	v0 = (v0 + v1) / 2.0f;

	glClear(GL_COLOR_BUFFER_BIT);

	// Render the background
	col = (struct color) { 0.2f, 0.1f, 0.0f, 1.0f };
	d.pos = &v_zero;
	d.rect = &r;
	uv.a.x = u0;
	uv.a.y = v0;
	uv.b.x = u0;
	uv.b.y = v0;
	d.uv = &uv;
	r = (rect_t) { { 0.0f, 0.0f }, { screen_width, screen_height } };
	d.color = &col;
	graphics_draw_rect(&d);
	col = (struct color) { 0.0f, 0.1f, 0.0f, 1.0f };
	float srw = state.shoot_range_width;
	r.a = (vec_t) { screen_width/2.0f - srw/2.0f, 0.0f };
	r.b = (vec_t) { screen_width/2.0f + srw/2.0f, screen_height };
	graphics_draw_rect(&d);

	// Render stats at top right
	const char *str;
	int len;
	float tx;
	if (false) {
		str = qprintf("FPS: %4d", state.fps);
		len = strlen(str);
		tx = screen_width - len*font_x;
		font_draw(tx, screen_height, &color_white, str);
		str = qprintf("VPF: %4d", state.vertices_last_frame);
		len = strlen(str);
		tx = screen_width - len*font_x;
		font_draw(tx, screen_height - font_y, &color_white, str);
	}

	// Render the status messages at the top
	for (int i = 0; i < STATUS_MSG_MAX; ++i) {
		if (state.status_msg_last_frame[i] >= state.frame_id) {
			float h = screen_height - i*font_y;
			font_draw(0, h, &color_white, state.status_msg[i]);
		}
	}

	if (state.menu_active) {
		menu_render();
	} else {
		// Render the gunmen and animals
		if (!state.menu_active) {
			for (int i = 0; i < PLAYERS_MAX; ++i) {
				if (!state.gunmen[i].active)
					continue;
				gunman_draw(&state.gunmen[i]);
			}

			for (int i = 0; i < state.animals_count; ++i)
				animal_render(&state.animals[i]);
		}

		// Render the bullet splashes
		for (int i = 0; i < BLOOD_SPLASHES; ++i) {
			struct blood_splash *bs = &state.blood[i];
			int frame = (state.frame_id - bs->frame_activated)/3;
			if (frame >= 13)
				continue;
			d.color = &color_white;
			d.pos = &bs->pos;
			rect_t r = { { -0.2f, +0.2f }, { +0.2f, -0.2f } };
			d.rect = &r;
			rect_t uv;
			float dx = _tex_blood.u1 - _tex_blood.u0;
			uv.a.x = _tex_blood.u0 + frame * 1.0f/13.0f * dx;
			uv.a.y = _tex_blood.v0;
			uv.b.x = _tex_blood.u0 + (frame+1) * 1.0f/13.0f * dx;
			uv.b.y = _tex_blood.v1;
			d.uv = &uv;
			graphics_draw_rect(&d);
		}

		// Render the muzzle flashes
		for (int i = 0; i < MUZZLE_FLASHES; ++i) {
			struct muzzle_flash *mf = &state.flashes[i];
			int frame = state.frame_id - mf->frame_activated;
			if (frame >= 20)
				continue;
			struct color col = color_white;
			col.a = 1.0f - frame/20.0f;
			d.color = &col;
			d.pos = &mf->pos;
			rect_t r = { { -0.2f, +0.0f }, { +0.2f, -0.4f } };
			d.rect = &r;
			rect_t uv;
			uv.a.x = _tex_flash.u0;
			uv.a.y = _tex_flash.v0;
			uv.b.x = _tex_flash.u1;
			uv.b.y = _tex_flash.v1;
			d.uv = &uv;
			graphics_draw_rect_rot(&d, mf->angle);
		}

		// Render bullet trails
		graphics_vertices_draw();
		vertices = graphics_vertices_get(0);
		int n = 0;
		for (int i = 0; i < BULLETS_MAX; ++i) {
			if (!state.bullets[i].active)
				continue;
			struct bullet *b = &state.bullets[i];
			x = b->pos.x - b->speed*b->dir.x;
			y = b->pos.y - b->speed*b->dir.y;
			nx = b->pos.x;
			ny = b->pos.y;
			vertices[n+0] = (struct vertex) {
				{ x, y },
					{ u0, v0 },
					color_white,
			};
			vertices[n+1] = (struct vertex) {
				{ nx, ny },
					{ u0, v0 },
					color_white,
			};
			n += 2;
		}
		glDrawArrays(GL_LINES, 0, n);

		// Render various texts on screen
		bool practice = state.practice_mode;
		if (practice && state.gamestate == GAMESTATE_GETREADY) {
			const char *text = "G E T   R E A D Y !";
			float mx = screen_width/2.0f - font_x*strlen(text)/2.0f;
			x = mx;
			y = screen_height/2.0f + font_y/2.0f;
			font_draw(x, y, &color_white, text);
		} else if (state.gamestate == GAMESTATE_GETREADY) {
			const char *text = "G E T   R E A D Y !";
			float mx = screen_width/2.0f - font_x*strlen(text)/2.0f;
			x = mx;
			y = screen_height/2.0f + font_y/2.0f;
			font_draw(x, y, &color_white, text);
			y -= 1.5f * font_y;
			text = "Waiting for: ";
			font_draw(x, y, &color_white, text);
			x += strlen(text) * font_x;
			for (int i = 0; i < PLAYERS_MAX; ++i) {
				struct gunman *gm = &state.gunmen[i];
				if (!gm->active)
					continue;
				if (gunman_inbarrel(gm)) {
					x += 2.0f * font_x;
					continue;
				}
				char buf[4] = "  ";
				buf[0] = '1' + i;
				font_draw(x, y, &player_colors[i], buf);
				x += 2.0f * font_x;
			}
			y -= 1.5f * font_y;
			bool has_penalty = false;
			for (int i = 0; i < PLAYERS_MAX; ++i) {
				struct gunman *gm = &state.gunmen[i];
				if (!gm->active)
					continue;
				has_penalty = has_penalty || gm->penalty;
			}
			if (has_penalty) {
				x = mx;
				text = "  Penalties: ";
				font_draw(x, y, &color_white, text);
				x += strlen(text) * font_x;
				for (int i = 0; i < PLAYERS_MAX; ++i) {
					struct gunman *gm = &state.gunmen[i];
					if (!gm->active)
						continue;
					if (!gm->penalty) {
						x += 2.0f * font_x;
						continue;
					}
					char buf[4] = "  ";
					buf[0] = '1' + i;
					col = player_colors[i];
					font_draw(x, y, &col, buf);
					x += 2.0f * font_x;
				}
			}
		} else if (state.gamestate == GAMESTATE_COUNTDOWN) {
			int seconds;
			seconds = state.countdown_start+180 - state.frame_id;
			seconds /= 60;
			char text[2] = " ";
			text[0] = '1' + seconds;
			x = screen_width/2.0f - font_x*strlen(text)/2.0f;
			y = screen_height/2.0f + font_y/2.0f;
			font_draw(x, y, &color_white, text);
		} else if (state.gamestate == GAMESTATE_GAMEOVER) {
			const char *text = state.gameover_text;
			float mx = screen_width/2.0f - font_x*strlen(text)/2.0f;
			x = mx;
			y = screen_height/2.0f + font_y/2.0f;
			font_draw(x, y, &color_white, text);
			if (state.practice_won && state.current_level == 9) {
				text = "CONGRATULATIONS";
				x = screen_width/2.0f-font_x*strlen(text)/2.0f;
				y -= 1.5f * font_y;
				font_draw(x, y, &color_white, text);

				text = "THE GRASSLAND IS NOW FREE FROM "
					"CUTE LITTLE CREATURES";
				x = screen_width/2.0f-font_x*strlen(text)/2.0f;
				y -= 1.5f * font_y;
				font_draw(x, y, &color_white, text);

				text = "YOU CAN NOW REST IN PEACE";
				x = screen_width/2.0f-font_x*strlen(text)/2.0f;
				y -= 1.5f * font_y;
				font_draw(x, y, &color_white, text);
			}
		}

		x = screen_width/2.0f - font_x*strlen(state.gamehint)/2.0f;
		y = font_y;
		font_draw(x, y, &color_white, state.gamehint);

		if (state.practice_mode) {
			const char *str;
			float time_left = state.time_left / 60.0f;
			str = qprintf("TIME: %5.2lf seconds", time_left);
			x = screen_width/2.0f - font_x*strlen(str)/2.0f;
			y = screen_height;
			font_draw(x, y, &color_white, str);
		}
	}

	// Render the mouse cursors
	for (int i = 0; i < PLAYERS_MAX; ++i) {
		if (state.menu_active) {
			if (!state.inputs_active[i])
				continue;
		} else {
			if (!state.gunmen[i].active)
				continue;
		}

		rect_t r = { { -0.1f, +0.1f }, { +0.1f, -0.1f } };
		vec_t mp = { state.mouses[i].x, state.mouses[i].y };
		struct graphics_rect_desc d;
		d.rect = &r;
		d.pos = &mp;
		d.uv = &_tex_crosshair;
		d.color = &player_colors[i];
		graphics_draw_rect(&d);
	}

	// Render the debug lines
	graphics_vertices_draw();
	vertices = graphics_vertices_get(0);
	u0 = _tex_dot.u0;
	v0 = _tex_dot.v0;
	u1 = _tex_dot.u1;
	v1 = _tex_dot.v1;
	u0 = (u0 + u1) / 2.0f;
	v0 = (v0 + v1) / 2.0f;
	for (int i = 0; i < state.debuglines_count; ++i) {
		x = state.debuglines[i].a.x;
		y = state.debuglines[i].a.y;
		nx = state.debuglines[i].b.x;
		ny = state.debuglines[i].b.y;
		vertices[i*2] = (struct vertex) {
			{ x, y },
			{ u0, v0 },
			color_white,
		};
		vertices[i*2+1] = (struct vertex) {
			{ nx, ny },
			{ u0, v0 },
			color_white,
		};
	}
	glDrawArrays(GL_LINES, 0, state.debuglines_count*2);

	graphics_vertices_draw();
	video_swap();
	graphics_check_errors();
}

#include "headers.h"

void
render(void)
{
	struct vertex *v;

	// Render all robots.
	for (int i = 0; i < MAX_ROBOTS; i++) {
		const struct robot *r = &g.robots[i];
		if (!r->active) continue;
		v = ge_get_dynamic_vertices(1);
		const vector_t *t = &textures[TEX_ROBOT];
		float by = robot_radius, ey = -robot_radius;
		v[0] = (struct vertex) {
			.pos = (vector_t) {{ 0.0f, by, robot_height, 0.0f }},
			.color = color_white,
			.s = t->v[0],
			.t = t->v[1],
		};
		v[1] = (struct vertex) {
			.pos = (vector_t) {{ 0.0f, by, 0.0f, 0.0f }},
			.color = color_white,
			.s = t->v[0],
			.t = t->v[3],
		};
		v[2] = (struct vertex) {
			.pos = (vector_t) {{ 0.0f, ey, 0.0f, 0.0f }},
			.color = color_white,
			.s = t->v[2],
			.t = t->v[3],
		};
		v[3] = (struct vertex) {
			.pos = (vector_t) {{ 0.0f, ey, robot_height, 0.0f }},
			.color = color_white,
			.s = t->v[2],
			.t = t->v[1],
		};
		matrix_t zrot;
		m_set_zrot(&zrot, g.camera.zrot);
		for (int i = 0; i < 4; i++) {
			vector_t t;
			v_transform(&t, &zrot, &v[i].pos);
			v[i].pos = t;
			v_inc(&v[i].pos, &r->pos);
		}
	}

	// Render the bullet trails as very long cuboids.
	for (int i = 0; i < MAX_BULLET_TRAILS; i++) {
		struct bullet_trail *t = &g.bullet_trails[i];
		if (t->end_tick <= g.tick)
			continue;
		int ticks_left = t->end_tick - g.tick;
		v = ge_get_dynamic_vertices(4);
		vector_t color = color_yellow;
		color.v[3] = ticks_left*1.0f / (g.fps/4);
		float dx = t->end.v[0] - t->start.v[0];
		float dy = t->end.v[1] - t->start.v[1];
		vector_t normal;
		normal.v[0] = dy;
		normal.v[1] = -dx;
		normal.v[2] = 0.0f;
		normal.v[3] = 0.0f;
		v_normalize(&normal);
		float radius = 0.01f;
		v_scale(&normal, radius);
		vector_t neg_normal;
		v_sub(&neg_normal, &v_zero, &normal);
		vector_t corners[4] = {
			neg_normal, neg_normal, normal, normal
		};
		corners[0].v[2] += radius;
		corners[1].v[2] -= radius;
		corners[2].v[2] -= radius;
		corners[3].v[2] += radius;
		for (int i = 0; i < 4; i++) {
			v[i*4+0] = (struct vertex) {
				.color = color,
				.s = textures[TEX_WHITE].v[0],
				.t = textures[TEX_WHITE].v[1],
			};
			v[i*4+1] = (struct vertex) {
				.color = color,
				.s = textures[TEX_WHITE].v[0],
				.t = textures[TEX_WHITE].v[3],
			};
			v[i*4+2] = (struct vertex) {
				.color = color,
				.s = textures[TEX_WHITE].v[2],
				.t = textures[TEX_WHITE].v[3],
			};
			v[i*4+3] = (struct vertex) {
				.color = color,
				.s = textures[TEX_WHITE].v[2],
				.t = textures[TEX_WHITE].v[1],
			};
			v_add(&v[i*4+0].pos, &t->start, &corners[i]);
			v_add(&v[i*4+1].pos, &t->start, &corners[(i+1)%4]);
			v_add(&v[i*4+2].pos, &t->end, &corners[(i+1)%4]);
			v_add(&v[i*4+3].pos, &t->end, &corners[i]);
		}
	}

	// Render the hand.
	v = ge_get_dynamic_vertices(1);
	vector_t hand_color = (vector_t) {{ 0.0f, 0.2f, 0.0f, 1.0f }};
	v[0] = (struct vertex) {
		.pos = (vector_t) {{ 0.0f, -0.1f, -0.05f, 1.0f }},
		.color = hand_color,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[3],
	};
	v[1] = (struct vertex) {
		.pos = (vector_t) {{ 0.0f, -0.1f, 0.0f, 1.0f }},
		.color = hand_color,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[1],
	};
	v[2] = (struct vertex) {
		.pos = (vector_t) {{ 0.25f, -0.1f, 0.0f, 1.0f }},
		.color = hand_color,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[1],
	};
	v[3] = (struct vertex) {
		.pos = (vector_t) {{ 0.25f, -0.1f, -0.05f, 1.0f }},
		.color = hand_color,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[3],
	};
	matrix_t trm, tmp_m, yrot, zrot, trans;
	m_set_yrot(&yrot, g.camera.xrot);
	m_set_zrot(&zrot, g.camera.zrot);
	m_set_translate(&trans, &g.camera.pos);
	m_mult(&tmp_m, &zrot, &yrot);
	m_mult(&trm, &trans, &tmp_m);
	for (int i = 0; i < 4; i++) {
		vector_t t = v[i].pos;
		v_transform(&v[i].pos, &trm, &t);
	}

	// Render the barrel.
	struct weapon_state *ws = &g.player.weapons[g.player.weapon_current];
	struct weapon_desc *wd = &weapons[ws->id];
	float bstart = (wd->barrel == WEAPON_BARREL_LONG) ? 0.15f : 0.20f;
	v = ge_get_dynamic_vertices(1);
	v[0] = (struct vertex) {
		.pos = (vector_t) {{ bstart, -0.099f, -0.02f, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[3],
	};
	v[1] = (struct vertex) {
		.pos = (vector_t) {{ bstart, -0.099f, -0.01f, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[1],
	};
	v[2] = (struct vertex) {
		.pos = (vector_t) {{ 0.25f, -0.099f, -0.01f, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[1],
	};
	v[3] = (struct vertex) {
		.pos = (vector_t) {{ 0.25f, -0.099f, -0.02f, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[3],
	};
	for (int i = 0; i < 4; i++) {
		vector_t t = v[i].pos;
		v_transform(&v[i].pos, &trm, &t);
	}

	// Render the gun's ammo.
	float eh = -0.20 * sqrf(1.0f-weapons_get_visibility());
	v = ge_get_dynamic_vertices(1);
	float ammo_f = 0.0002f;
	float ammo_h = -0.02f - wd->ammo_capacity*6.0f*ammo_f;
	v[0] = (struct vertex) {
		.pos = (vector_t) {{ bstart+0.01f, -0.099f, eh+ammo_h, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[3],
	};
	v[1] = (struct vertex) {
		.pos = (vector_t) {{ bstart+0.01f, -0.099f, eh-0.02f, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[1],
	};
	v[2] = (struct vertex) {
		.pos = (vector_t) {{
			bstart+0.01f+ammo_f*50.0f, -0.099f, eh-0.02f, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[1],
	};
	v[3] = (struct vertex) {
		.pos = (vector_t) {{
			bstart+0.01f+ammo_f*50.0f, -0.099f, eh+ammo_h, 1.0f }},
		.color = color_black,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[3],
	};
	for (int i = 0; i < 4; i++) {
		vector_t t = v[i].pos;
		v_transform(&v[i].pos, &trm, &t);
	}
	v = ge_get_dynamic_vertices(1);
	int tex = TEX_AMMO_VERTICAL;
	int ammo = g.player.weapons[g.player.weapon_current].ammo;
	float ammo_t2 = ammo/30.0f*(textures[tex].v[3]-textures[tex].v[1]);
	ammo_t2 += textures[tex].v[1];
	ammo_h = -0.02f - ammo*6.0f*ammo_f;
	v[0] = (struct vertex) {
		.pos = (vector_t) {{ bstart+0.01f, -0.0989f, eh+ammo_h, 1.0f }},
		.color = color_white,
		.s = textures[tex].v[2],
		.t = textures[tex].v[1],
	};
	v[1] = (struct vertex) {
		.pos = (vector_t) {{ bstart+0.01f, -0.0989f, eh+-0.02f, 1.0f }},
		.color = color_white,
		.s = textures[tex].v[2],
		.t = ammo_t2,
	};
	v[2] = (struct vertex) {
		.pos = (vector_t) {{
			bstart+0.01f+ammo_f*50.0f, -0.0989f, eh+-0.02f, 1.0f }},
		.color = color_white,
		.s = textures[tex].v[0],
		.t = ammo_t2,
	};
	v[3] = (struct vertex) {
		.pos = (vector_t) {{
			bstart+0.01f+ammo_f*50.0f, -0.0989f, eh+ammo_h, 1.0f }},
		.color = color_white,
		.s = textures[tex].v[0],
		.t = textures[tex].v[1],
	};
	for (int i = 0; i < 4; i++) {
		vector_t t = v[i].pos;
		v_transform(&v[i].pos, &trm, &t);
	}

	// Render the ammo stats.
	const char *str;
	int spare = g.player.weapons[g.player.weapon_current].spare;
	str = qprintf("ammo: %d/%d", ammo, spare);
	font_draw(0, window_height-font_y, &color_black, str);

	// Render the crosshair.
	float maxdim = maxf(window_width, window_height);
	float cw = g.player.crosshair_angle/g.fov * maxdim;
	float ch = g.player.crosshair_angle/g.fov * maxdim;
	v = ge_get_ortho_vertices(1);
	vector_t crosshair_color = color_white;
	crosshair_color.v[3] = 0.5f;
	v[0] = (struct vertex) {
		.pos = (vector_t) {{
			(window_width - cw)/2.0f,
			(window_height - ch)/2.0f,
			0.0f,
			1.0f,
		}},
		.color = crosshair_color,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[1],
	};
	v[1] = (struct vertex) {
		.pos = (vector_t) {{
			(window_width - cw)/2.0f,
			(window_height + ch)/2.0f,
			0.0f,
			1.0f,
		}},
		.color = crosshair_color,
		.s = textures[TEX_WHITE].v[0],
		.t = textures[TEX_WHITE].v[3],
	};
	v[2] = (struct vertex) {
		.pos = (vector_t) {{
			(window_width + cw)/2.0f,
			(window_height + ch)/2.0f,
			0.0f,
			1.0f,
		}},
		.color = crosshair_color,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[3],
	};
	v[3] = (struct vertex) {
		.pos = (vector_t) {{
			(window_width + cw)/2.0f,
			(window_height - ch)/2.0f,
			0.0f,
			1.0f,
		}},
		.color = crosshair_color,
		.s = textures[TEX_WHITE].v[2],
		.t = textures[TEX_WHITE].v[1],
	};
}

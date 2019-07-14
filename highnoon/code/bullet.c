#include "headers.h"

void bullet_add(int shooter, const vec_t *pos, const vec_t *dir)
{
	assert(fabs(v_magn(dir) - 1.0f) < 0.001f);

	int i;
	for (i = 0; i < BULLETS_MAX; ++i) {
		if (!state.bullets[i].active)
			break;
	}
	HANDLE_CASE(i == BULLETS_MAX);
	struct bullet *b = &state.bullets[i];

	b->active = true;
	b->disable_next_frame = false;
	b->shooter = shooter;
	b->pos = *pos;
	b->dir = *dir;
	b->speed = state.bullet_speed;
}

void bullet_tick(struct bullet *b)
{
	assert(b->active);
	if (b->disable_next_frame) {
		b->active = false;
		return;
	}

	vec_t newpos = b->dir;
	v_scale(&newpos, b->speed);
	v_inc(&newpos, &b->pos);

	bool found_target = false;

	int min_i = -1;
	float min_dist = 1.0e9f;
	for (int i = 0; i < state.animals_count; ++i) {
		struct animal *a = &state.animals[i];
		rect_t line = { b->pos, b->dir };
		float speed = b->speed;
		if (animal_ishit(a, &line, &speed)) {
			found_target = true;
			if (speed < min_dist) {
				min_dist = speed;
				min_i = i;
			}
		}
	}

	if (found_target) {
		assert(min_i != -1);
		animal_kill(&state.animals[min_i]);
		vec_t blood_pos = b->dir;
		v_scale(&blood_pos, sqrtf(min_dist));
		v_inc(&blood_pos, &b->pos);
		blood_add(&blood_pos);
		newpos = blood_pos;
		b->disable_next_frame = true;
	}

	for (int i = 0; !found_target && i < PLAYERS_MAX; ++i) {
		if (!state.inputs_active[i])
			continue;
		struct gunman *gm = &state.gunmen[i];
		rect_t line = { b->pos, b->dir };
		if (gunman_ishit(gm, &line, b->speed)) {
			newpos = line.a;
			line.a = b->pos;
			gunman_hit(gm, &line, b->speed);
			b->disable_next_frame = true;
			found_target = true;
		}
	}

	b->pos = newpos;
	if (fabs(b->pos.x) > 30.0f || fabs(b->pos.y) > 30.0f)
		b->active = false;
}

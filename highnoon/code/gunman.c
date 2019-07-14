#include "headers.h"

#define _pixel2meter (1.0 / 100.0f)
#define _tex2meter (TEX_SIZE * _pixel2meter)

enum { _FRAMES_ROTATING = 6 };
enum { _HITPOINTS_MAX = 8 };
enum _hitpoint_type { _HITPOINT_CIRCLE, _HITPOINT_LINE };

static int _snd_shoot;
static int _snd_dry;
static int _snd_open;
static int _snd_close;
static int _snd_load;
static int _snd_eject;

static rect_t _armtex;
static vec_t _armsize;
static const vec_t _armcenter = { 13 * _pixel2meter, 4 * _pixel2meter };

static rect_t _revtex;
static vec_t _revsize;
static vec_t _handcenter = { 5 * _pixel2meter, 62 * _pixel2meter };
static vec_t _revhold = { 12 * _pixel2meter, 10 * _pixel2meter };
static vec_t _shootpoint = { 20 * _pixel2meter, 48 * _pixel2meter };

static rect_t _barreltex;
static vec_t _barrelsize;
static float _barrelradius;

static vec_t _slotsize;
static rect_t _slottex[3];
static float _slotradius;
static const float _slotdist = 0.60f;

struct _hitpoint_desc {
	enum _hitpoint_type type;
	enum gunman_state transition;
	float params[4];
};

struct _state_desc {
	vec_t size;
	vec_t armpos;
	rect_t tex;
	// How much to add to healthy gunman.pos when we are transitioning into
	// this state
	vec_t armdelta;
	int hitpoints_count;
	struct _hitpoint_desc hitpoints[_HITPOINTS_MAX];
};

struct _state_desc _states[GMS_SIZE];

static void _load_tex(enum gunman_state state, const char *tname)
{
	rect_t tex;
	tex_get_rect(tname, &tex);
	_states[state].tex = tex;
	_states[state].size.x = tex.b.x - tex.a.x;
	_states[state].size.y = tex.b.y - tex.a.y;
	v_scale(&_states[state].size, _tex2meter);
}

static void _change_state(struct gunman *gm, enum gunman_state ns)
{
	float m = gm->dir;
	enum gunman_state os = gm->state;
	gm->pos.x += m*_states[ns].armdelta.x - m*_states[os].armdelta.x;
	gm->pos.y += _states[ns].armdelta.y - _states[os].armdelta.y;
	gm->state = ns;
}

bool gunman_isdead(const struct gunman *gm)
{
	return gm->state == GMS_DEAD_FRONT || gm->state == GMS_DEAD_BACK;
}

void gunman_init(void)
{
	puts("Initializing the gunman subsystem");
	_snd_shoot = audio_getid("gunshot.wav");
	_snd_dry = audio_getid("gunshot_dry.wav");
	_snd_open = audio_getid("gun_open.wav");
	_snd_close = audio_getid("gun_close.wav");
	_snd_load = audio_getid("gun_load.wav");
	_snd_eject = audio_getid("gun_eject.wav");

	float dx = -_armcenter.x+_handcenter.x-_revhold.x+_shootpoint.x;
	assert(fabsf(dx) < 0.001);

	tex_get_rect("man_arm.png", &_armtex);
	_armsize.x = _armtex.b.x - _armtex.a.x;
	_armsize.y = _armtex.b.y - _armtex.a.y;
	v_scale(&_armsize, _tex2meter);

	tex_get_rect("revolver.png", &_revtex);
	_revsize.x = _revtex.b.x - _revtex.a.x;
	_revsize.y = _revtex.b.y - _revtex.a.y;
	v_scale(&_revsize, _tex2meter);

	tex_get_rect("barrel.png", &_barreltex);
	_barrelsize.x = _barreltex.b.x - _barreltex.a.x;
	_barrelsize.y = _barreltex.b.y - _barreltex.a.y;
	v_scale(&_barrelsize, _tex2meter);
	_barrelradius = _barrelsize.x / 2;

	tex_get_rect("slot_empty.png", &_slottex[0]);
	tex_get_rect("slot_used.png", &_slottex[1]);
	tex_get_rect("slot_ready.png", &_slottex[2]);
	_slotsize.x = _slottex[0].b.x - _slottex[0].a.x;
	_slotsize.y = _slottex[0].b.y - _slottex[0].a.y;
	v_scale(&_slotsize, _tex2meter);
	_slotradius = _slotsize.x / 2;

	puts("\tLoading gunman.desc");
	FILE *f = fopen("gunman.desc", "r");
	HANDLE_CASE(f == NULL);
	int state;
	char tex[MAXSTR];

	while (fscanf(f, "%d %60s", &state, tex) == 2) {
		_load_tex(state, tex);
		int arm_x, arm_y;
		HANDLE_CASE(fscanf(f, "%d %d", &arm_x, &arm_y) != 2);
		_states[state].armpos.x = arm_x * _pixel2meter;
		_states[state].armpos.y = arm_y * _pixel2meter;
		int arm_dx, arm_dy;
		HANDLE_CASE(fscanf(f, "%d %d", &arm_dx, &arm_dy) != 2);
		_states[state].armdelta.x = arm_dx * _pixel2meter;
		_states[state].armdelta.y = -arm_dy * _pixel2meter;
	}
	HANDLE_CASE(fclose(f) != 0);

	puts("\tLoading hitpoints.desc");
	f = fopen("hitpoints.desc", "r");
	HANDLE_CASE(f == NULL);
	int transition;
	while (fscanf(f, "%d %d", &state, &transition) == 2) {
		struct _state_desc *s = &_states[state];
		struct _hitpoint_desc *hd = &s->hitpoints[s->hitpoints_count++];
		HANDLE_CASE(s->hitpoints_count > _HITPOINTS_MAX);
		hd->transition = transition;
		char type[MAXSTR];
		HANDLE_CASE(fscanf(f, "%60s", type) != 1);
		int param_cnt;
		if (strcmp(type, "circle") == 0) {
			hd->type = _HITPOINT_CIRCLE;
			param_cnt = 3;
		} else if (strcmp(type, "line") == 0) {
			hd->type = _HITPOINT_LINE;
			param_cnt = 4;
		} else {
			HANDLE_CASE(false);
		}
		for (int i = 0; i < param_cnt; ++i) {
			HANDLE_CASE(fscanf(f, "%f", &hd->params[i]) != 1);
			hd->params[i] *= _pixel2meter;
		}
	}
	HANDLE_CASE(fclose(f) != 0);
}

void gunman_destroy(void)
{
}

void gunman_setup(struct gunman *gm, int player_id, int pos)
{
	assert(pos >= 0 && pos < 6);
	gm->player_id = player_id;
	gm->state = GMS_HEALTHY;
	gm->penalty = false;
	gm->field_pos = pos;
	gm->dir = (pos%2 == 0 ? 1.0f : -1.0f);
	gm->gunangle = 0.0f;
	for (int i = 0; i < GUNSLOTS; ++i)
		gm->barrel_state[i] = GUNSLOT_READY;
	gm->rotating_frames_left = 0;
	gm->in_reload_mode = false;

	switch (pos) {
	case 0: case 2: case 4:
		gm->pos.x = 3.0f;
		gm->barrel_pos.x = 0.5;
		break;
	case 1: case 3: case 5:
		gm->pos.x = screen_width - 3.0f;
		gm->barrel_pos.x = screen_width - 0.5;
		break;
	}

	float sy = _states[GMS_HEALTHY].size.y;
	float hpy = _states[GMS_HEALTHY].armpos.y;
	float mid = screen_height/2 - sy/2.0f;
	float top = screen_height - sy - 0.5f;
	gm->pos.y = 0.5f + sy - hpy;
	gm->barrel_pos.y = 0.5f + _barrelsize.y / 2.0f;
	switch (pos) {
	case 0: case 1:
		break;
	case 2: case 3:
		gm->pos.y += mid;
		gm->barrel_pos.y += mid;
		break;
		break;
	case 4: case 5:
		gm->pos.y += top;
		gm->barrel_pos.y += top;
		break;
	}
}

void gunman_aim(struct gunman *gunman, const vec_t *target)
{
	vec_t v;
	v_sub(&v, target, &gunman->pos);
	gunman->gunangle = atan2f(v.y, v.x) + M_PI_2;
}

static void _rotate_barrel(struct gunman *gm)
{
	gm->rotating_frames_left = _FRAMES_ROTATING;
}

static void _click_in_barrel(struct gunman *gunman)
{
	vec_t d_barrel;
	v_sub(&d_barrel, &state.mouses[gunman->player_id], &gunman->barrel_pos);
	if (v_magn(&d_barrel) < sqrf(_slotradius)) {
		if (gunman->in_reload_mode) {
			audio_play(_snd_close);
			gunman->in_reload_mode = false;
		} else {
			audio_play(_snd_open);
			gunman->in_reload_mode = true;
		}
		return;
	}

	if (!gunman->in_reload_mode)
		return;

	float dang = 2.0f * M_PI / GUNSLOTS;
	for (int i = 0; i < GUNSLOTS; ++i) {
		float ang = i*dang;
		vec_t pos = { 0.00f, _slotdist*_barrelsize.y/2.0f };
		v_rotate(&pos, ang);
		vec_t dm;
		v_sub(&dm, &d_barrel, &pos);
		if (v_magn(&dm) < sqrf(_slotradius)) {
			if (gunman->barrel_state[i] == GUNSLOT_EMPTY) {
				audio_play(_snd_load);
				gunman->barrel_state[i] = GUNSLOT_READY;
			} else {
				audio_play(_snd_eject);
				gunman->barrel_state[i] = GUNSLOT_EMPTY;
			}
		}
	}
}

static bool _shoot(struct gunman *gunman, rect_t *result)
{
	if (gunman->barrel_state[0] != GUNSLOT_READY) {
		audio_play(_snd_dry);
		_rotate_barrel(gunman);
		return false;
	}

	float sr_sx = screen_width/2.0f - state.shoot_range_width/2.0f;
	float sr_ex = screen_width/2.0f + state.shoot_range_width/2.0f;
	bool outside_range = result->a.x < sr_sx || sr_ex < result->a.x;

	if (outside_range && !state.shoot_outside_range)
		return false;

	// The bullet's entry to the world
	result->a.x = 0.0f;
	result->a.y = _armcenter.y - _handcenter.y + _revhold.y - _shootpoint.y;
	v_rotate(&result->a, gunman->gunangle);
	v_inc(&result->a, &gunman->pos);
	muzzle_flash_add(gunman->gunangle, &result->a);

	// The bullet's direction
	float a = gunman->gunangle - M_PI_2;
	result->b = (vec_t) { cosf(a), sinf(a) };

	audio_play(_snd_shoot);
	gunman->barrel_state[0] = GUNSLOT_USED;
	_rotate_barrel(gunman);

	if (outside_range)
		return false;
	return true;
}

bool gunman_click(struct gunman *gunman, rect_t *result)
{
	if (gunman_isdead(gunman))
		return false;
	if (gunman->rotating_frames_left > 0)
		return false;

	vec_t d_bar;
	v_sub(&d_bar, &state.mouses[gunman->player_id], &gunman->barrel_pos);
	if (!gunman->in_reload_mode && v_magn(&d_bar) > sqrf(_barrelradius)) {
		return _shoot(gunman, result);
	} else {
		if (state.reload_allowed)
			_click_in_barrel(gunman);
		return false;
	}
}

void gunman_draw(struct gunman *gunman)
{
	struct graphics_rect_desc desc;
	rect_t r;

	// Draw the main body
	desc.color = &player_colors[gunman->player_id];
	desc.uv = &_states[gunman->state].tex;
	desc.pos = &gunman->pos;
	const struct _state_desc *gs = &_states[gunman->state];
	r.a = (vec_t) { gunman->dir * -gs->armpos.x, gs->armpos.y };
	r.b.x = r.a.x + gunman->dir * gs->size.x;
	r.b.y = r.a.y - gs->size.y;
	desc.rect = &r;
	graphics_draw_rect(&desc);

	if (!gunman_isdead(gunman)) {
		// Draw the arm
		desc.uv = &_armtex;
		desc.pos = &gunman->pos;
		r.a.x = gunman->dir * -_armcenter.x;
		r.a.y = _armcenter.y;
		r.b.x = r.a.x + gunman->dir * _armsize.x;
		r.b.y = r.a.y - _armsize.y;
		graphics_draw_rect_rot(&desc, gunman->gunangle);

		// Draw the gun
		desc.color = &color_white;
		desc.uv = &_revtex;
		desc.pos = &gunman->pos;
		r.a.x = gunman->dir * (-_armcenter.x+_handcenter.x-_revhold.x);
		r.a.y = _armcenter.y - _handcenter.y + _revhold.y;
		r.b.x = r.a.x + gunman->dir * _revsize.x;
		r.b.y = r.a.y - _revsize.y;
		graphics_draw_rect_rot(&desc, gunman->gunangle);

	}

	// Draw the barrel
	desc.color = &color_white;
	desc.uv = &_barreltex;
	desc.pos = &gunman->barrel_pos;
	r.a.x = -_barrelsize.x / 2.0f;
	r.a.y =  _barrelsize.y / 2.0f;
	r.b.x =  _barrelsize.x / 2.0f;
	r.b.y = -_barrelsize.y / 2.0f;
	desc.rect = &r;
	graphics_draw_rect(&desc);

	// Draw the slots
	desc.color = &color_white;
	r.a.x = -_slotsize.x / 2.0f;
	r.a.y =  _slotsize.y / 2.0f;
	r.b.x =  _slotsize.x / 2.0f;
	r.b.y = -_slotsize.y / 2.0f;
	desc.rect = &r;
	vec_t pos;
	desc.pos = &pos;
	float dang = 2.0f * M_PI / GUNSLOTS;
	float extra = 0.0f;
	if (gunman->rotating_frames_left > 0) {
		float amount = _FRAMES_ROTATING - gunman->rotating_frames_left;
		extra = - amount / _FRAMES_ROTATING * dang;
	}
	for (int i = 0; i < GUNSLOTS; ++i) {
		desc.uv = &_slottex[gunman->barrel_state[i]];
		float ang = i*dang + extra;
		pos = (vec_t) { 0.00f, _slotdist*_barrelsize.y/2.0f };
		v_rotate(&pos, ang);
		v_inc(&pos, &gunman->barrel_pos);
		graphics_draw_rect_rot(&desc, ang);
	}

	// Draw the middle if in reload mode
	if (gunman->in_reload_mode) {
		desc.uv = &_slottex[GUNSLOT_EMPTY];
		desc.pos = &gunman->barrel_pos;
		r.a.x = -_slotsize.x / 2.0f;
		r.a.y =  _slotsize.y / 2.0f;
		r.b.x =  _slotsize.x / 2.0f;
		r.b.y = -_slotsize.y / 2.0f;
		desc.rect = &r;
		graphics_draw_rect(&desc);
	}
}

void gunman_tick(struct gunman *gunman)
{
	if (false) {
		struct _state_desc *sd = &_states[gunman->state];
		vec_t *arm = &sd->armpos;
		float m = gunman->dir;
		for (int i = 0; i < sd->hitpoints_count; ++i) {
			struct _hitpoint_desc *hd = &sd->hitpoints[i];
			if (hd->type == _HITPOINT_LINE) {
				rect_t r;
				r.a.x = m*hd->params[0];
				r.a.y =  -hd->params[1];
				r.b.x = m*hd->params[2];
				r.b.y =  -hd->params[3];
				vec_t offset;
				offset.x = gunman->pos.x - m*arm->x;
				offset.y = gunman->pos.y + arm->y;
				v_inc(&r.a, &offset);
				v_inc(&r.b, &offset);
				add_debugline(&r);
			}
		}
	}

	if (gunman->rotating_frames_left > 0) {
		gunman->rotating_frames_left -= 1;
		if (gunman->rotating_frames_left == 0) {
			enum gunslot_state *bs = gunman->barrel_state;
			enum gunslot_state first = bs[0];
			for (int i = 1; i < GUNSLOTS; ++i)
				bs[i-1] = bs[i];
			bs[GUNSLOTS-1] = first;
		}
	}

	if (gunman->in_reload_mode) {
		vec_t d_barrel = state.mouses[gunman->player_id];
		v_dec(&d_barrel, &gunman->barrel_pos);
		if (v_magn(&d_barrel) > sqrf(_barrelradius)) {
			v_normalize(&d_barrel);
			v_scale(&d_barrel, _barrelradius);
			v_inc(&d_barrel, &gunman->barrel_pos);
			state.mouses[gunman->player_id] = d_barrel;
		}
	}
}

// Returns the closest point on the line. line must be normalized.
static float _point_on_line(const rect_t *line, const vec_t *point)
{
	vec_t pt;
	v_sub(&pt, point, &line->a);
	return v_dot(&pt, &line->b);
}

// line must be normalized.
static void _line_on_line(vec_t *res, const rect_t *line, const rect_t *other)
{
	float x1 = line->a.x;
	float y1 = line->a.y;
	float x2 = x1 + line->b.x;
	float y2 = y1 + line->b.y;
	float x3 = other->a.x;
	float y3 = other->a.y;
	float x4 = other->b.x;
	float y4 = other->b.y;

	float denom = (x1-x2)*(y3-y4) - (y1-y2)*(x3-x4);
	if (fabs(denom) < 0.001f) {
		*res = (vec_t) { 100.0f, 100.0f };
		return;
	}
	float x = ((x1*y2-y1*x2)*(x3-x4) - (x1-x2)*(x3*y4-y3*x4)) / denom;
	float y = ((x1*y2-y1*x2)*(y3-y4) - (y1-y2)*(x3*y4-y3*x4)) / denom;
	*res = (vec_t) { x, y };
}

float _hit(const struct gunman *gunman, const rect_t *line, float len, int hdi)
{
	struct _state_desc *sd = &_states[gunman->state];
	vec_t *arm = &sd->armpos;
	float m = gunman->dir;
	struct _hitpoint_desc *hd = &sd->hitpoints[hdi];
	if (hd->type == _HITPOINT_CIRCLE) {
		// Determine the hit circle's world coordinates
		float radius = hd->params[2];
		vec_t center;
		center.x = m*hd->params[0] + gunman->pos.x - m*arm->x;
		center.y = -hd->params[1] + gunman->pos.y + arm->y;

		// Find the closest point to the circle on line
		float f = _point_on_line(line, &center);
		if (f < 0.0f || f > len)
			return -1.0f;

		// Calculate that point and check its distance
		vec_t diff = line->b;
		v_scale(&diff, f);
		v_inc(&diff, &line->a);
		v_dec(&diff, &center);
		float dist2 = v_magn(&diff);
		if (dist2 < radius*radius)
			return f*f;
		else
			return -1.0f;
	} else {
		assert(hd->type == _HITPOINT_LINE);

		// Determine the hit line's world coordinates
		rect_t other;
		other.a = (vec_t) { m*hd->params[0], -hd->params[1] };
		other.b = (vec_t) { m*hd->params[2], -hd->params[3] };
		vec_t other_vec;
		v_sub(&other_vec, &other.b, &other.a);
		float other_len = v_magn(&other_vec);
		vec_t offset;
		offset.x = gunman->pos.x - m*arm->x;
		offset.y = gunman->pos.y + arm->y;
		v_inc(&other.a, &offset);
		v_inc(&other.b, &offset);

		// Determine the common point
		vec_t common;
		_line_on_line(&common, line, &other);

		// Check whether this point is inside the bounds of the
		// two lines
		offset = line->b;
		v_scale(&offset, len);
		v_inc(&offset, &line->a);
		vec_t d1, d2, d3, d4;
		float m1, m2, m3, m4;
		v_sub(&d1, &line->a, &common);
		v_sub(&d2, &offset, &common);
		v_sub(&d3, &other.a, &common);
		v_sub(&d4, &other.b, &common);
		m1 = v_magn(&d1);
		m2 = v_magn(&d2);
		m3 = v_magn(&d3);
		m4 = v_magn(&d4);
		if (m1 > len*len || m2 > len*len)
			return -1.0f;
		if (m3 > other_len || m4 > other_len)
			return -1.0f;
		return m1;
	}
}

bool gunman_ishit(const struct gunman *gunman, rect_t *line, float len)
{
	assert(fabs(v_magn(&line->b) - 1.0f) < 0.001f);

	struct _state_desc *sd = &_states[gunman->state];
	for (int i = 0; i < sd->hitpoints_count; ++i) {
		float hp = _hit(gunman, line, len, i);
		if (-0.0001f < hp && hp < len*len) {
			vec_t pt = line->b;
			v_scale(&pt, sqrtf(hp));
			v_inc(&pt, &line->a);
			line->a = pt;
			return true;
		}
	}

	return false;
}

void gunman_hit(const struct gunman *gunman, const rect_t *line, float len)
{
	assert(fabs(v_magn(&line->b) - 1.0f) < 0.001f);

	struct _state_desc *sd = &_states[gunman->state];
	int first = -1;
	float first_val = 100.0f;
	for (int i = 0; i < sd->hitpoints_count; ++i) {
		float hp = _hit(gunman, line, len, i);
		if (-0.0001f < hp && hp < len*len) {
			if (first == -1 || hp < first_val) {
				first = i;
				first_val = hp;
			}
		}
	}
	assert(first != -1);

	vec_t blood_pos = line->b;
	v_scale(&blood_pos, sqrtf(first_val));
	v_inc(&blood_pos, &line->a);
	blood_add(&blood_pos);

	enum gunman_state st;
	st = _states[gunman->state].hitpoints[first].transition;
	_change_state((struct gunman*)gunman, st);
}

bool gunman_inbarrel(const struct gunman *gunman)
{
	vec_t d_bar;
	v_sub(&d_bar, &state.mouses[gunman->player_id], &gunman->barrel_pos);
	return v_magn(&d_bar) < sqrf(_barrelradius);
}

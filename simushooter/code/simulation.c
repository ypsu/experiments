#include "headers.h"

enum {
	_HIT_NONE = 0,
	_HIT_LEGS = 64,
	_HIT_BODY = 128,
	_HIT_HEAD = 192,
};

char
map[MAP_TILES][MAP_TILES+1];

float
map_height;

const float
cover_height = 1.0f;

const char
_hitmap_header[] =
"P5\n"
"1024 1024\n"
"255\n";

static struct {
	unsigned char hitmap[TEX_SIZE][TEX_SIZE];
	// bpath means bullet path. It is a list of [x,y] pairs starting from
	// the bullet's starting point to its end or its first obstacle. This
	// array contains the result of the latest bpath_calc call.
	int bpath_length, bpath_traced_bullet;
	int bpath[3*TEX_SIZE][2];
	int bpath_above_cover_flip[2];
	bool bpath_above_cover;
}
_v;

// Are tile(x,y) and the surrounding tiles within bounds?
static bool
_tile_exists(int x, int y)
{
	return (x >= 1 && x < MAP_TILES-1 && y >= 1 && y < MAP_TILES-1);
}

static bool
_tile_walkable(int x, int y)
{
	return map[y][x] == '.';
}

static void
_tile_register(int x, int y, int robot_id)
{
	short *ts = g.tile_robots[y][x];
	int sz = 0;
	while (ts[sz] != -1) {
		if (ts[sz] == robot_id) {
			return;
		}
		sz += 1;
		CHECK(sz < MAX_ROBOTS_IN_TILE);
	}
	ts[sz] = robot_id;
}

static void
_tile_unregister(int x, int y, int robot_id)
{
	short *ts = g.tile_robots[y][x];
	int pos = -1;
	int sz = 0;
	while (ts[sz] != -1) {
		if (ts[sz] == robot_id) {
			CHECK(pos == -1);
			pos = sz;
		}
		sz += 1;
		CHECK(sz <= MAX_ROBOTS_IN_TILE);
	}
	if (pos == -1)
		return;
	ts[pos] = ts[sz-1];
	ts[sz-1] = -1;
}

void
sim_init(void)
{
	puts("Initializing the simulation subsystem.");
	memset(g.tile_robots, -1, sizeof g.tile_robots);
	FILE *f;

	puts("Loading assets/world.map.");
	CHECK((f = fopen("assets/world.map", "r")) != NULL);
	for (int i = 0; i < MAP_TILES; i++) {
		char buf[700];
		CHECK(fscanf(f, "%600s", buf) == 1);
		CHECK(strlen(buf) == MAP_TILES);
		strcpy(map[MAP_TILES-i-1], buf);
	}
	CHECK(fclose(f) == 0);

	puts("Loading assets/hitmap.pgm.");
	int len;
	const char *buf = util_load_file("assets/hitmap.pgm", &len);
	CHECK(len == strlen(_hitmap_header) + TEX_SIZE*TEX_SIZE);
	buf += strlen(_hitmap_header);
	memcpy(_v.hitmap, buf, TEX_SIZE*TEX_SIZE);

	g.player.weapons[0].id = W_ASSAULT_RIFLE;
	g.player.weapons[0].ammo=weapons[g.player.weapons[0].id].ammo_capacity;
	g.player.weapons[0].spare = 5;
	g.player.weapons[1].id = W_REVOLVER;
	g.player.weapons[1].ammo=weapons[g.player.weapons[1].id].ammo_capacity;
	g.player.weapons[1].spare = 10;
}

void
sim_stop(void)
{
}

static void
_move_player(void)
{
	struct robot *r = &g.robots[0];
	r->new_pos = r->pos;
	if (g.camera.free_flight) {
		return;
	}

	r->new_zrot = r->zrot - input_mdx / 400.0f;
	r->new_xrot = r->xrot - input_mdy / 400.0f;
	r->new_xrot = clampf(r->new_xrot, -M_PI/3.0f, M_PI/3.0f);
	g.camera.zrot = r->new_zrot;
	g.camera.xrot = r->new_xrot;

	vector_t look_forward = v_zero, look_left = v_zero;
	float speed = 6*g.dt;
	look_forward.v[0] = speed * cosf(g.camera.zrot);
	look_forward.v[1] = speed * sinf(g.camera.zrot);
	look_left.v[0] = -look_forward.v[1];
	look_left.v[1] =  look_forward.v[0];
	vector_t move = v_zero;
	g.player.moving = false;
	if (input_is_pressed(SDL_SCANCODE_W)) {
		v_inc(&move, &look_forward);
		g.player.moving = true;
	}
	if (input_is_pressed(SDL_SCANCODE_S)) {
		v_dec(&move, &look_forward);
		g.player.moving = true;
	}
	if (input_is_pressed(SDL_SCANCODE_A)) {
		v_inc(&move, &look_left);
		g.player.moving = true;
	}
	if (input_is_pressed(SDL_SCANCODE_D)) {
		v_dec(&move, &look_left);
		g.player.moving = true;
	}
	v_inc(&r->new_pos, &move);
}

static void
_move_robots(void)
{
	_move_player();

	float maxd = 30.0f * g.dt;
	float q = powf(0.01f, g.dt);
	for (int i = 1; i < MAX_ROBOTS; i++) {
		struct robot *r = &g.robots[i];
		if (!r->active) continue;
		float dxx = r->pos.v[0] - g.robots[0].new_pos.v[0];
		float dyy = r->pos.v[1] - g.robots[0].new_pos.v[1];
		if (fabs(dxx) < 0.1f && fabs(dyy) < 0.1f) continue;
		vector_t d = {{ dxx, dyy, 0.0f, 0.0f }};
		v_normalize(&d);
		v_scale(&d, (r->legsout?3.5f:7.0f)*g.dt);
		if (fabs(r->momentum.v[0])+fabs(r->momentum.v[1]) >= 0.001) {
			v_inc(&d, &r->momentum);
			v_scale(&r->momentum, q);
			v_dec(&d, &r->momentum);
		}
		float dlen = sqrtf(v_magn(&d));
		if (dlen > maxd) {
			// We can't move too much to avoid skipping tiles.
			v_scale(&d, maxd / dlen);
		}
		vector_t newpos = r->pos;
		v_dec(&newpos, &d);
		// Only move to the new tile if it's not crowded.
		int nx = (int)newpos.v[0];
		int ny = (int)newpos.v[1];
		if (g.tile_robots[ny][nx][3] == -1) {
			r->new_pos = newpos;
		}
	}
}

static void
_fix_collisions(void)
{
	for (int i = 0; i < MAX_ROBOTS; i++) {
		struct robot *r = &g.robots[i];
		if (!r->active) continue;
		int ox = (int)r->pos.v[0];
		int oy = (int)r->pos.v[1];
		int nx = (int)r->new_pos.v[0];
		int ny = (int)r->new_pos.v[1];
		CHECK(_tile_exists(nx, ny));
		int dy[9] = { -1, +1,  0,  0, -1, -1, +1, +1,  0 };
		int dx[9] = {  0,  0, -1, +1, -1, +1, -1, +1,  0 };
		// Let's avoid other robots.
		for (int d = 0; d < 9; d++) {
			int xx = ox + dx[d];
			int yy = oy + dy[d];
			if (!_tile_walkable(xx, yy))
				continue;
			short *ts = g.tile_robots[yy][xx];
			for (int j = 0; ts[j] != -1; j++) {
				// It's enough to consider each pair once.
				if (ts[j] <= i) continue;
				struct robot *r2 = &g.robots[ts[j]];
				float dxx = r2->new_pos.v[0] - r->new_pos.v[0];
				float dyy = r2->new_pos.v[1] - r->new_pos.v[1];
				float dist = dxx*dxx + dyy*dyy;
				float r0 = robot_radius;
				float r1 = robot_radius;
				float needed_dist = sqrf(r0+r1);
				if (dist >= needed_dist) continue;
				CHECK(dxx != 0.0f || dyy != 0.0f);
				vector_t v = {{ dxx, dyy, 0.0f, 0.0f }};
				float sqd = sqrtf(dist);
				float scale;
				scale = (sqrtf(needed_dist) - sqd) / sqd / 2.0f;
				v_scale(&v, scale);
				v_dec(&r->new_pos, &v);
				v_inc(&r2->new_pos, &v);
			}
		}
		// Let's fix up the position so it isn't colliding the map
		// geometry. Add small epsilon to the displacement to ensure
		// that we don't get weird behavior by displacing the robot too
		// close to the walls.
		float rr = robot_radius + 0.001f;
		for (int d = 0; d < 9; d++) {
			int xx = ox + dx[d];
			int yy = oy + dy[d];
			if (_tile_walkable(xx, yy))
				continue;
			if (!robot_touches_tile(i, xx, yy))
				continue;
			float center_dx = fabs(xx+0.5f-r->pos.v[0]);
			float center_dy = fabs(yy+0.5f-r->pos.v[1]);
			if (center_dx > center_dy) {
				if (xx < ox) r->new_pos.v[0] = ox + rr;
				if (xx > ox) r->new_pos.v[0] = ox + 1.0f - rr;
			} else {
				if (yy < oy) r->new_pos.v[1] = oy + rr;
				if (yy > oy) r->new_pos.v[1] = oy + 1.0f - rr;
			}
		}
	}
}

static void
_apply_new_positions(void)
{
	int dy[9] = { -1, +1,  0,  0, -1, -1, +1, +1,  0 };
	int dx[9] = {  0,  0, -1, +1, -1, +1, -1, +1,  0 };
	for (int i = 0; i < MAX_ROBOTS; i++) {
		struct robot *r = &g.robots[i];
		if (!r->active) {
			continue;
		}
		r->pos = r->new_pos;
		r->xrot = r->new_xrot;
		r->zrot = r->new_zrot;
		for (int d = 0; d < 9; d++) {
			int xx = (int)r->pos.v[0] + dx[d];
			int yy = (int)r->pos.v[1] + dy[d];
			CHECK(_tile_exists(xx, yy));
			if (!_tile_walkable(xx, yy))
				continue;
			if (robot_touches_tile(i, xx, yy)) {
				_tile_register(xx, yy, i);
			} else {
				_tile_unregister(xx, yy, i);
			}
		}
	}

	if (!g.camera.free_flight) {
		struct robot *r = &g.robots[0];
		vector_t camera_height = v_up;
		v_scale(&camera_height, 0.90f*robot_height);
		g.camera.pos = r->new_pos;
		v_inc(&g.camera.pos, &camera_height);
	}
}

static void
_shoot(void)
{
	struct weapon_state *w = &g.player.weapons[g.player.weapon_current];
	if (input_is_clicked(SDL_BUTTON_RIGHT)) {
		g.fov = M_PI / 3.0f;
	} else {
		g.fov = M_PI / 2.0f;
	}

	if (g.player.gun_busy_until_tick == g.tick) {
		enum gun_state gs = g.player.gun_state;
		if (gs == GUN_IN_RECOIL || gs == GUN_RAISING) {
			g.player.gun_state = GUN_READY;
		} else if (gs == GUN_RELOADING) {
			audio_play(SND_GUN_LOAD);
			weapons_animate(GUN_RAISING, g.fps / 5);
		} else if (gs == GUN_SWITCHING) {
			g.player.weapon_current ^= 1;
			weapons_animate(GUN_RAISING, g.fps / 5);
		} else if (gs == GUN_LOWERING_FOR_RELOAD) {
			weapons_reload();
		} else if (gs == GUN_LOWERING_FOR_SWITCH) {
			weapons_animate(GUN_SWITCHING, g.fps / 5);
		}
	}
	if (g.player.gun_state != GUN_READY) {
		return;
	}
	if (input_is_pressed(SDL_SCANCODE_R)) {
		if (w->spare > 0) {
			weapons_animate(GUN_LOWERING_FOR_RELOAD, g.fps / 5);
		}
		return;
	}
	if (input_is_pressed(SDL_SCANCODE_F)) {
		weapons_animate(GUN_LOWERING_FOR_SWITCH, g.fps / 5);
		return;
	}
	const struct weapon_desc *wd = &weapons[w->id];
	int btn = SDL_BUTTON_LEFT;
	bool fired = input_was_clicked(btn);
	bool firing = input_is_clicked(btn);
	if (wd->operation != WEAPON_OPERATION_AUTO && !fired) {
		return;
	}
	if (wd->operation == WEAPON_OPERATION_AUTO) {
		if (!firing || (w->ammo == 0 && !fired)) {
			return;
		}
	}
	if (w->ammo <= 2) {
		audio_play(SND_GUN_BERETTA_EMPTY);
		if (w->ammo == 0) {
			return;
		}
	}
	audio_play(SND_GUN_BERETTA_FIRE);
	w->ammo -= 1;
	g.player.gun_busy_until_tick = g.tick + g.fps/10;
	g.player.gun_state = GUN_IN_RECOIL;
	int bullets = 1;
	if (w->id == W_REVOLVER) bullets = 12;
	for (int i = 0; i < bullets; i++) {
		struct robot *r = &g.robots[0];
		float xrot = r->xrot + (randf()-0.5f)*g.player.crosshair_angle;
		float zrot = r->zrot + (randf()-0.5f)*g.player.crosshair_angle;
		vector_t look_forward = v_zero;
		look_forward.v[0] = cosf(xrot);
		look_forward.v[2] = sinf(xrot);
		vector_t start = r->pos;
		start.v[2] += 0.90f*robot_height;
		start.v[3] = 1.0f;
		matrix_t m;
		m_set_zrot(&m, zrot);
		vector_t tmp = look_forward;
		v_transform(&look_forward, &m, &tmp);
		v_scale(&look_forward, MAP_TILES);
		vector_t end = start;
		v_inc(&end, &look_forward);
		int nid = g.bullet_trails_next++;
		g.bullet_trails_next %= MAX_BULLET_TRAILS;
		g.bullet_trails[nid].start = start;
		g.bullet_trails[nid].end = end;
		g.bullet_trails[nid].start_tick = g.tick;
		g.bullet_trails[nid].end_tick = g.tick + g.fps/4;
		g.bullet_trails[nid].shooter_robot = 0;
	}
}

static void
_bot_shoot(void)
{
	return;
	if (g.tick%g.fps != 0 || !g.robots[2].active) return;
	vector_t look_forward = v_zero;
	float xang = randf()*0.10f - 0.05f;
	look_forward.v[0] = cosf(xang);
	look_forward.v[2] = sinf(xang);
	vector_t start = g.robots[2].pos;
	start.v[2] += 0.75f*robot_height;
	start.v[3] = 1.0f;
	float dxx = g.robots[2].pos.v[0] - g.robots[0].pos.v[0];
	float dyy = g.robots[2].pos.v[1] - g.robots[0].pos.v[1];
	float zang = atan2f(dyy, dxx) + M_PI + randf()*0.10-0.05f;
	matrix_t m;
	m_set_zrot(&m, zang);
	vector_t tmp = look_forward;
	v_transform(&look_forward, &m, &tmp);
	v_scale(&look_forward, MAP_TILES);
	vector_t end = start;
	v_inc(&end, &look_forward);
	g.bullet_trails[g.bullet_trails_next].start = start;
	g.bullet_trails[g.bullet_trails_next].end = end;
	g.bullet_trails[g.bullet_trails_next].start_tick = g.tick;
	g.bullet_trails[g.bullet_trails_next].end_tick = g.tick + g.fps/4;
	g.bullet_trails[g.bullet_trails_next].shooter_robot = 1;
	g.bullet_trails_next = (g.bullet_trails_next+1) % MAX_BULLET_TRAILS;
	audio_play_at(SND_GUN_BERETTA_FIRE, &start);
}

static bool
_bpath_add_tile(int x, int y)
{
	const int *flip_tile = _v.bpath_above_cover_flip;
	if (x == flip_tile[0] && y == flip_tile[1]) {
		_v.bpath_above_cover ^= 1;
	}
	if (map[y][x] == '.' || (_v.bpath_above_cover && map[y][x] == 'c')) {
		_v.bpath[_v.bpath_length][0] = x;
		_v.bpath[_v.bpath_length][1] = y;
		_v.bpath_length += 1;
		return true;
	}
	int i = _v.bpath_length;
	CHECK(i != 0);
	struct bullet_trail *t = &g.bullet_trails[_v.bpath_traced_bullet];
	float f = 0.0f;
	if (x == _v.bpath[i-1][0] + 1) {
		float nx = x - t->start.v[0];
		float ox = t->end.v[0] - t->start.v[0];
		f = nx / ox;
	} else if (x == _v.bpath[i-1][0] - 1) {
		float nx = x+1 - t->start.v[0];
		float ox = t->end.v[0] - t->start.v[0];
		f = nx / ox;
	} else if (y == _v.bpath[i-1][1] + 1) {
		float ny = y - t->start.v[1];
		float oy = t->end.v[1] - t->start.v[1];
		f = ny / oy;
	} else if (y == _v.bpath[i-1][1] - 1) {
		float ny = y+1 - t->start.v[1];
		float oy = t->end.v[1] - t->start.v[1];
		f = ny / oy;
	} else {
		CHECK(false);
	}
	vector_t d = t->end;
	v_dec(&d, &t->start);
	v_scale(&d, f);
	t->end = t->start;
	v_inc(&t->end, &d);
	return false;
}

static bool
_bpath_add_line(int y, int x0, int x1, int sx)
{
	CHECK(_v.bpath_length + abs(x0-x1) + 3 < 3*TEX_SIZE);
	for (int x = x0; x != x1+sx; x += sx) {
		if (!_bpath_add_tile(x, y)) {
			return false;
		}
	}
	return true;
}

static void
_bpath_calc(int bulletid)
{
	_v.bpath_length = 0;
	_v.bpath_traced_bullet = bulletid;
	struct bullet_trail *t = &g.bullet_trails[bulletid];

	// Ensure that floors/ceiling limits the bullet.
	float newz = t->end.v[2];
	if (newz < 0.0f) {
		newz = 0.0f;
	} else if (newz > map_height) {
		newz = map_height;
	}
	if (newz != t->end.v[2]) {
		vector_t d = t->end;
		v_dec(&d, &t->start);
		float f = fabs(newz-t->start.v[2]) / fabs(d.v[2]);
		v_scale(&d, f);
		t->end = t->start;
		v_inc(&t->end, &d);
	}

	// Ensure that the cover stops the bullet when needed.
	_v.bpath_above_cover_flip[0] = -1;
	_v.bpath_above_cover_flip[1] = -1;
	_v.bpath_above_cover = false;
	if (t->start.v[2] > cover_height && newz > cover_height) {
		_v.bpath_above_cover = true;
	}
	if ((t->start.v[2] > cover_height) != (newz > cover_height)) {
		newz = 1.0f;
		vector_t d = t->end;
		v_dec(&d, &t->start);
		float f = fabs(newz-t->start.v[2]) / fabs(d.v[2]);
		v_scale(&d, f);
		vector_t cross_point = t->start;
		v_inc(&cross_point, &d);
		int x = (int)cross_point.v[0];
		int y = (int)cross_point.v[1];
		if (_tile_exists(x, y)) {
			if (map[y][x] == 'c') {
				// If cross-point tile is cover, no need to
				// flip, that endpoint will be the end.
				t->end = cross_point;
			} else {
				_v.bpath_above_cover_flip[0] = x;
				_v.bpath_above_cover_flip[1] = y;
			}
		}
		if (t->start.v[2] > cover_height) {
			_v.bpath_above_cover = true;
		}
	}

	// Trace the tile's touched by the bullet.
	float x0 = t->start.v[0];
	float y0 = t->start.v[1];
	float x1 = t->end.v[0];
	float y1 = t->end.v[1];
	int sx = (x0 < x1) ? +1 : -1;
	bool ok = true;
	if ((int)y0 == (int)y1) {
		ok = _bpath_add_line(y0, (int)x0, (int)x1, sx);
	} else if (y0 < y1) {
		float dx = x1 - x0;
		float dy1 = 1.0f / (y1 - y0);
		float y = floorf(y0 + 1.0f);
		float x = x0 + (y-y0)*dy1*dx;
		ok = _bpath_add_line((int)y0, (int)x0, (int)x, sx);
		if (!ok) {
			return;
		}
		int k = (int)y1 - (int)y0;
		float tx = dy1 * dx;
		for (int i = 1; i < k; i++) {
			float nx = x + tx;
			ok = _bpath_add_line((int)y, (int)x, (int)nx, sx);
			if (!ok) {
				return;
			}
			x = nx;
			y += 1.0f;
		}
		float nx = x + (y1-y)*dy1*dx;
		ok = _bpath_add_line((int)y, (int)x, (int)nx, sx);
	} else if (y0 > y1) {
		float dx = x1 - x0;
		float dy1 = 1.0f / (y0 - y1);
		float y = ceilf(y0 - 1.0f);
		float x = x0 + (y0-y)*dy1*dx;
		ok = _bpath_add_line((float)y0, (int)x0, (int)x, sx);
		if (!ok) {
			return;
		}
		int k = (int)y0 - (int)y1;
		float tx = dy1 * dx;
		for (int i = 1; i < k; i++) {
			float nx = x + tx;
			y -= 1.0f;
			ok = _bpath_add_line((int)y, (int)x, (int)nx, sx);
			if (!ok) {
				return;
			}
			x = nx;
		}
		float nx = x + (y-y1)*dy1*dx;
		ok = _bpath_add_line((int)y1, (int)x, (int)nx, sx);
	}
}

static void
_resolve_shots(void)
{
	static int hits = 0;
	static char last_dist[MAXSTR];
	static char tex_coords[MAXSTR];
	static char pixels[MAXSTR];
	static int hits_legs, hits_body, hits_head;
	short robot_last_bullet[MAX_ROBOTS];
	memset(robot_last_bullet, -1, sizeof robot_last_bullet);
	for (int i = 0; i < MAX_BULLET_TRAILS; i++) {
		struct bullet_trail *b = &g.bullet_trails[i];
		if (b->start_tick != g.tick) {
			continue;
		}
		if (b->shooter_robot != 0) {
			continue;
		}
		robot_last_bullet[b->shooter_robot] = i;
		_bpath_calc(i);
		int check_count = 0;
		short robots_to_check[MAX_ROBOTS];
		for (int j = 0; j < _v.bpath_length; j++) {
			int x = _v.bpath[j][0];
			int y = _v.bpath[j][1];
			const short *ts = g.tile_robots[y][x];
			for (int k = 0; ts[k] != -1; k++) {
				int r = ts[k];
				if (robot_last_bullet[r] == i) {
					continue;
				}
				robot_last_bullet[r] = i;
				robots_to_check[check_count++] = r;
			}
		}
		for (int j = 0; j < check_count; j++) {
			int rid = robots_to_check[j];
			struct robot *r = &g.robots[rid];
			if (!r->active) {
				continue;
			}
			vector_t n = b->end;
			v_dec(&n, &b->start);
			n.v[2] = 0.0f;
			float t = sqrtf(v_magn(&n));
			if (t < 0.001f) {
				continue;
			}
			v_scale(&n, 1.0f / t);
			vector_t robot_diff = r->pos;
			v_dec(&robot_diff, &b->start);
			robot_diff.v[2] = 0.0f;
			float dot = v_dot(&robot_diff, &n);
			dot = clampf(dot, 0.0f, t);
			vector_t p = n;
			v_scale(&p, dot);
			// p is now the closest point on line (0 + t*n)
			// to the robot.
			v_dec(&p, &robot_diff);
			float d = sqrtf(v_magn(&p));
			strcpy(last_dist, qprintf("last_dist: %f", d));
			if (d > robot_radius) {
				continue;
			}
			float tx = d / robot_radius / 2.0f;
			float cross = n.v[0]*p.v[1] - n.v[1]*p.v[0];
			if (cross > 0.0f) {
				tx = 0.5f - tx;
			} else {
				tx = 0.5f + tx;
			}
			float ty = dot*(b->end.v[2]-b->start.v[2])/t;
			ty += b->start.v[2];
			if (ty < 0.0f || ty > robot_height) {
				continue;
			}
			hits += 1;
			ty /= robot_height;
			const char fmt[] = "tex: [%0.2f, %0.2f]";
			strcpy(tex_coords, qprintf(fmt, tx, ty));

			const vector_t *tex = &textures[TEX_ROBOT];
			float tw = tex->v[2] - tex->v[0];
			float th = tex->v[3] - tex->v[1];
			int px = (int)((tex->v[0] + tx*tw)*TEX_SIZE);
			int py = (int)((tex->v[3] - ty*th)*TEX_SIZE);;
			px = clampi(px, 0, TEX_SIZE-1);
			py = clampi(py, 0, TEX_SIZE-1);
			strcpy(pixels, qprintf("pixels: %4d,%4d", px, py));

			int hit = _v.hitmap[py][px];
			if (hit == _HIT_LEGS) {
				hits_legs += 1;
			} else if (hit == _HIT_BODY) {
				hits_body += 1;
			} else if (hit == _HIT_HEAD) {
				hits_head += 1;
			} else if (hit != _HIT_NONE) {
				CHECK(false);
			} else {
				continue;
			}

			if (hit == _HIT_HEAD) {
				// Remove the robot now that it has been shot.
				int dy[9] = {-1, +1,  0,  0, -1, -1, +1, +1, 0};
				int dx[9] = { 0,  0, -1, +1, -1, +1, -1, +1, 0};
				for (int d = 0; d < 9; d++) {
					int xx = (int)r->pos.v[0] + dx[d];
					int yy = (int)r->pos.v[1] + dy[d];
					CHECK(_tile_exists(xx, yy));
					if (!_tile_walkable(xx, yy)) continue;
					if (robot_touches_tile(rid, xx, yy)) {
						_tile_unregister(xx, yy, rid);
					}
				}
				r->active = false;
			} else if (hit == _HIT_BODY) {
				vector_t v = v_zero;
				v.v[0] = b->end.v[0] - b->start.v[0];
				v.v[1] = b->end.v[1] - b->start.v[1];
				v_normalize(&v);
				v_scale(&v, 5.0f);
				v_dec(&r->momentum, &v);
			} else if (hit == _HIT_LEGS) {
				r->legsout = true;
			}
		}
	}
	const char *str = qprintf("hits: %d", hits);
	font_draw(0, window_height-3*font_y, &color_black, str);
	font_draw(0, window_height-4*font_y, &color_black, last_dist);
	font_draw(0, window_height-5*font_y, &color_black, tex_coords);
	font_draw(0, window_height-6*font_y, &color_black, pixels);
	const char fmt[] = "hits: head:%d body:%d legs:%d";
	str = qprintf(fmt, hits_head, hits_body, hits_legs);
	font_draw(0, window_height-7*font_y, &color_black, str);
}

void
simulate(void)
{
	_move_robots();
	_fix_collisions();
	_apply_new_positions();
	_shoot();
	_bot_shoot();
	_resolve_shots();

	struct weapon_state *ws = &g.player.weapons[g.player.weapon_current];
	struct weapon_desc *wd = &weapons[ws->id];
	float target;
	if (g.player.moving) {
		target = (wd->accuracy_lo + wd->accuracy_hi) / 2.0f;
	} else {
		target = wd->accuracy_lo;
	}
	if (g.player.crosshair_angle > target) {
		g.player.crosshair_angle -= g.dt/180.0f * M_PI;
		if (g.player.crosshair_angle < target) {
			g.player.crosshair_angle = target;
		}
	} else if (g.player.crosshair_angle < target) {
		g.player.crosshair_angle += g.dt/180.0f * M_PI;
		if (g.player.crosshair_angle > target) {
			g.player.crosshair_angle = target;
		}
	}
}

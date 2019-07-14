#include "headers.h"

static int _buzz_id;
static rect_t _tex_dot;

static void _entry_init(int eid, const char *name, const char *hint)
{
	assert(eid < MENU_ENTRIES);
	assert(strlen(name) <= MENU_NAMELEN);
	assert(strlen(hint) < MAXSTR);
	state.menu.entries[eid].active = true;
	strcpy(state.menu.entries[eid].name, name);
	strcpy(state.menu.entries[eid].hint, hint);
}

static void _align_options(int eid)
{
	assert(eid < MENU_ENTRIES);
	struct menu_entry *e = &state.menu.entries[eid];
	if (e->options_count == 0)
		return;
	int single_length = (MAXSTR - MENU_NAMELEN - 2) / e->options_count;
	for (int i = 0; i < e->options_count; ++i) {
		int offset = 2 + MENU_NAMELEN + i*single_length;
		int unused = single_length - e->options[i].length;
		offset += unused/2;
		e->options[i].offset = offset;
	}
}

static void _add_option(int eid, const char *name)
{
	assert(eid < MENU_ENTRIES);
	assert(strlen(name) <= MENU_NAMELEN);
	struct menu_entry *e = &state.menu.entries[eid];
	assert(e->options_count < MENU_MAXOPTS);
	struct menu_option *o = &e->options[e->options_count++];
	strcpy(o->name, name);
	o->length = strlen(name);
}

static bool _player_in_menu(int pid)
{
	vec_t sz = { font_x * MAXSTR, (1.5f*font_y) * MENU_ENTRIES };
	vec_t mid = { screen_width/2.0f, screen_height/2.0f };
	vec_t start, end;
	start = (vec_t) { mid.x - sz.x/2.0f, mid.y + sz.y/2.0f };
	end   = (vec_t) { mid.x + sz.x/2.0f, mid.y - sz.y/2.0f };

	float mx = state.mouses[pid].x;
	float my = state.mouses[pid].y;

	if (mx < start.x || end.x < mx)
		return false;
	if (my < end.y || start.y < my)
		return false;
	return true;
}

static void _reflect_menu(void)
{
	struct menu_entry *e = state.menu.entries;

	for (int i = 1; i < MENU_ENTRIES-4; ++i)
		e[i].active = false;

	if (e[0].selected_option == 0) {
		e[ 1].active = true;
		e[ 8].selected_option = 0;
		e[ 9].selected_option = 1;
	} else {
		for (int i = 2; i < MENU_ENTRIES-1; ++i)
			e[i].active = true;
	}

	state.reload_allowed = e[9].selected_option;
	float bs[3] = { 9.0f, 1.0f, 0.3f };
	state.bullet_speed = bs[e[10].selected_option];
	float sw = screen_width;
	float srw[6] = { sw/48.0f,sw/24.0f,sw/12.0f,sw/6.0f,sw/3.0f,sw/1.5f };
	state.shoot_range_width = srw[e[11].selected_option];
	state.shoot_outside_range = e[12].selected_option;

	for (int pid = 0; pid < PLAYERS_MAX; ++pid) {
		if (!e[pid+2].active)
			continue;
		int side = e[pid+2].selected_option/3;
		int level = e[pid+2].selected_option%3;
		int pos = level*2 + side;
		gunman_setup(&state.gunmen[pid], pid, pos);
	}
}

static void _start_game(void)
{
	state.gamehint[0] = 0;
	struct menu_entry *e = state.menu.entries;
	if (e[0].selected_option == 1) {
		state.practice_mode = false;
		state.practice_won = false;
		int active_players = 0;
		int pos_mask = 0;
		int teams_mask = 0;
		for (int i = 0; i < PLAYERS_MAX; ++i) {
			state.gunmen[i].active = false;
			if (!state.inputs_active[i])
				continue;
			if (!_player_in_menu(i))
				continue;
			state.gunmen[i].active = true;
			active_players += 1;
			pos_mask |= 1 << e[i+2].selected_option;
			teams_mask |= 1 << (e[i+2].selected_option/3);
		}
		if (active_players < 2) {
			set_status(0, "At least two players needed!");
			return;
		}
		if (__builtin_popcount(pos_mask) != active_players) {
			set_status(0, "Players need different positions!");
			return;
		}
		if (teams_mask != 3) {
			set_status(0, "Both teams need players!");
			return;
		}
		state.animals_count = 0;
		if (e[8].selected_option == 1)
			state.animals_count = 3 + rand()%4;
		else if (e[8].selected_option == 2)
			state.animals_count = 12 + rand()%5;
		for (int i = 0; i < state.animals_count; ++i) {
			if (rand()%2 == 0)
				animal_setup_rabbit(&state.animals[i]);
			else
				animal_setup_bird(&state.animals[i]);
		}
		state.menu_active = false;
	} else {
		state.practice_mode = true;
		state.practice_won = false;
		for (int i = 0; i < PLAYERS_MAX; ++i) {
			state.gunmen[i].active = false;
			if (!state.inputs_active[i])
				continue;
			if (!_player_in_menu(i))
				continue;
			if (!state.buttons_pressed[i])
				continue;
			gunman_setup(&state.gunmen[i], i, 2);
			state.gunmen[i].active = true;
		}

		if (e[1].selected_option == 0) {
			const char *s;
			s = "HINT: Move the mouse above barrel to get ready.";
			strcpy(state.gamehint, s);
			state.animals_count = 1;
			animal_setup_target(&state.animals[0]);
			state.time_left = 30*60;
		} else if (e[1].selected_option == 1) {
			const char *s;
			s = "HINT: You can shoot only in the green area.";
			strcpy(state.gamehint, s);
			state.animals_count = 3;
			animal_setup_target(&state.animals[0]);
			animal_setup_target(&state.animals[1]);
			animal_setup_target(&state.animals[2]);
			state.animals[1].pos.x = screen_width - 1.0f;
			state.animals[1].pos.y = 1.0f;
			state.animals[2].pos.x = screen_width - 1.0f;
			state.animals[2].pos.y = screen_height - 1.0f;
			state.time_left = 30*60;
		} else if (e[1].selected_option == 2) {
			const char *s;
			s = "HINT: To reload click on the barrel";
			strcpy(state.gamehint, s);
			s = " center then on the slots.";
			strcat(state.gamehint, s);
			state.animals_count = 7;
			for (int i = 0; i < 7; ++i)
				animal_setup_target(&state.animals[i]);
			state.time_left = 60*60;
		} else if (e[1].selected_option == 3) {
			state.time_left = 10*60;
			state.animals_count = 1;
			animal_setup_rabbit(&state.animals[0]);
		} else if (e[1].selected_option == 4) {
			state.time_left = 10*60;
			state.animals_count = 3;
			animal_setup_rabbit(&state.animals[0]);
			animal_setup_rabbit(&state.animals[1]);
			animal_setup_rabbit(&state.animals[2]);
		} else if (e[1].selected_option == 5) {
			state.time_left = 15*60;
			state.animals_count = 6;
			animal_setup_rabbit(&state.animals[0]);
			animal_setup_rabbit(&state.animals[1]);
			animal_setup_rabbit(&state.animals[2]);
			animal_setup_rabbit(&state.animals[3]);
			animal_setup_rabbit(&state.animals[4]);
			animal_setup_rabbit(&state.animals[5]);
		} else if (e[1].selected_option == 6) {
			state.time_left = 10*60;
			state.animals_count = 1;
			animal_setup_bird(&state.animals[0]);
		} else if (e[1].selected_option == 7) {
			state.time_left = 15*60;
			state.animals_count = 3;
			animal_setup_bird(&state.animals[0]);
			animal_setup_bird(&state.animals[1]);
			animal_setup_bird(&state.animals[2]);
		} else if (e[1].selected_option == 8) {
			state.time_left = 10*60;
			state.animals_count = 4;
			animal_setup_rabbit(&state.animals[0]);
			animal_setup_rabbit(&state.animals[1]);
			animal_setup_bird(&state.animals[2]);
			animal_setup_bird(&state.animals[3]);
		} else if (e[1].selected_option == 9) {
			state.time_left = 15*60;
			state.animals_count = 6;
			animal_setup_bird(&state.animals[0]);
			animal_setup_bird(&state.animals[1]);
			animal_setup_bird(&state.animals[2]);
			animal_setup_bird(&state.animals[3]);
			animal_setup_bird(&state.animals[4]);
			animal_setup_bird(&state.animals[5]);
		}
		state.current_level = e[1].selected_option;
		state.menu_active = false;
	}
	state.gamestate = GAMESTATE_GETREADY;
}


void menu_init(void)
{
	const char *hint;

	_buzz_id = audio_getid("buzz.wav");
	_tex_dot = (rect_t) { { 0.5f, 0.5f }, { 0.5f, 0.5f } };
	tex_get("dot.png", &_tex_dot.a.x, &_tex_dot.a.y);
	tex_get("dot.png", &_tex_dot.b.x, &_tex_dot.b.y);

	_entry_init( 0, "Game mode      :", "");
	_add_option( 0, "practice");
	_add_option( 0, "versus");

	_entry_init( 1, "Level          :", "");
	_add_option( 1, "0");
	_add_option( 1, "1");
	_add_option( 1, "2");
	_add_option( 1, "3");
	_add_option( 1, "4");
	_add_option( 1, "5");
	_add_option( 1, "6");
	_add_option( 1, "7");
	_add_option( 1, "8");
	_add_option( 1, "9");

	hint = "L/R: left/right team, B/M/T: top/middle/bottom position";
	for (int p = 0; p < PLAYERS_MAX; ++p) {
		const char *name;
		name = qprintf("P%d position    :", p+1);
		_entry_init(p+2, name, hint);
		_add_option(p+2, "LB");
		_add_option(p+2, "LM");
		_add_option(p+2, "LT");
		_add_option(p+2, "RB");
		_add_option(p+2, "RM");
		_add_option(p+2, "RT");
		state.menu.entries[p+2].selected_option = p/2%3 + p%2*3;
	}

	_entry_init( 8, "Animals        :", "");
	_add_option( 8, "none");
	_add_option( 8, "a few");
	_add_option( 8, "a lot");

	_entry_init( 9, "Allow reload   :", "");
	_add_option( 9, "no");
	_add_option( 9, "yes");
	state.menu.entries[ 9].selected_option = 1;

	_entry_init(10, "Bullet speed   :", "");
	_add_option(10, "instant");
	_add_option(10, "fast");
	_add_option(10, "slow");
	state.menu.entries[10].selected_option = 1;

	_entry_init(11, "Shoot range    :", "");
	_add_option(11, "ultra");
	_add_option(11, "thin");
	_add_option(11, "tight");
	_add_option(11, "narrow");
	_add_option(11, "medium");
	_add_option(11, "wide");
	state.menu.entries[11].selected_option = 4;

	hint = "You can't kill with blind shot but it still wastes an ammo";
	_entry_init(12, "Outside range  :", hint);
	_add_option(12, "no action");
	_add_option(12, "blind shot");
	state.menu.entries[12].selected_option = 1;

	_entry_init(13, "START", "Click to start the game!");

	for (int i = 0; i < MENU_ENTRIES; ++i)
		_align_options(i);

	_reflect_menu();
}

void menu_destroy(void)
{
}

void menu_render(void)
{
	struct graphics_rect_desc d;
	struct color col;
	rect_t r;
	vec_t sz = { font_x * MAXSTR, (1.5f*font_y) * MENU_ENTRIES };
	vec_t mid = { screen_width/2.0f, screen_height/2.0f };
	vec_t start, end;
	start = (vec_t) { mid.x - sz.x/2.0f, mid.y + sz.y/2.0f };
	end   = (vec_t) { mid.x + sz.x/2.0f, mid.y - sz.y/2.0f };

	// Draw menu background
	col = (struct color) { 0.5f, 0.0f, 0.0f, 1.0f };
	r = (rect_t) { start, end };
	d.color = &col;
	d.uv = &_tex_dot;
	d.pos = &v_zero;
	d.rect = &r;
	graphics_draw_rect(&d);

	// Draw the entries
	float x = start.x + font_x/2.0f;
	float y = start.y - 0.25f*font_y;
	for (int i = 0; i < MENU_ENTRIES; ++i, y -= 1.5f*font_y) {
		const struct menu_entry *e = &state.menu.entries[i];
		if (!e->active)
			continue;
		if (i >= 2 && i-2 < PLAYERS_MAX) {
			if (!state.inputs_active[i-2] || !_player_in_menu(i-2))
				continue;
			gunman_draw(&state.gunmen[i-2]);
		}
		font_draw(x, y, &color_white, e->name);

		for (int j = 0; j < e->options_count; ++j) {
			if (i == 1 && j > state.max_level)
				break;
			const struct menu_option *o = &e->options[j];
			if (j == e->selected_option)
				col = color_green;
			else
				col = color_white;
			font_draw(x+font_x*o->offset, y, &col, o->name);
		}

		// Render hint if there's a mouse above this
		for (int j = 0; j < PLAYERS_MAX; ++j) {
			if (!state.inputs_active[j])
				continue;
			float mx = state.mouses[j].x;
			if (!(start.x < mx && mx < end.x))
				continue;

			float ey = y + 0.25f*font_y;
			float sy = ey - 1.5f*font_y;
			float my = state.mouses[j].y;
			if (sy < my && my < ey) {
				float sx = mid.x - strlen(e->hint)*font_x/2.0f;
				font_draw(sx, font_y, &color_white, e->hint);
			}
		}
	}
}

void menu_click(const vec_t *pos)
{
	vec_t sz = { font_x * MAXSTR, (1.5f*font_y) * MENU_ENTRIES };
	vec_t mid = { screen_width/2.0f, screen_height/2.0f };
	vec_t start, end;
	start = (vec_t) { mid.x - sz.x/2.0f, mid.y + sz.y/2.0f };
	end   = (vec_t) { mid.x + sz.x/2.0f, mid.y - sz.y/2.0f };

	if (!(start.x < pos->x && pos->x < end.x))
		return;
	if (!(end.y < pos->y && pos->y < start.y))
		return;
	int cy = floorf((start.y - pos->y - 0.25f*font_y) / 1.5f / font_y);
	int cx = floorf((pos->x - start.x - font_x/2) / font_x);
	if (cy < 0 || cy >= MENU_ENTRIES)
		return;
	if (cy == MENU_ENTRIES-1) {
		_start_game();
	} else {
		struct menu_entry *e = &state.menu.entries[cy];
		for (int i = 0; i < e->options_count; ++i) {
			const struct menu_option *o = &e->options[i];
			if (o->offset <= cx && cx < o->offset+o->length) {
				if (cy == 1 && i > state.max_level)
					continue;
				if (e->selected_option != i)
					audio_play(_buzz_id);
				e->selected_option = i;
				break;
			}
		}
	}
	_reflect_menu();
}

void menu_show(void)
{
	state.menu_active = true;
	_reflect_menu();
}

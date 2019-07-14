#include "headers.h"

static int _audio_bad;
static int _audio_go;
static int _audio_wait;
static int _audio_winleft;
static int _audio_winright;
static int _audio_windraw;
static int _audio_wingood;
static int _audio_winfail;

void game_init(void)
{
	puts("Initializing the game subsystem");
	_audio_bad = audio_getid("bad.wav");
	_audio_go = audio_getid("go.wav");
	_audio_wait = audio_getid("wait.wav");
	_audio_winleft = audio_getid("win_left.wav");
	_audio_winright = audio_getid("win_right.wav");
	_audio_windraw = audio_getid("win_draw.wav");
	_audio_wingood = audio_getid("win_good.wav");
	_audio_winfail = audio_getid("win_fail.wav");
}

void game_destroy(void)
{
}

void game_tick(void)
{
	if (state.menu_active) {
		for (int i = 0; i < PLAYERS_MAX; ++i) {
			if (!state.inputs_active[i])
				continue;
			if (state.buttons_pressed[i])
				menu_click(&state.mouses[i]);
		}
		return;
	}

	for (int i = 0; i < PLAYERS_MAX; ++i) {
		struct gunman *gm = &state.gunmen[i];
		if (!gm->active || !gm->penalty)
			continue;
		bool locked = state.gamestate == GAMESTATE_GUNFIGHT;
		locked &= state.frame_id-state.countdown_start < 240;
		locked |= state.gamestate == GAMESTATE_COUNTDOWN;
		if (locked) {
			state.mouses[i] = gm->barrel_pos;
		}
	}

	for (int i = 0; i < PLAYERS_MAX; ++i) {
		struct gunman *gm = &state.gunmen[i];
		if (!gm->active)
			continue;
		gunman_aim(gm, &state.mouses[i]);
	}

	for (int i = 0; i < state.animals_count; ++i)
		animal_tick(&state.animals[i]);

	if (state.gamestate == GAMESTATE_GUNFIGHT) {
		bool has_left = false;
		bool has_right = false;
		int has_ammo = 0;
		for (int i = 0; i < PLAYERS_MAX; ++i) {
			struct gunman *gm = &state.gunmen[i];
			if (!gm->active)
				continue;
			if (state.buttons_pressed[i]) {
				state.last_shot = state.frame_id;
				rect_t bullet = { state.mouses[i], v_zero };
				if (gunman_click(gm, &bullet)) {
					bullet_add(i, &bullet.a, &bullet.b);
				}
			}
			gunman_tick(gm);
			if (!gunman_isdead(gm)) {
				if (gm->field_pos%2 == 0)
					has_left = true;
				else
					has_right = true;
				if (gm->barrel_state[0] == GUNSLOT_READY) {
					has_ammo += 1;
				}
			}
		}

		bool animal_alive = false;
		for (int i = 0; !animal_alive && i < state.animals_count; ++i) {
			if (state.animals[i].state == ANIMAL_STATE_ALIVE)
				animal_alive = true;
		}

		if (state.practice_mode && animal_alive) {
			state.time_left -= 1;
		}

		bool silence = state.last_shot+120 < state.frame_id;
		bool noammo = !state.reload_allowed && has_ammo == 0;
		bool team_dead = !has_left || !has_right;
		bool vs_ended = !state.practice_mode && (noammo || team_dead);
		bool killed_all = state.practice_mode && !animal_alive;
		bool ended = silence && (vs_ended || killed_all);
		if (state.practice_mode && state.time_left == 0)
			ended = true;
		if (ended) {
			const char *text;
			if (vs_ended) {
				if (has_left && !has_right) {
					audio_play(_audio_winleft);
					text = "L E F T   T E A M   W I N S";
				} else if (!has_left && has_right) {
					audio_play(_audio_winright);
					text = "R I G H T   T E A M   W I N S";
				} else {
					audio_play(_audio_windraw);
					text = "D R A W";
				}
			} else if (killed_all) {
				audio_play(_audio_wingood);
				text = "G O O D   S H O O T I N G";
				state.practice_won = true;
				int nl = state.current_level+1;
				if (nl <= 9 && nl > state.max_level) {
					struct state *s = &state;
					s->max_level = nl;
					s->menu.entries[1].selected_option = nl;
				}
			} else if (state.time_left == 0) {
				audio_play(_audio_winfail);
				text = "F A I L";
			} else {
				assert(false);
			}
			strcpy(state.gameover_text, text);
			state.gamestate = GAMESTATE_GAMEOVER;
		}
	} else if (state.gamestate == GAMESTATE_GETREADY) {
		bool all_ready = true;
		for (int i = 0; i < PLAYERS_MAX; ++i) {
			struct gunman *gm = &state.gunmen[i];
			if (!gm->active)
				continue;
			all_ready = all_ready && gunman_inbarrel(gm);
		}
		if (all_ready) {
			state.gamestate = GAMESTATE_COUNTDOWN;
			state.countdown_start = state.frame_id+1;
		}
	} else if (state.gamestate == GAMESTATE_COUNTDOWN) {
		int frames = state.countdown_start+180 - state.frame_id;
		int second = frames/60 + 1;
		bool all_ok = true;
		for (int i = 0; i < PLAYERS_MAX; ++i) {
			struct gunman *gm = &state.gunmen[i];
			if (!gm->active)
				continue;
			if (!gunman_inbarrel(gm)) {
				if (!state.practice_mode && second < 2) {
					audio_play(_audio_bad);
					gm->penalty = true;
				}
				state.gamestate = GAMESTATE_GETREADY;
				all_ok = false;
			}
		}

		if (all_ok) {
			if (frames == 0) {
				audio_play(_audio_go);
				state.gamestate = GAMESTATE_GUNFIGHT;
			} else if (frames % 60 == 0) {
				audio_play(_audio_wait);
			}
		}
	} else if (state.gamestate == GAMESTATE_GAMEOVER) {
		if (state.last_shot+120 < state.frame_id) {
			for (int i = 0; i < PLAYERS_MAX; ++i) {
				if (state.buttons_pressed[i])
					menu_show();
			}
		}
	}

	for (int i = 0; i < PLAYERS_MAX; ++i) {
		struct gunman *gm = &state.gunmen[i];
		if (!gm->active)
			continue;
		gunman_tick(gm);
	}
	for (int i = 0; i < BULLETS_MAX; ++i) {
		if (state.bullets[i].active)
			bullet_tick(&state.bullets[i]);
	}
}

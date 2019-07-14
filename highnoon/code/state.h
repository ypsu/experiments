#ifndef state_h
#define state_h

// The game's state is fully described by this structure. E.g. if you would save
// this and then later restore it you would get back to the same state you were
// in when you saved it in the first place.

struct state;
extern struct state state;

enum { PLAYERS_MAX = 6 };
enum { BULLETS_MAX = 32 };
enum { STATUS_MSG_MAX = 4 };
enum { DEBUGLINES_MAX = 32 };
enum { BLOOD_SPLASHES = 16 };
enum { MUZZLE_FLASHES = 16 };
enum { ANIMALS_MAX = 32 };

extern struct color player_colors[PLAYERS_MAX];

struct state {
	int frame_id;
	int fps;
	int vertices_last_frame;
	int vertices_drawn;

	int status_msg_last_frame[STATUS_MSG_MAX];
	char status_msg[STATUS_MSG_MAX][MAXSTR];

	bool menu_active;
	bool inputs_active[PLAYERS_MAX];
	vec_t mouses[PLAYERS_MAX];
	int buttons_down[PLAYERS_MAX];
	int buttons_pressed[PLAYERS_MAX];

	enum gamestate gamestate;
	bool practice_mode;
	int time_left;
	int countdown_start;
	int last_shot;
	char gameover_text[MAXSTR];
	char gamehint[MAXSTR];
	bool practice_won;
	int current_level;
	int max_level;

	float bullet_speed;
	bool reload_allowed;
	float shoot_range_width;
	bool shoot_outside_range;
	struct gunman gunmen[PLAYERS_MAX];
	struct bullet bullets[BULLETS_MAX];

	struct blood_splash blood[BLOOD_SPLASHES];
	struct muzzle_flash flashes[MUZZLE_FLASHES];

	int animals_count;
	struct animal animals[ANIMALS_MAX];

	struct menu menu;

	int debuglines_count;
	rect_t debuglines[DEBUGLINES_MAX];
};

void state_init(void);
void state_destroy(void);

#endif

#ifndef gunman_h
#define gunman_h

enum { GUNSLOTS = 6 };

enum gunslot_state { GUNSLOT_EMPTY, GUNSLOT_USED, GUNSLOT_READY };

enum gunman_state {
	GMS_HEALTHY = 0,
	GMS_KNEELING = 1,
	GMS_SITTING = 2,
	GMS_LAYING = 3,
	GMS_DEAD_FRONT = 4,
	GMS_DEAD_BACK = 5,
	GMS_SIZE = 16,
};

struct gunman {
	bool active;

	int player_id;

	enum gunman_state state;

	// If true then the gunman's mouse is locked and starts late
	bool penalty;

	// If it's even then its on the left side, if odd then on the right side
	int field_pos;

	// 1.0 for right facing gunman, -1.0 for left facing one
	float dir;

	// The position around which the hands are rotating
	vec_t pos;

	// The center of the barrel
	vec_t barrel_pos;

	// The angle of the arm (0 means down)
	float gunangle;

	enum gunslot_state barrel_state[GUNSLOTS];

	// Number of frames while the gunman can't shoot again
	int rotating_frames_left;

	// While in reload mode the cursor can't leave the barrel's area
	bool in_reload_mode;
};

void gunman_init(void);
void gunman_destroy(void);
void gunman_setup(struct gunman *gm, int player_id, int pos);
void gunman_aim(struct gunman *gunman, const vec_t *target);
// Returns true if the gunman can shoot
// result.a = in: shoot position, out: the starting point of the bullet
// result.b = the direction of the bullet
bool gunman_click(struct gunman *gunman, rect_t *result);
void gunman_draw(struct gunman *gunman);
void gunman_tick(struct gunman *gunman);
// Returns true iff gunman hits line. line.a is the starting point, line.b is
// the normalized direction of the line, len is the length of the line.
// If hit then line->a will contain the hit point on the return.
bool gunman_ishit(const struct gunman *gunman, rect_t *line, float len);
void gunman_hit(const struct gunman *gunman, const rect_t *line, float len);
// Return true iff the gunman's mouse is in the gunman's barrel.
bool gunman_inbarrel(const struct gunman *gunman);
bool gunman_isdead(const struct gunman *gm);

#endif

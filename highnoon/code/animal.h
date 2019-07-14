#ifndef animal_h
#define animal_h

enum animal_type {
	ANIMAL_TYPE_TARGET,
	ANIMAL_TYPE_BIRD,
	ANIMAL_TYPE_RABBIT,
	ANIMAL_TYPE_SIZE,
};

enum animal_state {
	ANIMAL_STATE_ALIVE,
	ANIMAL_STATE_DEAD,
};

struct animal {
	enum animal_type type;
	enum animal_state state;
	vec_t pos;

	vec_t source;
	vec_t dest;
	float progress;
	float progress_speed;
};

void animal_init(void);
void animal_destroy(void);
void animal_setup_rabbit(struct animal *animal);
void animal_render(const struct animal *animal);
void animal_setup_target(struct animal *animal);
void animal_setup_bird(struct animal *animal);
void animal_setup_rabbit(struct animal *animal);
void animal_tick(struct animal *animal);
// Returns true iff animal hits line. line.a is the starting point, line.b is
// the normalized direction of the line, *len is the length of the line.
// If hit then line->a will contain the hit point on the return and len will
// contain the squared length from line.a to the hitpoint.
bool animal_ishit(const struct animal *animal, rect_t *line, float *len);
void animal_kill(struct animal *animal);

#endif

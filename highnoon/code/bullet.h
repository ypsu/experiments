#ifndef bullet_h
#define bullet_h

struct bullet {
	bool active;
	bool disable_next_frame;
	int shooter;
	vec_t pos;
	vec_t dir; // normalized direction
	float speed;
};

void bullet_add(int shooter, const vec_t *pos, const vec_t *dir);
void bullet_tick(struct bullet *b);

#endif

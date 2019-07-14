#ifndef blood_h
#define blood_h

struct blood_splash {
	int frame_activated;
	vec_t pos;
};

void blood_init(void);
void blood_destroy(void);
void blood_add(const vec_t *pos);

#endif

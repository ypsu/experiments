#ifndef muzzle_flash_f
#define muzzle_flash_h

struct muzzle_flash {
	int frame_activated;
	float angle;
	vec_t pos;
};

void muzzle_flash_init(void);
void muzzle_flash_destroy(void);
void muzzle_flash_add(float angle, const vec_t *pos);

#endif

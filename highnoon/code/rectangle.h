#ifndef rectangle_h
#define rectangle_h

typedef struct rectangle {
	vec_t a, b;
} rect_t;

void r_inc(rect_t *r, const vec_t *v);

#endif

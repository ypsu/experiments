#ifndef vector_h
#define vector_h

typedef struct vector {
	float x, y;
} vec_t;

extern const struct vector v_zero;

void v_inc(vec_t *r, const vec_t *a);
void v_dec(vec_t *r, const vec_t *a);
void v_add(vec_t *r, const vec_t *a, const vec_t *b);
void v_sub(vec_t *r, const vec_t *a, const vec_t *b);
void v_rotate(vec_t *r, float angle);
void v_scale(vec_t *r, float f);
void v_normalize(vec_t *r);
float v_magn(const vec_t *v);
float v_length(const vec_t *v);
float v_dot(const vec_t *a, const vec_t *b);

#endif

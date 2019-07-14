#ifndef graphics_h
#define graphics_h

// This module handles most of the OpenGL API

// The bottom left corner is [0,0] and the top right corner is [screen_width,
// screen_height]. The units are meters.
extern const float screen_width;
extern const float screen_height;

struct vertex {
	vec_t xy;
	vec_t uv;
	struct color color;
};

void graphics_init(void);
void graphics_destroy(void);
void graphics_check_errors(void);
struct vertex *graphics_vertices_get(int n);
void graphics_vertices_draw(void);

struct graphics_rect_desc {
	const rect_t *rect;
	const vec_t *pos;
	const rect_t *uv;
	const struct color *color;
};

void graphics_draw_rect(const struct graphics_rect_desc *desc);
void graphics_draw_rect_rot(const struct graphics_rect_desc *desc, float ang);
void graphics_save_sshot(const char *fname);

#endif

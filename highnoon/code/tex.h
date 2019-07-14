#ifndef tex_h
#define tex_h

// The width and height of the global texture.
enum { TEX_SIZE = 1024 };

void tex_init(void);
void tex_destroy(void);

// Translate a texture's coordinates into the global texture's coordinates.
void tex_get(const char *name, float *u, float *v);

// Get the coordinates of the two corners of a texture into r. The two corners
// are the top left and the bottom right.
void tex_get_rect(const char *name, rect_t *r);

#endif

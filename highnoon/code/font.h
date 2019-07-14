#ifndef font_h
#define font_h

// Fixed width font rendering functions.

// Width and height of one character in meters.
extern float font_x;
extern float font_y;

void font_init(void);
void font_destroy(void);
void font_draw(float x, float y, const struct color *color, const char *str);

#endif

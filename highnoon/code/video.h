#ifndef video_h
#define video_h

// The height and width of the OpenGL context
extern int video_width, video_height;

// This sets up the OpenGL library
void video_init(void);
void video_destroy(void);
// Swap the backbuffer and frontbuffer
void video_swap(void);

#endif

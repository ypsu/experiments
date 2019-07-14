#include "headers.h"

int video_width, video_height;

static EGLDisplay _display;
static EGLSurface _surface;

void video_init(void)
{
	puts("Initializing the video subsystem");
	bcm_host_init();

	puts("\tInitializing dispmanx stuff");
	uint32_t _width, _height;
	HANDLE_CASE(graphics_get_display_size(0, &_width, &_height) < 0);
	int width = _width, height = _height;
	// Force an HD (16:9) aspect ratio
	height = _width * 9 / 16;
	if (height > (int) _height) {
		height = _height;
		width = _height * 16 / 9;
		HANDLE_CASE(width > (int) _width);
	}

	video_width = width;
	video_height = height;
	printf("\tDisplay size: %dx%d\n", width, height);

	static EGL_DISPMANX_WINDOW_T nativewin;
	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = width;
	dst_rect.height = height;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = width << 16;
	src_rect.height = height << 16;

	dispman_display = vc_dispmanx_display_open(0);
	dispman_update = vc_dispmanx_update_start(0);
	{
		DISPMANX_UPDATE_HANDLE_T a = dispman_update;
		DISPMANX_DISPLAY_HANDLE_T b = dispman_display;
		int c = 0;
		VC_RECT_T *d = &dst_rect;
		DISPMANX_RESOURCE_HANDLE_T e = 0;
		VC_RECT_T *f = &src_rect;
		DISPMANX_PROTECTION_T g = DISPMANX_PROTECTION_NONE;
		VC_DISPMANX_ALPHA_T *h = NULL;
		DISPMANX_CLAMP_T *i = NULL;
		DISPMANX_TRANSFORM_T j = 0;
		DISPMANX_ELEMENT_HANDLE_T z;

		z = vc_dispmanx_element_add(a, b, c, d, e, f, g, h, i, j);
		dispman_element = z;
	}
	vc_dispmanx_update_submit_sync(dispman_update);

	nativewin.element = dispman_element;
	nativewin.width = width;
	nativewin.height = height;


	puts("\tInitializing EGL");
	EGLint numConfigs;
	EGLint majorVersion;
	EGLint minorVersion;
	EGLContext context;
	EGLConfig config;
	EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION,
		2,
		EGL_NONE,
		EGL_NONE
	};
	EGLint attribList[] = {
		EGL_RED_SIZE,       8,
		EGL_GREEN_SIZE,     8,
		EGL_BLUE_SIZE,      8,
		EGL_ALPHA_SIZE,     8,
		EGL_DEPTH_SIZE,     24,
		EGL_STENCIL_SIZE,   EGL_DONT_CARE,
		EGL_SAMPLE_BUFFERS, 0,
		EGL_NONE
	};

	_display = eglGetDisplay((EGLNativeDisplayType) NULL);
	EGLDisplay d = _display;
	HANDLE_CASE(_display == EGL_NO_DISPLAY);
	HANDLE_CASE(!eglInitialize(_display, &majorVersion, &minorVersion));
	printf("\tEGL version: %d.%d\n", majorVersion, minorVersion);
	HANDLE_CASE(!eglGetConfigs(_display, NULL, 0, &numConfigs));
	HANDLE_CASE(!eglChooseConfig(d, attribList, &config, 1, &numConfigs));
	_surface = eglCreateWindowSurface(d, config, (void*)&nativewin, NULL);
	HANDLE_CASE(_surface == EGL_NO_SURFACE);
	context = eglCreateContext(d, config, EGL_NO_CONTEXT, contextAttribs);
	HANDLE_CASE(context == EGL_NO_CONTEXT);
	HANDLE_CASE(!eglMakeCurrent(_display, _surface, _surface, context));
}

void video_destroy(void)
{
}

void video_swap(void)
{
	HANDLE_CASE(eglSwapBuffers(_display, _surface) == 0);
}

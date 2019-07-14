#include "headers.h"

static struct {
	SDL_Window *sdl_window;
	SDL_GLContext *sdl_context;
}
_v;

static void
_auto_stop_sdl(void)
{
	SDL_GL_DeleteContext(_v.sdl_context);
	SDL_DestroyWindow(_v.sdl_window);
	SDL_Quit();
}

static void
_init_sdl(void)
{
	puts("Initializing SDL.");
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init() failed: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, -1);
#ifdef PLATFORM_RPI
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
#endif

	SDL_DisplayMode mode;
	CHECK(SDL_GetDesktopDisplayMode(0, &mode) == 0);
	window_width = mode.w;
	window_height = mode.h;
	const char *title = "Urban Blitz";
	int flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL;
	_v.sdl_window = SDL_CreateWindow(title, 0, 0, mode.w, mode.h, flags);
	CHECK(_v.sdl_window != NULL);
	_v.sdl_context = SDL_GL_CreateContext(_v.sdl_window);
	CHECK(_v.sdl_context != NULL);
	CHECK(SDL_GL_SetSwapInterval(1) == 0);
	onexit(_auto_stop_sdl);
}

int
main(void)
{
	_init_sdl();
	audio_init();
	ge_init();
	tex_init();
	ge_load_static_geometry();
	font_init();
	robot_init();
	sim_init();

	g.fps = 60;
	g.dt = 1.0f / g.fps;
	int cur_fps = 0;
	int last_time = time(NULL);
	int start_time = last_time;

	g.camera.pos.v[1] = 12.8f * 0.25f;

	g.robots[0] = (struct robot) {
		true, false,
		{{ 40.0f, 40.0f, 0.0f, 1.0f }},
		{{ 40.0f, 40.0f, 0.0f, 1.0f }},
		0.0f, 0.0f, 0.0f, 0.0f,
		{{ 0.0f, 0.0f, 0.0f, 0.0f }},
	};
	g.player.ammo = 2;

	int s = 1;
	for (int r = 6; r < MAP_TILES && s < MAX_ROBOTS; r += 4) {
		for (int c = 0; c < 35 && s < MAX_ROBOTS; c += 4) {
			if (map[r][c] != '.') {
				continue;
			}
			g.robots[s] = g.robots[0];
			g.robots[s].pos.v[0] = c + 0.5;
			g.robots[s].pos.v[1] = r + 0.5;
			g.robots[s].new_pos.v[0] = c; // TODO: remove
			g.robots[s].new_pos.v[1] = r;
			s += 1;
		}
	}

	input_set_mouse(INPUT_MOUSE_RELATIVE);
	puts("Entering main loop.");
	while (!g.should_quit) {
		g.tick += 1;
		if (g.tick > 1234567890) {
			puts("You are playing for too long. Take a break!\n");
			exit(1);
		}
		int t = time(NULL);
		if (t != last_time && t-start_time >= 2) {
			// Variance at the startup's first second is ignored.
			g.fps = cur_fps;
			if (g.fps < 30) {
				printf("Too low framerate! Weak machine!\n");
				exit(1);
			}
			if (g.fps >= 300) {
				printf("Too high framerate! Enable vsync!\n");
				exit(1);
			}
			g.dt = 1.0f / g.fps;
		}
		if (t != last_time) {
			last_time = t;
			cur_fps = 0;
		}
		cur_fps += 1;
		util_tick();

		SDL_Event ev;
		SDL_PumpEvents();
		input_tick();
		while (SDL_PollEvent(&ev) == 1) {
			switch (ev.type) {
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEWHEEL:
				input_handle_event(&ev);
				break;
			case SDL_QUIT:
				g.should_quit = true;
				break;
			case SDL_WINDOWEVENT:
				t = ev.window.event;
				if (t == SDL_WINDOWEVENT_RESIZED) {
					window_width = ev.window.data1;
					window_height = ev.window.data2;
					ge_reinit_window();
				}
				break;
			}
		}
		audio_tick();

		if (input_was_pressed(SDL_SCANCODE_ESCAPE))
			g.should_quit = true;
		if (input_was_pressed(SDL_SCANCODE_F1))
			g.camera.free_flight = !g.camera.free_flight;

		if (g.camera.free_flight) {
			g.camera.zrot -= input_mdx / 400.0f;
			g.camera.xrot -= input_mdy / 400.0f;
			float limit = M_PI/2.0f;
			g.camera.xrot = clampf(g.camera.xrot, -limit, limit);
			vector_t look_forward = v_zero;
			look_forward.v[0] = cosf(g.camera.zrot);
			look_forward.v[1] = sinf(g.camera.zrot);
			v_scale(&look_forward, 0.25f);
			if (input_is_pressed(SDL_SCANCODE_LSHIFT)) {
				v_scale(&look_forward, 0.05f);
			}
			vector_t look_left = v_zero;
			look_left.v[0] = -look_forward.v[1];
			look_left.v[1] =  look_forward.v[0];
			vector_t look_up = {{ 0.0f, 0.0f, 1.0f, 0.0f }};
			if (input_is_pressed(SDL_SCANCODE_W))
				v_inc(&g.camera.pos, &look_forward);
			if (input_is_pressed(SDL_SCANCODE_S))
				v_dec(&g.camera.pos, &look_forward);
			if (input_is_pressed(SDL_SCANCODE_A))
				v_inc(&g.camera.pos, &look_left);
			if (input_is_pressed(SDL_SCANCODE_D))
				v_dec(&g.camera.pos, &look_left);
			if (input_is_pressed(SDL_SCANCODE_SPACE))
				v_inc(&g.camera.pos, &look_up);
			if (input_is_pressed(SDL_SCANCODE_LCTRL))
				v_dec(&g.camera.pos, &look_up);
		}

		simulate();
		render();

		const char *fmt =
			"mouse = [%0.0f, %0.0f], "
			"d = [%+0.0f, %+0.0f]";
		const char *str;
		str = qprintf(fmt, input_mx, input_my, input_mdx, input_mdy);
		font_draw(0, font_y, &color_black, str);

		const char *fps;
		fps = qprintf("fps = %d", g.fps);
		int xoff = window_width - strlen(fps)*font_x;
		font_draw(xoff, 0, &color_black, fps);

		ge_submit();
		SDL_GL_SwapWindow(_v.sdl_window);
		CHECK(usleep(6000) == 0);
	}

	sim_stop();
	robot_stop();
	font_stop();
	tex_stop();
	ge_stop();
	audio_stop();
	return 0;
}

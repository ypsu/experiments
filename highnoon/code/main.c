#include "headers.h"

static bool _should_quit;

static struct termios _orig_termios;

static void _terminal_reset(void)
{
	HANDLE_CASE(tcsetattr(2, TCSAFLUSH, &_orig_termios) == -1);
}

static void _terminal_setup(void)
{
	HANDLE_CASE(tcgetattr(2, &_orig_termios) == -1);
	struct termios raw = _orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	HANDLE_CASE(tcsetattr(2, TCSAFLUSH, &raw) == -1);
}

static void _terminal_io(void)
{
	static struct pollfd pfd = { 0, POLLIN, 0 };
	int cnt = poll(&pfd, 1, 0);
	HANDLE_CASE(cnt == -1);
	if (cnt == 0)
		return;
	char ch;
	HANDLE_CASE(read(0, &ch, 1) != 1);
	if (ch == 27) {
		if (state.menu_active)
			_should_quit = true;
		else
			menu_show();
	} else if (ch == 's') {
		puts("Saving a screenshot to a.ppm");
		graphics_save_sshot("a.ppm");
	}
}

static void _cd_to_exe(void)
{
	char buf[4096];
	int len = readlink("/proc/self/exe", buf, 4095);
	HANDLE_CASE(len == -1 || len > 4000);
	buf[len] = 0;
	HANDLE_CASE(chdir(dirname(buf)) == -1);
}

static void _check_for_x11(void)
{
	if (getenv("DISPLAY") != NULL) {
		puts("X11 server detected.");
		puts("Please don't run this from X11. Run it from a TTY.");
		puts("Press CTRL-ALT-F2, log in, and run this from there.");
		exit(1);
	}
}

int main(void)
{
	_check_for_x11();
	_cd_to_exe();
	srand(time(NULL));

	input_init();
	audio_init();
	video_init();
	graphics_init();
	tex_init();
	font_init();
	state_init();
	render_init();
	game_init();
	blood_init();
	muzzle_flash_init();
	gunman_init();
	animal_init();
	menu_init();
	_terminal_setup();
	puts("Initialization done");

	if (video_width < 1000)
		set_status(0, "WARNING: resolution too low!");

	int last_frame_id = 1;
	time_t last_time = time(NULL);
	while (!_should_quit) {
		state.frame_id += 1;
		time_t t = time(NULL);
		if (t != last_time) {
			state.fps = state.frame_id - last_frame_id;
			last_frame_id = state.frame_id;
			last_time = t;
			input_check_mouses();
		}
		state.vertices_drawn = 0;
		if (state.frame_id > 200 && (state.fps<55 || state.fps>65)) {
			set_status(1, "FPS is not 60!");
			set_status(2, "The game won't be enjoyable. Sorry.");
		}

		util_tick();
		input_tick();
		_terminal_io();
		game_tick();
		render();
		state.vertices_last_frame = state.vertices_drawn;
	}

	puts("Quitting");
	_terminal_reset();
	menu_destroy();
	animal_destroy();
	gunman_destroy();
	muzzle_flash_destroy();
	blood_destroy();
	game_destroy();
	render_destroy();
	state_destroy();
	font_destroy();
	tex_destroy();
	graphics_destroy();
	video_destroy();
	audio_destroy();
	input_destroy();
	return 0;
}

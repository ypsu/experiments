#include "headers.h"

enum _BUTTONS {
	_BUT_LEFT = 1,
	_BUT_RIGHT = 2,
	_BUT_MID = 4,
};

struct _input {
	enum _BUTTONS buttons;
	int dx, dy;
};

static int _fds[PLAYERS_MAX];
static int _poll_count;
static struct pollfd _pollfds[PLAYERS_MAX];
static int _pollid_to_playerid[PLAYERS_MAX];
static int _sensitivity[PLAYERS_MAX];

static void _handle_input(int player)
{
	vec_t *mp = &state.mouses[player];

	unsigned char buf[400];
	int sz = read(_fds[player], buf, 400);
	HANDLE_CASE(sz <= 0 || sz%4 != 0);
	for (int i = 0; i < sz; i += 4) {
		int ctrl = buf[i];
		int dx = buf[i+1];
		int dy = buf[i+2];
		int dz = buf[i+3];
		if (ctrl & 16)
			dx = ~255 | dx;
		if (ctrl & 32)
			dy = ~255 | dy;
		if (dz >= 8)
			dz = ~255 | dz;

		if (dz != 0) {
			_sensitivity[player] += dz;
			if (_sensitivity[player] < 1)
				_sensitivity[player] = 1;
			else if (_sensitivity[player] > 50)
				_sensitivity[player] = 50;
			const char *fmt;
			float sens = _sensitivity[player] / 10.0f;
			fmt = "Changed player %d's sensitivity is %3.1f.";
			set_status(0, qprintf(fmt, player+1, sens));
		}

		vec_t dv;
		float sens = _sensitivity[player] / 10.0f;
		dv.x = dx * screen_width/video_width * sens;
		dv.y = dy * screen_height/video_height * sens;

		v_inc(mp, &dv);
		mp->x = clampf(mp->x, 0.0f, screen_width);
		mp->y = clampf(mp->y, 0.0f, screen_height);

		bool down = ctrl & 1;
		bool just_pressed = !state.buttons_down[player] && down;
		state.buttons_pressed[player] = just_pressed;
		state.buttons_down[player] = down;
	}
}

static int _get_device_id(int fd)
{
	unsigned char buf[2];
	buf[0] = 0xF2;
	HANDLE_CASE(write(fd, buf, 1) != 1);
	HANDLE_CASE(read(fd, buf, 2) != 2);
	HANDLE_CASE(buf[0] != 0xFA);
	return buf[1];
}

static void _find_new_devices(void)
{
	// Open new devices.
	for (int i = 0; i < PLAYERS_MAX; ++i) {
		if (_fds[i] != -1) {
			continue;
		}
		char fname[MAXSTR];
		sprintf(fname, "/dev/input/mouse%d", i);
		_fds[i] = open(fname, O_RDWR);
		if (_fds[i] == -1) {
			continue;
		}
		printf("Found a mouse at %s\n", fname);
		_sensitivity[i] = 10;
		state.inputs_active[i] = true;
		unsigned char buf[6] = { 0xF3, 200, 0xF3, 100, 0xF3, 80 };
		HANDLE_CASE(write(_fds[i], buf, 6) != 6);
		HANDLE_CASE(read(_fds[i], buf, 6) != 1);
		HANDLE_CASE(buf[0] != 0xFA);
		HANDLE_CASE(_get_device_id(_fds[i]) != 0x03);
	}

	// Setup the polling structures.
	_poll_count = 0;
	for (int i = 0; i < PLAYERS_MAX; ++i) {
		if (_fds[i] == -1) {
			continue;
		}
		_pollfds[_poll_count].fd = _fds[i];
		_pollfds[_poll_count].events = POLLIN;
		_pollid_to_playerid[_poll_count] = i;
		_poll_count += 1;
	}

	if (_poll_count == 0) {
		puts("\tError: no mouse found!");
		puts("\tMake sure /dev/input/mouseX exists and is accessible!");
		exit(1);
	}
}

void input_init(void)
{
	memset(_fds, -1, sizeof _fds);
	_find_new_devices();
}

void input_check_mouses(void)
{
	_find_new_devices();
}

void input_destroy(void)
{
	for (int i = 0; i < PLAYERS_MAX; ++i) {
		if (_fds[i] != -1)
			HANDLE_CASE(close(_fds[i]) == -1);
	}
}

void input_tick(void)
{
	for (int i = 0; i < PLAYERS_MAX; ++i)
		state.buttons_pressed[i] = false;

	int cnt = poll(_pollfds, _poll_count, 0);
	HANDLE_CASE(cnt == -1);
	if (cnt == 0)
		return;
	for (int i = 0; i < _poll_count; ++i) {
		int playerid = _pollid_to_playerid[i];
		if ((_pollfds[i].revents & POLLERR) != 0) {
			printf("Lost player %d's mouse.\n", playerid);
			state.inputs_active[playerid] = false;
			HANDLE_CASE(close(_fds[playerid]) == -1);
			_fds[playerid] = -1;
			_find_new_devices();
			menu_show();
			return;
		}
		if ((_pollfds[i].revents & POLLIN) != 0) {
			_handle_input(playerid);
		}
	}
}

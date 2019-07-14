#include "headers.h"

enum { _MAX_KEY = SDL_NUM_SCANCODES };
enum { _MAX_MOUSE = 10 };

static struct {
	uint8_t keys_state[_MAX_KEY];
	int keys_last_pressed[_MAX_KEY];

	uint8_t mouse_state[_MAX_MOUSE];
	int mouse_last_pressed[_MAX_MOUSE];
}
_v;

float
input_mx,
input_my,
input_mdx,
input_mdy,
input_mdw;

void
input_handle_event(SDL_Event *ev)
{
	int t = ev->type;
	if (t == SDL_KEYUP || t == SDL_KEYDOWN) {
		SDL_KeyboardEvent *key = &ev->key;
		int code = key->keysym.scancode;
		if (code < _MAX_KEY) {
			_v.keys_state[code] = key->state;
			if (t == SDL_KEYDOWN) {
				_v.keys_last_pressed[code] = g.tick;
			}
		}
	} else if (t == SDL_MOUSEBUTTONUP || t == SDL_MOUSEBUTTONDOWN) {
		int button = ev->button.button;
		CHECK(button < _MAX_MOUSE);
		_v.mouse_state[button] = ev->button.state;
		if (t == SDL_MOUSEBUTTONDOWN) {
			_v.mouse_last_pressed[button] = g.tick;
		}
	} else if (t == SDL_MOUSEWHEEL) {
		input_mdw += ev->wheel.y;
	} else {
		abort();
	}
}

void
input_tick(void)
{
	int x, y, dx, dy;
	SDL_GetMouseState(&x, &y);
	SDL_GetRelativeMouseState(&dx, &dy);

	input_mx = x;
	input_my = y;
	input_mdx = dx;
	input_mdy = dy;
	input_mdw = 0;
}

bool
input_is_pressed(SDL_Scancode key)
{
	int code = key;
	return code < _MAX_KEY && _v.keys_state[code] == SDL_PRESSED;
}

bool
input_is_clicked(int button)
{
	return button < _MAX_MOUSE && _v.mouse_state[button] == SDL_PRESSED;
}

bool
input_was_pressed(SDL_Scancode key)
{
	if (!input_is_pressed(key))
		return false;
	int code = key;
	return _v.keys_last_pressed[code] == g.tick;
}

bool
input_was_clicked(int button)
{
	if (!input_is_clicked(button))
		return false;
	return _v.mouse_last_pressed[button] == g.tick;
}

void
input_set_mouse(enum input_mouse_type type)
{
	if (type == INPUT_MOUSE_ABSOLUTE) {
		SDL_ShowCursor(1);
		SDL_SetRelativeMouseMode(SDL_FALSE);
	} else if (type == INPUT_MOUSE_RELATIVE) {
		SDL_ShowCursor(0);
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
}

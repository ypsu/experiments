#include "headers.h"

struct state state;
struct color player_colors[PLAYERS_MAX];

void state_init(void)
{
	puts("Initializing the state subsystem");
	player_colors[0] = color_red;
	player_colors[1] = color_green;
	player_colors[2] = color_blue;
	player_colors[3] = color_yellow;
	player_colors[4] = color_cyan;
	player_colors[5] = color_pink;
	state.menu_active = true;
}

void state_destroy(void)
{
}

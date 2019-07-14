#ifndef game_h
#define game_h

enum gamestate {
	GAMESTATE_GETREADY,
	GAMESTATE_COUNTDOWN,
	GAMESTATE_GUNFIGHT,
	GAMESTATE_GAMEOVER,
};

void game_init(void);
void game_destroy(void);
void game_tick(void);

#endif

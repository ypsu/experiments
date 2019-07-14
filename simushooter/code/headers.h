#ifndef HEADERS_H
#define HEADERS_H

#define _GNU_SOURCE
#include <AL/al.h>
#include <AL/alc.h>
#include <SDL.h>
#include <SDL_opengles2.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


// The engine uses the following coordinate system:
//  z     y
//  ^    7
//  |   /
//  |  /
//  | /
//  |/
// -+--------> x
// /|
//
// So the floor of the map is on the x and y axis and the [0,0,0] is the bottom
// left of the map (when viewed from above in the tile editor). This doesn't
// apply to ortho mode where x represents width, y represents height and [0,0]
// is the top left corner.
//
// All angles mean counterclockwise rotation. Angle 0 is the same as the x axis.


// util.c: Utilities, helpers, miscellaneous stuff.

	// Maximum string length including the zero terminator (63 real length
	// then).
	enum { MAXSTR = 64 };

	// Safe malloc - always succeeds.
	void *smalloc(int len);

	// Scratch allocation - memory valid until next tick.
	void *qalloc(int len);

	#define CHECK(cond) util_check((cond), #cond, __FILE__, __LINE__);
	void util_check(bool ok, const char *cond, const char *file, int line);

	// Returns a formatted string which will be valid until the next tick.
	const char *qprintf(const char *format, ...);

	// length is an output parameter, must be non-NULL.
	void *util_load_file(const char *fname, int *length);

	// Contrary to atexit, these functions are also called on CHECK fail.
	void onexit(void (*f)());

	// Clean up the scratch pad.
	void util_tick(void);


// math_helpers.c: Some random math helper utilities.

	#ifndef M_PI
	#define M_PI 3.141592653589793238462643383
	#endif

	typedef struct {
		float v[4];
	} vector_t;

	typedef struct {
		float m[4][4];
	} matrix_t;

	float maxf(float a, float b);
	float minf(float a, float b);
	float sqrf(float x);
	float clampf(float value, float lo, float hi);
	int clampi(int value, int lo, int hi);
	// Return a random value from [0.0f, 1.0f].
	float randf(void);


// vector.c: Standard 4-dimensional vector operations.

	extern const vector_t v_zero; // zero vector
	extern const vector_t v_origin; // point at the origin
	extern const vector_t v_up; // {{ 0, 0, 1, 0 }}

	#define V_EXPAND3(vec) (vec).v[0], (vec).v[1], (vec).v[2]
	#define V_EXPAND4(vec) (vec).v[0], (vec).v[1], (vec).v[2], (vec).v[3]

	void v_set(vector_t *v, float x, float y, float z, float w);
	void v_inc(vector_t *v, const vector_t *w);
	void v_dec(vector_t *v, const vector_t *w);
	void v_add(vector_t *v, const vector_t *a, const vector_t *b);
	void v_sub(vector_t *v, const vector_t *a, const vector_t *b);
	void v_scale(vector_t *v, float f);
	float v_dot(const vector_t *a, const vector_t *b);
	float v_magn(const vector_t *v);
	void v_normalize(vector_t *v);
	const char *v_tostr(const vector_t *v);

	// out = m*v
	void v_transform(vector_t *out, const matrix_t *m, const vector_t *v);


// matrix.c: Standard 4x4 matrix operations.

	extern const matrix_t m_zero;
	extern const matrix_t m_id;

	// out = a*b
	void m_mult(matrix_t *out, const matrix_t *a, const matrix_t *b);

	void m_transpose(matrix_t *m);

	void m_scale(matrix_t *m, float f);

	void m_set_xrot(matrix_t *m, float angle);
	void m_set_yrot(matrix_t *m, float angle);
	void m_set_zrot(matrix_t *m, float angle);
	void m_set_translate(matrix_t *m, const vector_t *v);


// audio.c: Audio engine.

	enum snd_id {
		SND_BREATHING,
		SND_GUN_BERETTA_EMPTY,
		SND_GUN_BERETTA_FIRE,
		SND_GUN_BERETTA_RELOAD,
		SND_GUN_BERETTA_RELOAD_EMPTY,
		SND_GUN_CLOSE,
		SND_GUN_LOAD,
		SND_GUNSHOT_DRY,
		SND_GUNSHOT,
		SND_LAST,
	};

	void audio_init(void);
	void audio_stop(void);
	void audio_tick(void);
	void audio_play(enum snd_id id);
	void audio_play_at(enum snd_id id, const vector_t *v);
	// Length in number of frames.
	int audio_length(enum snd_id id);

// ge.c: Graphics Engine. Contains the low level stuff like the OpenGLES calls,
// shader manipulation, primitive rendering.

	// The scene is rendered in 3 draw calls in this order:
	//   1. static geometry - This is changed rarely but drawn every time.
	//                        Good for map geometry for example.
	//   2. dynamic geometry - This is used for rendering the dynamic
	//                         elements in the 3D world.
	//   3. ortho geometry - This is overlayed on top of the above, used for
	//                       HUD and text.
	// You can only draw quads. The vertices for the quads must be given in
	// counterclockwise order.

	// The game window's size.
	extern int window_width, window_height;

	struct vertex {
		vector_t pos;
		vector_t color;
		float s, t;
	};

	void ge_init(void);
	void ge_reinit_window(void);
	void ge_stop(void);
	void ge_check_errors(void);
	void ge_load_static_geometry(void);

	// Get a buffer of 4*n vertices to draw rectangles in the current frame.
	// The vertices need to be in counter clockwise direction. The buffer is
	// only valid until you call ge_submit().
	struct vertex *ge_get_dynamic_vertices(int n);
	struct vertex *ge_get_ortho_vertices(int n);

	// View is the camera's transformation. It applies only to the static
	// and dynamic geometry.
	void ge_submit(void);


// texture.c: Texture manipulation helpers. There's a single texture,
// everything must fit into that. This simplifies everything.

	// The width and height of the texture image.
	enum { TEX_SIZE = 1024 };

	enum {
		TEX_WHITE,
		TEX_AMMO_VERTICAL,
		TEX_ROBOT,
		TEX_LAST,
	};
	extern vector_t textures[TEX_LAST];

	void tex_init(void);
	void tex_stop(void);

	// Translate a texture's coordinates into the global texture's
	// coordinates.
	void tex_get(const char *name, float *u, float *v);


// font.c: Text rendering utilities.

	extern const int font_x, font_y;

	void font_init(void);
	void font_stop(void);

	void font_draw(int x, int y, const vector_t *color, const char *str);


// colors.c: Some predefined colors for convenience.

	extern const vector_t color_black;
	extern const vector_t color_white;
	extern const vector_t color_red;
	extern const vector_t color_green;
	extern const vector_t color_blue;
	extern const vector_t color_yellow;
	extern const vector_t color_cyan;
	extern const vector_t color_pink;


// input.c: Input handling.

	// Mouse's absolute and delta coordinates.
	extern float input_mx, input_my, input_mdx, input_mdy, input_mdw;

	void input_handle_event(SDL_Event *event);
	void input_tick(void);

	// Returns true iff the key is being held.
	bool input_is_pressed(SDL_Scancode key);
	bool input_is_clicked(int button);

	// Returns true iff the key was pressed in this tick.
	bool input_was_pressed(SDL_Scancode key);
	bool input_was_clicked(int button);

	enum input_mouse_type { INPUT_MOUSE_RELATIVE, INPUT_MOUSE_ABSOLUTE };
	void input_set_mouse(enum input_mouse_type type);


// robot.c: Robot handling.

	enum { MAX_ROBOTS = 64 };
	extern const float robot_height, robot_radius;

	struct robot {
		bool active;
		bool legsout; // True if the legs have been shot.
		vector_t pos, new_pos;
		float zrot, xrot, new_zrot, new_xrot;
		vector_t momentum; // Becomes non-zero after a body shot.
	};

	void robot_init(void);
	void robot_stop(void);
	int robot_alloc(void);
	bool robot_touches_tile(int robotid, int column, int row);


// simulation.c: Simulate the world.

	enum { MAP_TILES = 256 };
	// Contains assets/world.map.
	extern char map[MAP_TILES][MAP_TILES+1];
	extern float map_height;
	extern const float cover_height;

	enum { MAX_BULLET_TRAILS = 64 };
	struct bullet_trail {
		vector_t start, end;
		int start_tick;
		int end_tick;
		int shooter_robot;
	};

	void sim_init(void);
	void sim_stop(void);
	void simulate(void);


// render.c: Renders the scene based on the data from the global state g.

	void render(void);


// weapons.c: Contains all the logic for the various weapons and items.

	enum { WEAPONS_CARRIED = 2 };

	enum weapon_id {
		W_NONE,
		W_PISTOL,
		W_REVOLVER,
		W_UZI,
		W_SAWED_SHOTGUN,
		W_SNIPER_RIFLE,
		W_HUNTING_RIFLE,
		W_ASSAULT_RIFLE,
		W_PUMP_SHOTGUN,
		W_ASSAULT_SHOTGUN,
		W_COUNT,
	};

	enum gun_state {
		GUN_READY,
		GUN_IN_RECOIL,
		GUN_RAISING,

		GUN_LOWERING_FOR_RELOAD,
		GUN_RELOADING,

		GUN_LOWERING_FOR_AMMO,
		GUN_UNLOADING_AMMO,
		GUN_RAISING_AMMO,
		GUN_SHOWING_AMMO,
		GUN_LOWERING_AMMO,
		GUN_LOADING_AMMO,

		GUN_LOWERING_FOR_SWITCH,
		GUN_SWITCHING,

		GUN_STATE_COUNT,
	};

	struct weapon_state {
		enum weapon_id id;
		int ammo;
		int spare;
	};

	enum weapon_barrel {
		WEAPON_BARREL_SHORT,
		WEAPON_BARREL_LONG,
	};

	enum weapon_ammo {
		WEAPON_AMMO_BULLETS,
		WEAPON_AMMO_SHELLS,
	};

	enum weapon_container {
		WEAPON_CONTAINER_CLIPS,
		WEAPON_CONTAINER_BARREL,
	};

	enum weapon_operation {
		WEAPON_OPERATION_AUTO,
		WEAPON_OPERATION_SEMI,
		WEAPON_OPERATION_MANUAL,
	};

	struct weapon_desc {
		enum weapon_id id;
		char name[MAXSTR];
		enum weapon_barrel barrel;
		enum weapon_ammo ammo;
		enum weapon_container container;
		enum weapon_operation operation;
		int ammo_capacity;
		float kick;
		float accuracy_lo, accuracy_hi;
		float fire_delay;
	};

	struct weapon_desc weapons[W_COUNT];

	// Set up the necessary structures for playing back an animation.
	void weapons_animate(enum gun_state state, int length);
	int weapons_get_texture(void);
	// 0 means totally hidden, 1 means totally visible.
	float weapons_get_visibility(void);
	// Do the accounting, play audio, and raise the gun. Call after the gun
	// is lowered already.
	void weapons_reload(void);


// state.c: Global state. All dynamic state and other global variables are
// aggregated here. If a variable is constant throughout the game, it has no
// place here and is in the global scope.

	enum { MAX_ROBOTS_IN_TILE = 8 };

	struct state {
		// The tick id of the current frame.
		int tick;

		// The main loop should quit.
		bool should_quit;

		// The FPS count for previous second. Should be quite static.
		int fps;

		// Delta time for a frame / speed of the game. Equals to 1/fps.
		float dt;

		// Field of view, usually 90 degrees.
		float fov;

		struct {
			bool free_flight;
			vector_t pos;
			float zrot, xrot;
		} camera;

		// Robot 0 is the player.
		struct robot robots[MAX_ROBOTS];
		int bullet_trails_next;
		struct bullet_trail bullet_trails[MAX_BULLET_TRAILS];

		struct {
			// 90 degrees means full screen, 0 means perfect
			// accuracy.
			float crosshair_angle;
			bool moving;

			enum gun_state gun_state;
			int gun_busy_until_tick;
			int gun_animation_length;
			int ammo;
			struct weapon_state weapons[WEAPONS_CARRIED];
			int weapon_current;
		} player;

		// For each tile in the map tile_robots[r][c] contains all the
		// robots for which the boundary circle touches tile(r,c). Tile
		// robots thus contains robot IDs which indices into the
		// robots array. The end of the list of indices is marked by a
		// -1. This spatial cache is used for speeding up the collision
		// detection significantly by doing the testing only in the
		// surrounding tiles. This assumes all robots must fit into a
		// tile although a robot can be registered into multiple tiles.
		// Basically MAX_ROBOTS_IN_TILE*radius_of_a_robot^2 must be
		// smaller than area_of_a_tile which is 1. Also, during the
		// simulation no robot can jump across more than one tile. This
		// latter fact is used for collision detection against the map's
		// geometry.
		short tile_robots[MAP_TILES][MAP_TILES][MAX_ROBOTS_IN_TILE+1];
	};

	extern struct state g;

#endif

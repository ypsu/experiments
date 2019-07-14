#include "headers.h"

struct weapon_desc weapons[W_COUNT] = {
	{
		.id = W_NONE,
		.name = "None",
	}, {
		.id = W_PISTOL,
		.name = "Pistol",
		.barrel = WEAPON_BARREL_SHORT,
		.ammo = WEAPON_AMMO_BULLETS,
		.container = WEAPON_CONTAINER_CLIPS,
		.operation = WEAPON_OPERATION_SEMI,
		.ammo_capacity = 7,
		.kick = 0.1 * M_PI/180.0f,
		.accuracy_lo = 1.0 * M_PI/180.0f,
		.accuracy_hi = 3.0 * M_PI/180.0f,
		.fire_delay = 0.1,
	}, {
		.id = W_REVOLVER,
		.name = "Revolver",
		.barrel = WEAPON_BARREL_SHORT,
		.ammo = WEAPON_AMMO_BULLETS,
		.container = WEAPON_CONTAINER_BARREL,
		.operation = WEAPON_OPERATION_MANUAL,
		.ammo_capacity = 6,
		.kick = 0.2 * M_PI/180.0f,
		.accuracy_lo = 2.0 * M_PI/180.0f,
		.accuracy_hi = 5.0 * M_PI/180.0f,
		.fire_delay = 0.5,
	}, {
		.id = W_UZI,
		.name = "Uzi",
		.barrel = WEAPON_BARREL_SHORT,
		.ammo = WEAPON_AMMO_BULLETS,
		.container = WEAPON_CONTAINER_BARREL,
		.operation = WEAPON_OPERATION_AUTO,
		.ammo_capacity = 25,
		.kick = 0.05 * M_PI/180.0f,
		.accuracy_lo = 1.0 * M_PI/180.0f,
		.accuracy_hi = 3.0 * M_PI/180.0f,
		.fire_delay = 0.02,
	}, {
		.id = W_SAWED_SHOTGUN,
		.name = "Sawed off shotgun",
		.barrel = WEAPON_BARREL_SHORT,
		.ammo = WEAPON_AMMO_SHELLS,
		.container = WEAPON_CONTAINER_BARREL,
		.operation = WEAPON_OPERATION_SEMI,
		.ammo_capacity = 2,
		.kick = 0.2 * M_PI/180.0f,
		.accuracy_lo = 3.0 * M_PI/180.0f,
		.accuracy_hi = 5.0 * M_PI/180.0f,
		.fire_delay = 0.1,
	}, {
		.id = W_SNIPER_RIFLE,
		.name = "Sniper rifle",
		.barrel = WEAPON_BARREL_LONG,
		.ammo = WEAPON_AMMO_BULLETS,
		.container = WEAPON_CONTAINER_BARREL,
		.operation = WEAPON_OPERATION_MANUAL,
		.ammo_capacity = 5,
		.kick = 0.5 * M_PI/180.0f,
		.accuracy_lo = 0.1 * M_PI/180.0f,
		.accuracy_hi = 3.0 * M_PI/180.0f,
		.fire_delay = 1.0,
	}, {
		.id = W_HUNTING_RIFLE,
		.name = "Hunting rifle",
		.barrel = WEAPON_BARREL_LONG,
		.ammo = WEAPON_AMMO_BULLETS,
		.container = WEAPON_CONTAINER_CLIPS,
		.operation = WEAPON_OPERATION_SEMI,
		.ammo_capacity = 10,
		.kick = 0.2 * M_PI/180.0f,
		.accuracy_lo = 0.3 * M_PI/180.0f,
		.accuracy_hi = 3.0 * M_PI/180.0f,
		.fire_delay = 0.1,
	}, {
		.id = W_ASSAULT_RIFLE,
		.name = "Assault rifle",
		.barrel = WEAPON_BARREL_LONG,
		.ammo = WEAPON_AMMO_BULLETS,
		.container = WEAPON_CONTAINER_CLIPS,
		.operation = WEAPON_OPERATION_AUTO,
		.ammo_capacity = 30,
		.kick = 0.2 * M_PI/180.0f,
		.accuracy_lo = 0.5 * M_PI/180.0f,
		.accuracy_hi = 3.0 * M_PI/180.0f,
		.fire_delay = 0.05,
	}, {
		.id = W_PUMP_SHOTGUN,
		.name = "Pump action shotgun",
		.barrel = WEAPON_BARREL_LONG,
		.ammo = WEAPON_AMMO_SHELLS,
		.container = WEAPON_CONTAINER_BARREL,
		.operation = WEAPON_OPERATION_MANUAL,
		.ammo_capacity = 8,
		.kick = 0.5 * M_PI/180.0f,
		.accuracy_lo = 2.0 * M_PI/180.0f,
		.accuracy_hi = 4.0 * M_PI/180.0f,
		.fire_delay = 0.5,
	}, {
		.id = W_ASSAULT_SHOTGUN,
		.name = "Assault shotgun",
		.barrel = WEAPON_BARREL_LONG,
		.ammo = WEAPON_AMMO_SHELLS,
		.container = WEAPON_CONTAINER_BARREL,
		.operation = WEAPON_OPERATION_SEMI,
		.ammo_capacity = 10,
		.kick = 0.5 * M_PI/180.0f,
		.accuracy_lo = 2.0 * M_PI/180.0f,
		.accuracy_hi = 4.0 * M_PI/180.0f,
		.fire_delay = 0.1,
	},
};

void
weapons_animate(enum gun_state state, int length)
{
	g.player.gun_state = state;
	g.player.gun_busy_until_tick = g.tick + length;
	g.player.gun_animation_length = length;
}

float
weapons_get_visibility(void)
{
	enum anim_type { A_SHOWN, A_HIDDEN, A_LOWERING, A_RAISING };
	static const enum anim_type anim_type[GUN_STATE_COUNT] = {
		[GUN_READY] = A_SHOWN,
		[GUN_IN_RECOIL] = A_SHOWN,
		[GUN_RAISING] = A_RAISING,
		[GUN_LOWERING_FOR_RELOAD] = A_LOWERING,
		[GUN_RELOADING] = A_HIDDEN,
		[GUN_LOWERING_FOR_AMMO] = A_LOWERING,
		[GUN_UNLOADING_AMMO] = A_HIDDEN,
		[GUN_RAISING_AMMO] = A_RAISING,
		[GUN_SHOWING_AMMO] = A_SHOWN,
		[GUN_LOWERING_AMMO] = A_LOWERING,
		[GUN_LOADING_AMMO] = A_HIDDEN,
		[GUN_LOWERING_FOR_SWITCH] = A_LOWERING,
		[GUN_SWITCHING] = A_HIDDEN,
	};
	float anim_percent = 0.0;
	float anim_len = g.player.gun_animation_length;
	if (anim_len > 0.0f) {
		anim_percent = (g.player.gun_busy_until_tick-g.tick) / anim_len;
	}
	switch (anim_type[g.player.gun_state]) {
	case A_SHOWN:
		return 1.0f;
	case A_HIDDEN:
		return 0.0f;
	case A_RAISING:
		return 1.0f - anim_percent;
	case A_LOWERING:
		return anim_percent;
	}
	abort();
}

void
weapons_reload(void)
{
	struct weapon_state *w = &g.player.weapons[g.player.weapon_current];
	CHECK(w->spare > 0);
	w->spare -= 1;
	w->ammo = weapons[w->id].ammo_capacity;
	enum snd_id id = SND_GUN_BERETTA_RELOAD_EMPTY;
	audio_play(id);
	weapons_animate(GUN_RELOADING, audio_length(id));
}

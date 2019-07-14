#include "headers.h"

void muzzle_flash_init(void)
{
	memset(state.flashes, -63, sizeof state.flashes);
}

void muzzle_flash_destroy(void)
{
}

void muzzle_flash_add(float angle, const vec_t *pos)
{
	// Find an available slot
	int slot;
	for (slot = 0; slot < MUZZLE_FLASHES; ++slot) {
		if (state.flashes[slot].frame_activated < state.frame_id-20)
			break;
	}
	HANDLE_CASE(slot == MUZZLE_FLASHES);

	// Put the data into the available slot
	state.flashes[slot].frame_activated = state.frame_id;
	state.flashes[slot].angle = angle;
	state.flashes[slot].pos = *pos;
}

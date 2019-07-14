#include "headers.h"

void blood_init(void)
{
	memset(state.blood, -63, sizeof state.blood);
}

void blood_destroy(void)
{
}

void blood_add(const vec_t *pos)
{
	// Find an available slot
	int slot;
	for (slot = 0; slot < BLOOD_SPLASHES; ++slot) {
		if (state.blood[slot].frame_activated < state.frame_id-13*3)
			break;
	}
	HANDLE_CASE(slot == BLOOD_SPLASHES);

	// Put the data into the available slot
	state.blood[slot].frame_activated = state.frame_id;
	state.blood[slot].pos = *pos;
}

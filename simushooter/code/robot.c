#include "headers.h"

const float
robot_height = 2.0f,
robot_radius = 0.49f;

void
robot_init(void)
{
	memset(g.robots, -1, sizeof g.robots);
}

void
robot_stop(void)
{
}

int
robot_alloc(void)
{
	for (int i = 0; i < MAX_ROBOTS; i++) {
		if (!g.robots[i].active) {
			return i;
		}
	}
	abort();
}

bool
robot_touches_tile(int robotid, int column, int row)
{
	// To simplify the logic, we consider the robots rectangular in this
	// context. The robot square side's length is 2*model_radius.
	//
	// Again, as noted at g.tile_robots, we assume that a robot's square
	// is always smaller than a tile's square.

	const struct robot *r = &g.robots[robotid];
	CHECK(r->active);

	float my0 = r->new_pos.v[1] - robot_radius;
	float my1 = r->new_pos.v[1] + robot_radius;
	float mx0 = r->new_pos.v[0] - robot_radius;
	float mx1 = r->new_pos.v[0] + robot_radius;
	float ty0 = row, ty1 = row + 1.0f;
	float tx0 = column, tx1 = column + 1.0f;

	bool y_inside = (ty0 < my0 && my0 < ty1) || (ty0 < my1 && my1 < ty1);
	bool x_inside = (tx0 < mx0 && mx0 < tx1) || (tx0 < mx1 && mx1 < tx1);
	return y_inside && x_inside;
}

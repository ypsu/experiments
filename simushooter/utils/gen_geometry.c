#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Input: the grid of map.
// Output: a list of quads with world and texture coordinates.

// Each tile has four sides: North, South, East, West. We will make a pass
// through the grid to ensure the quads are generated for each side. If the same
// side of at least two neighboring tiles have the same type like two buildings
// next to each other, we won't generate two separate quads but rather one which
// will cover the whole side of the consecutive tiles of the same type. Also, we
// try to avoid quads which wouldn't be visible to the player like the edge
// between two walls.

// Legend:
// . Walkable area.
// w Walled area. Nothing can pass it.
// c Cover. This is a chest high, non-passable wall.

enum { MAXDIM = 700 };

#define CHECK(cond) _util_check((cond), #cond, __FILE__, __LINE__);
static void
_util_check(bool ok, const char *cond, const char *file, int line)
{
	if (ok)
		return;
	const char *msg = "CHECK(%s) failed at %s:%d.\n";
	printf(msg, cond, file, line);
	abort();
}

const char col_east[] = "0.30 0.30 0.30";
const char col_north[] = "0.40 0.40 0.40";
const char col_west[] = "0.50 0.50 0.50";
const char col_south[] = "0.60 0.60 0.60";

struct {
	// The map dimensions are autodetected.
	int R, C;

	// The rows are in opposite order so that the lower left corner of the
	// map is at the origin.
	char field[MAXDIM][MAXDIM+1];
}
_v;

static void
_swapi(int *a, int *b)
{
	int t = *a;
	*a = *b;
	*b = t;
}

static bool
_is_obstacle(int ch)
{
	return ch == 'w' || ch == 'c';
}

static void
_vertical_pass(int column, bool west)
{
	int x = column;
	if (!west) x += 1;
	int last_type = 0;
	int last_height = 4;
	for (int row = 0; row <= _v.R; row++) {
		int cur_type;
		bool ok = false;
		if (row == _v.R) {
			cur_type = 0;
			ok = true;
		} else {
			int r = _v.R - row - 1;
			cur_type = _v.field[r][column];
			if (west && column > 0) {
				if (!_is_obstacle(_v.field[r][column-1]))
					ok = true;
			} else if (!west && column+1 < _v.C) {
				if (!_is_obstacle(_v.field[r][column+1]))
					ok = true;
			}
		}

		if (cur_type == last_type)
			continue;

		// Finish last type.
		if (ok && _is_obstacle(last_type)) {
			int z0 = 0, z1 = last_height;
			if (west) _swapi(&z0, &z1);
			printf("%d.0 %d.0 %d.0 ", x, row, z0);
			printf("%s ", west ? col_west : col_east);
			printf("white.png ");
			printf("%0.6f %0.6f\n", row*1.0/_v.R, 1.0-z0/4.0);
			printf("%d.0 %d.0 %d.0 ", x, row, z1);
			printf("%s ", west ? col_west : col_east);
			printf("white.png ");
			printf("%0.6f %0.6f\n", row*1.0/_v.R, 1.0-z1/4.0);
			last_type = 0;
		}

		// Start the new type.
		if (ok && _is_obstacle(cur_type)) {
			int z0 = 0, z1 = 4;
			if (cur_type == 'c') {
				z1 = 1;
			}
			last_height = z1;
			if (!west) _swapi(&z0, &z1);
			printf("%d.0 %d.0 %d.0 ", x, row, z0);
			printf("%s ", west ? col_west : col_east);
			printf("white.png ");
			printf("%0.6f %0.6f\n", row*1.0/_v.R, 1.0-z0/4.0);
			printf("%d.0 %d.0 %d.0 ", x, row, z1);
			printf("%s ", west ? col_west : col_east);
			printf("white.png ");
			printf("%0.6f %0.6f\n", row*1.0/_v.R, 1.0-z1/4.0);
			last_type = cur_type;
		}
	}
}

static void
_horizontal_pass(int row, bool north)
{
	int y = row;
	if (north) y += 1;
	int last_type = 0;
	int last_height = 4;
	for (int column = 0; column <= _v.C; column++) {
		int cur_type;
		bool ok = false;
		if (column == _v.C) {
			cur_type = 0;
			ok = true;
		} else {
			int r = _v.R - row - 1;
			cur_type = _v.field[r][column];
			if (north && r > 0) {
				if (!_is_obstacle(_v.field[r-1][column]))
					ok = true;
			} else if (!north && r+1 < _v.R) {
				if (!_is_obstacle(_v.field[r+1][column]))
					ok = true;
			}
		}

		if (cur_type == last_type)
			continue;

		// Finish last type.
		if (ok && _is_obstacle(last_type)) {
			int z0 = 0, z1 = last_height;
			if (north) _swapi(&z0, &z1);
			printf("%d.0 %d.0 %d.0 ", column, y, z0);
			printf("%s ", north ? col_north : col_south);
			printf("white.png ");
			printf("%0.6f %0.6f\n", column*1.0/_v.C, 1.0-z0/4.0);
			printf("%d.0 %d.0 %d.0 ", column, y, z1);
			printf("%s ", north ? col_north : col_south);
			printf("white.png ");
			printf("%0.6f %0.6f\n", column*1.0/_v.C, 1.0-z1/4.0);
			last_type = 0;
		}

		// Start the new type.
		if (ok && _is_obstacle(cur_type)) {
			int z0 = 0, z1 = 4;
			if (cur_type == 'c') {
				z1 = 1;
			}
			last_height = z1;
			if (!north) _swapi(&z0, &z1);
			printf("%d.0 %d.0 %d.0 ", column, y, z0);
			printf("%s ", north ? col_north : col_south);
			printf("white.png ");
			printf("%0.6f %0.6f\n", column*1.0/_v.C, 1.0-z0/4.0);
			printf("%d.0 %d.0 %d.0 ", column, y, z1);
			printf("%s ", north ? col_north : col_south);
			printf("white.png ");
			printf("%0.6f %0.6f\n", column*1.0/_v.C, 1.0-z1/4.0);
			last_type = cur_type;
		}
	}
}

int
main(void)
{
	int r;
	for (r = 0; r < MAXDIM && scanf("%600s", _v.field[r]) == 1; r++) {
		if (_v.C == 0) {
			_v.C = strlen(_v.field[r]);
		} else {
			CHECK(_v.C == (int) strlen(_v.field[r]));
		}
	}
	CHECK(r < MAXDIM);
	_v.R = r;

	// Place the floor.
	const char coltex[] = "0.29 0.41 0.16 white.png";
	printf("0 0 0 %s 0 0\n", coltex);
	printf("%d 0 0 %s 1 0\n", _v.C, coltex);
	printf("%d %d 0 %s 1 1\n", _v.C, _v.R, coltex);
	printf("0 %d 0 %s 0 1\n", _v.R, coltex);

	for (int c = 0; c < _v.C; c++) {
		_vertical_pass(c, true);
		_vertical_pass(c, false);
	}

	for (int r = 0; r < _v.R; r++) {
		_horizontal_pass(r, true);
		_horizontal_pass(r, false);
	}

	// Top pass for covers.
	for (int r = 0; r < _v.R; r++) {
		for (int c = 0; c < _v.C; c++) {
			if (_v.field[r][c] != 'c') {
				continue;
			}
			int rr, cc;
			for (cc = c; _v.field[r][cc+1] == 'c'; cc++)
				;
			for (rr = r; _v.field[rr+1][c] == 'c'; rr++)
				;
			for (int rrr = r; rrr <= rr; rrr++) {
				for (int ccc = c; ccc <= cc; ccc++) {
					CHECK(_v.field[rrr][ccc] == 'c');
					_v.field[rrr][ccc] = '.';
				}
			}
			const char fmt[] = "%d %d 1 0.7 0.7 0.7 white.png %d %d\n";
			printf(fmt, c, _v.R-rr-1, 0, 0);
			printf(fmt, cc+1, _v.R-rr-1, 0, 1);
			printf(fmt, cc+1, _v.R-r, 1, 1);
			printf(fmt, c, _v.R-r, 1, 0);
		}
	}

	return 0;
}

#include "headers.h"

#define _pixel2meter (1.0 / 100.0f)
#define _tex2meter (TEX_SIZE * _pixel2meter)

struct _animal_desc {
	float radius;
	vec_t size;
	rect_t alive, dead;
};

static struct _animal_desc _animals_desc[ANIMAL_TYPE_SIZE];

static void _randpos(vec_t *pos)
{
	float sw3 = screen_width / 3.0f;
	pos->x = rand()*1.0 / RAND_MAX * sw3 + sw3;
	pos->y = rand()*1.0 / RAND_MAX * (screen_height-1.0f) + 0.5f;
}

static void _action_for_rabbit(struct animal *animal)
{
	animal->source = animal->pos;
	_randpos(&animal->dest);
	animal->progress = 0.0f;
	float frames = (1.0f + 2.0f*rand()*1.0f/RAND_MAX) * 60.0f;
	animal->progress_speed = 1.0f / frames;
}

void animal_init(void)
{
	puts("Initializing the animal subsystem");

	struct _animal_desc *ad;

	ad = &_animals_desc[ANIMAL_TYPE_TARGET];
	tex_get_rect("animal_target_alive.png", &ad->alive);
	tex_get_rect("animal_target_dead.png", &ad->dead);
	ad = &_animals_desc[ANIMAL_TYPE_BIRD];
	tex_get_rect("animal_bird_alive.png", &ad->alive);
	tex_get_rect("animal_bird_dead.png", &ad->dead);
	ad = &_animals_desc[ANIMAL_TYPE_RABBIT];
	tex_get_rect("animal_rabbit_alive.png", &ad->alive);
	tex_get_rect("animal_rabbit_dead.png", &ad->dead);

	for (int i = 0; i < ANIMAL_TYPE_SIZE; ++i) {
		struct _animal_desc *ad = &_animals_desc[i];
		v_sub(&ad->size, &ad->alive.b, &ad->alive.a);
		v_scale(&ad->size, _tex2meter);
		ad->radius = (ad->size.x + ad->size.y) / 4.0f;
	}
}

void animal_destroy(void)
{
}

void animal_setup_target(struct animal *animal)
{
	animal->type = ANIMAL_TYPE_TARGET;
	animal->state = ANIMAL_STATE_ALIVE;
	_randpos(&animal->pos);
	_action_for_rabbit(animal);
}

void animal_setup_bird(struct animal *animal)
{
	animal->type = ANIMAL_TYPE_BIRD;
	animal->state = ANIMAL_STATE_ALIVE;
	_randpos(&animal->pos);
	_action_for_rabbit(animal);
}

void animal_setup_rabbit(struct animal *animal)
{
	animal->type = ANIMAL_TYPE_RABBIT;
	animal->state = ANIMAL_STATE_ALIVE;
	_randpos(&animal->pos);
	_action_for_rabbit(animal);
}

void animal_render(const struct animal *animal)
{
	struct graphics_rect_desc desc;
	rect_t r;

	struct _animal_desc *ad = &_animals_desc[animal->type];

	desc.color = &color_white;
	if (animal->state == ANIMAL_STATE_DEAD)
		desc.uv = &ad->dead;
	else
		desc.uv = &ad->alive;
	desc.pos = &animal->pos;
	r.a = (vec_t) { -ad->size.x/2.0f,  ad->size.y/2.0f };
	r.b = (vec_t) {  ad->size.x/2.0f, -ad->size.y/2.0f };
	desc.rect = &r;
	graphics_draw_rect(&desc);
}

static void _tick_bird(struct animal *animal)
{
	if (animal->progress < 1.0f)
		animal->progress += animal->progress_speed;
	animal->progress = clampf(animal->progress, 0.0f, 1.0f);

	if (animal->progress == 1.0f)
		_action_for_rabbit(animal);

	vec_t diff;
	v_sub(&diff, &animal->dest, &animal->source);
	vec_t normal = { -diff.y, diff.x };
	v_scale(&normal, sinf(animal->progress*M_PI));
	v_scale(&diff, animal->progress);
	v_add(&animal->pos, &animal->source, &diff);
	v_inc(&animal->pos, &normal);
}

static void _tick_rabbit(struct animal *animal)
{
	if (animal->progress < 1.0f)
		animal->progress += animal->progress_speed;
	animal->progress = clampf(animal->progress, 0.0f, 1.0f);

	if (animal->progress == 1.0f)
		_action_for_rabbit(animal);

	vec_t diff;
	v_sub(&diff, &animal->dest, &animal->source);
	v_scale(&diff, animal->progress);
	v_add(&animal->pos, &animal->source, &diff);
}

void animal_tick(struct animal *animal)
{
	if (animal->state == ANIMAL_STATE_DEAD)
		return;

	if (animal->type == ANIMAL_TYPE_BIRD)
		_tick_bird(animal);
	else if (animal->type == ANIMAL_TYPE_RABBIT)
		_tick_rabbit(animal);
}

// Returns the closest point on the line. line must be normalized.
static float _point_on_line(const rect_t *line, const vec_t *point)
{
	vec_t pt;
	v_sub(&pt, point, &line->a);
	return v_dot(&pt, &line->b);
}

bool animal_ishit(const struct animal *animal, rect_t *line, float *len)
{
	if (animal->state == ANIMAL_STATE_DEAD)
		return false;

	float f = _point_on_line(line, &animal->pos);
	if (f < 0.0f || f > *len)
		return false;

	// Calculate that point and check its distance
	struct _animal_desc *ad = &_animals_desc[animal->type];
	vec_t closest = line->b;
	v_scale(&closest, f);
	v_inc(&closest, &line->a);
	vec_t diff;
	v_sub(&diff, &closest, &animal->pos);
	float dist2 = v_magn(&diff);
	if (dist2 < ad->radius*ad->radius) {
		line->a = closest;
		*len = f*f;
		return true;
	} else {
		return false;
	}
}

void animal_kill(struct animal *animal)
{
	animal->state = ANIMAL_STATE_DEAD;
}

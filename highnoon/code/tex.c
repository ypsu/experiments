#include "headers.h"

__attribute__((packed))
struct tga_header {
	uint8_t id_length;
	uint8_t tga_colormap_type;
	uint8_t image_type;
	uint8_t tga_colormap[5];
	uint16_t origin_x, origin_y;
	uint16_t width, height;
	uint8_t pixel_size;
	uint8_t image_descriptor;
};

__attribute__((packed))
struct tga_color
{
	uint8_t b, g, r, a;
};

enum { MAX_TEXTURES = 32 };

static int texture_count;
static struct texture {
	char name[MAXSTR];
	float x, y, w, h;
} textures[MAX_TEXTURES];

static struct tga_color _buf[TEX_SIZE * TEX_SIZE];
static struct tga_color _line[TEX_SIZE];

static GLuint _texid;

void tex_init(void)
{
	puts("Initializing the texture manager subsystem");
	puts("\tLoading textures.tga");
	FILE *f = fopen("textures.tga", "r");
	HANDLE_CASE(f == NULL);

	struct tga_header hdr;
	HANDLE_CASE(fread(&hdr, sizeof hdr, 1, f) != 1);

	HANDLE_CASE(hdr.id_length != 0);
	HANDLE_CASE(hdr.tga_colormap_type != 0);
	HANDLE_CASE(hdr.image_type != 2);
	HANDLE_CASE(hdr.width != TEX_SIZE);
	HANDLE_CASE(hdr.height != TEX_SIZE);
	HANDLE_CASE(hdr.pixel_size != 32);
	HANDLE_CASE(hdr.image_descriptor != 8);

	size_t cnt = TEX_SIZE * TEX_SIZE;
	HANDLE_CASE(fread(_buf, sizeof _buf[0], cnt+1, f) != cnt);
	HANDLE_CASE(fclose(f) != 0);

	// Flip back the tga file (by default the picture is upside down)
	int line_sz = hdr.width * sizeof _buf[0];
	for (int i = 0; i < hdr.height/2; ++i) {
		void *a = &_buf[i * TEX_SIZE];
		void *b = &_buf[(hdr.height-i-1) * TEX_SIZE];
		memcpy(_line, a, line_sz);
		memcpy(a, b, line_sz);
		memcpy(b, _line, line_sz);
	}

	// Swap red with blue to conform with GL
	for (size_t i = 0; i < cnt; ++i) {
		struct tga_color *c = &_buf[i];
		uint8_t t = c->r;
		c->r = c->b;
		c->b = t;
	}

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &_texid);
	glBindTexture(GL_TEXTURE_2D, _texid);
	GLenum type = GL_UNSIGNED_BYTE;
	int sz = TEX_SIZE;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, type, _buf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	graphics_check_errors();

	puts("\tLoading textures.desc");
	f = fopen("textures.desc", "r");
	HANDLE_CASE(f == NULL);

	int parsed;
	char name[MAXSTR];
	float x, y, w, h;
	const char *fmt = "%30s %f %f %f %f";
	while ((parsed = fscanf(f, fmt, name, &x, &y, &w, &h)) == 5) {
		HANDLE_CASE(texture_count == MAX_TEXTURES);
		strcpy(textures[texture_count].name, name);
		textures[texture_count].x = x / TEX_SIZE;
		textures[texture_count].y = y / TEX_SIZE;
		textures[texture_count].w = w / TEX_SIZE;
		textures[texture_count].h = h / TEX_SIZE;
		texture_count += 1;
	}
	HANDLE_CASE(parsed != -1);
	HANDLE_CASE(fclose(f) != 0);
}

void tex_destroy(void)
{
	glDeleteTextures(1, &_texid);
}

void tex_get(const char *name, float *u, float *v)
{
	int id = 0;
	while (id < MAX_TEXTURES) {
		if (strcmp(textures[id].name, name) == 0)
			break;
		id += 1;
	}
	if (id == MAX_TEXTURES) {
		printf("Error: texture %s not found\n", name);
		exit(1);
	}
	const struct texture *tex = &textures[id];

	float x = *u;
	float y = *v;
	x *= tex->w;
	y *= tex->h;
	x += tex->x;
	y += tex->y;
	*u = x;
	*v = y;
}

void tex_get_rect(const char *name, rect_t *r)
{
	r->a = (vec_t) { 0.0f, 0.0f };
	r->b = (vec_t) { 1.0f, 1.0f };
	tex_get(name, &r->a.x, &r->a.y);
	tex_get(name, &r->b.x, &r->b.y);
	const float texel_offset = 0.5f / TEX_SIZE;
	r->a.x += texel_offset;
	r->a.y += texel_offset;
	r->b.x -= texel_offset;
	r->b.y -= texel_offset;
}

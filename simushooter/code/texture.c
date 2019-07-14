#include "headers.h"

vector_t
textures[TEX_LAST];

const char _header[] =
"P7\n"
"WIDTH 1024\n"
"HEIGHT 1024\n"
"DEPTH 4\n"
"MAXVAL 255\n"
"TUPLTYPE RGB_ALPHA\n"
"ENDHDR\n";

struct _texture {
	char name[MAXSTR];
	float x, y, w, h;
};

enum { _MAX_TEXTURES = 64 };

static struct {
	GLuint texid;
	int texture_count;
	struct _texture textures[_MAX_TEXTURES];
}
_v;

void
tex_init(void)
{
	puts("Loading assets/textures.pam.");
	int len;
	const char *buf = util_load_file("assets/textures.pam", &len);
	CHECK(len == strlen(_header) + TEX_SIZE*TEX_SIZE*4);
	CHECK(memcmp(buf, _header, strlen(_header)) == 0);
	const char *pixels = buf + strlen(_header);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &_v.texid);
	glBindTexture(GL_TEXTURE_2D, _v.texid);
	{
		GLenum target = GL_TEXTURE_2D;
		GLint ifmt = GL_RGBA;
		GLsizei w = TEX_SIZE;
		GLsizei h = TEX_SIZE;
		GLenum fmt = GL_RGBA;
		GLenum type = GL_UNSIGNED_BYTE;
		glTexImage2D(target, 0, ifmt, w, h, 0, fmt, type, pixels);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	ge_check_errors();

	puts("Loading assets/textures.desc.");
	FILE *f = fopen("assets/textures.desc", "r");
	CHECK(f != NULL);

	int parsed;
	char name[MAXSTR];
	float x, y, w, h;
	const char *fmt = "%30s %f %f %f %f";
	while ((parsed = fscanf(f, fmt, name, &x, &y, &w, &h)) == 5) {
		CHECK(_v.texture_count != _MAX_TEXTURES);
		strcpy(_v.textures[_v.texture_count].name, name);
		_v.textures[_v.texture_count].x = x / TEX_SIZE;
		_v.textures[_v.texture_count].y = y / TEX_SIZE;
		_v.textures[_v.texture_count].w = w / TEX_SIZE;
		_v.textures[_v.texture_count].h = h / TEX_SIZE;
		_v.texture_count += 1;
	}
	CHECK(parsed == -1);
	CHECK(fclose(f) == 0);

	char tex2file[TEX_LAST][MAXSTR] = {
		[TEX_WHITE] = "white.png",
		[TEX_AMMO_VERTICAL] = "ammo_vertical.png",
		[TEX_ROBOT] = "robot.png",
	};
	for (int i = 0; i < TEX_LAST && tex2file[i][0] != 0; i++) {
		textures[i] = (vector_t) {{ 0.0f, 0.0f, 1.0f, 1.0f }};
		tex_get(tex2file[i], &textures[i].v[0], &textures[i].v[1]);
		tex_get(tex2file[i], &textures[i].v[2], &textures[i].v[3]);
	}
}

void
tex_stop(void)
{
	glDeleteTextures(1, &_v.texid);
}

void
tex_get(const char *name, float *u, float *v)
{
	int id = 0;
	while (id < _MAX_TEXTURES) {
		if (strcmp(_v.textures[id].name, name) == 0)
			break;
		id += 1;
	}
	CHECK(id != _MAX_TEXTURES);
	const struct _texture *tex = &_v.textures[id];

	float x = *u;
	float y = *v;
	x *= tex->w;
	y *= tex->h;
	x += tex->x;
	y += tex->y;
	*u = x;
	*v = y;
}

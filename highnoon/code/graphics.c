#include "headers.h"

const float screen_width = 16.00f;
const float screen_height = 9.00f;

static const char _vertex_shader_code[] =
	"uniform vec2 u_transform;                                           \n"
	"attribute vec4 a_pos;                                               \n"
	"attribute vec2 a_texcoord;                                          \n"
	"attribute vec4 a_color;                                             \n"
	"varying vec4 v_color;                                               \n"
	"varying vec2 v_texcoord;                                            \n"
	"                                                                    \n"
	"void main()                                                         \n"
	"{                                                                   \n"
	"	vec2 p;                                                      \n"
	"	p[0] = u_transform[0]*a_pos[0] - 1.0;                        \n"
	"	p[1] = u_transform[1]*a_pos[1] - 1.0;                        \n"
	"	gl_Position = vec4(p, -1.0, 1.0);                            \n"
	"	v_color = a_color * vec4(1.0, 1.0, 1.0, 1.0);                \n"
	"	v_texcoord = a_texcoord;                                     \n"
	"}                                                                   \n"
;

static const char _fragment_shader_code[] =
	"varying vec4 v_color;                                               \n"
	"varying vec2 v_texcoord;                                            \n"
	"uniform sampler2D u_sampler;                                        \n"
	"                                                                    \n"
	"void main()                                                         \n"
	"{                                                                   \n"
	"	vec4 texcolor = texture2D(u_sampler, v_texcoord);            \n"
	"	gl_FragColor = v_color * texcolor;                           \n"
	"}                                                                   \n"
;

static GLint _att_pos;
static GLint _att_texcoord;
static GLint _att_color;
static GLint _uni_transform;

static GLuint _load_shader(GLenum type, const char *src)
{
	GLuint shader;
	GLint compiled;

	shader = glCreateShader(type);
	HANDLE_CASE(shader == 0);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		char info[4096];
		glGetShaderInfoLog(shader, 4096, NULL, info);
		printf("Error compiling:\n%s\nError:\n%s\n", src, info);
		exit(1);
	}

	return shader;
}

static void _setup_shaders(void)
{
	GLuint vs, fs, po;
	vs = _load_shader(GL_VERTEX_SHADER, _vertex_shader_code);
	fs = _load_shader(GL_FRAGMENT_SHADER, _fragment_shader_code);
	po = glCreateProgram();
	HANDLE_CASE(po == 0);
	glAttachShader(po, vs);
	glAttachShader(po, fs);
	glLinkProgram(po);

	GLint linked;
	glGetProgramiv(po, GL_LINK_STATUS, &linked);
	if (!linked) {
		char info[4096];
		glGetProgramInfoLog(po, 4096, NULL, info);
		printf("Unable to link shaders:\n%s\n", info);
		exit(1);
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
	glUseProgram(po);

	_att_pos = glGetAttribLocation(po, "a_pos");
	HANDLE_CASE(_att_pos == -1);
	glEnableVertexAttribArray(_att_pos);

	_att_texcoord = glGetAttribLocation(po, "a_texcoord");
	HANDLE_CASE(_att_texcoord == -1);
	glEnableVertexAttribArray(_att_texcoord);

	_att_color = glGetAttribLocation(po, "a_color");
	HANDLE_CASE(_att_color == -1);
	glEnableVertexAttribArray(_att_color);

	_uni_transform = glGetUniformLocation(po, "u_transform");
	HANDLE_CASE(_uni_transform == -1);

	float transform[2] = { 2.0f / screen_width, 2.0f / screen_height };
	glUniform2fv(_uni_transform, 1, transform);
}

void graphics_init(void)
{
	puts("Initializing the graphics subsystem");
	puts("\tConfiguring OpenGL");
	glViewport(0, 0, video_width, video_height);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	puts("\tCompiling shaders");
	_setup_shaders();
	graphics_check_errors();
}

void graphics_destroy(void)
{
}

void graphics_check_errors(void)
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		printf("OpenGL error occured: %d!\n", error);
		exit(1);
	}
}

enum { _VERTICES_SIZE = 4096 };

static int _vertices_count;
static struct vertex _vertices[_VERTICES_SIZE];

struct vertex *graphics_vertices_get(int n)
{
	int sz = _vertices_count;
	int nsz = sz + n;
	HANDLE_CASE(nsz > _VERTICES_SIZE);
	_vertices_count = nsz;
	return &_vertices[sz];
}

void graphics_vertices_draw(void)
{
	struct vertex *v = _vertices;
	int n = _vertices_count;
	GLenum attr;

	const int sz = sizeof v[0];
	attr = _att_pos;
	glVertexAttribPointer(attr, 4, GL_FLOAT, GL_FALSE, sz, &v->xy.x);
	attr = _att_texcoord;
	glVertexAttribPointer(attr, 2, GL_FLOAT, GL_FALSE, sz, &v->uv.x);
	attr = _att_color;
	glVertexAttribPointer(attr, 4, GL_FLOAT, GL_FALSE, sz, &v->color);

	glDrawArrays(GL_TRIANGLES, 0, n);

	state.vertices_drawn += _vertices_count;
	_vertices_count = 0;
}

void graphics_draw_rect(const struct graphics_rect_desc *desc)
{
	struct vertex *vertices = graphics_vertices_get(6);

	float x, y, nx, ny;
	float u0, v0, u1, v1;
	x = desc->rect->a.x + desc->pos->x;
	y = desc->rect->a.y + desc->pos->y;
	nx = desc->rect->b.x + desc->pos->x;
	ny = desc->rect->b.y + desc->pos->y;
	u0 = desc->uv->a.x;
	v0 = desc->uv->a.y;
	u1 = desc->uv->b.x;
	v1 = desc->uv->b.y;

	vertices[0] = (struct vertex) {
		{ x, y },
		{ u0, v0 },
		*desc->color,
	};
	vertices[1] = (struct vertex) {
		{ x, ny },
		{ u0, v1 },
		*desc->color,
	};
	vertices[2] = (struct vertex) {
		{ nx, ny },
		{ u1, v1 },
		*desc->color,
	};
	vertices[3] = (struct vertex) {
		{ x, y },
		{ u0, v0 },
		*desc->color,
	};
	vertices[4] = (struct vertex) {
		{ nx, ny },
		{ u1, v1 },
		*desc->color,
	};
	vertices[5] = (struct vertex) {
		{ nx, y },
		{ u1, v0 },
		*desc->color,
	};
}

void graphics_draw_rect_rot(const struct graphics_rect_desc *desc, float ang)
{
	struct vertex *vertices = graphics_vertices_get(6);

	float x, y, nx, ny;
	float u0, v0, u1, v1;
	x = desc->rect->a.x;
	y = desc->rect->a.y;
	nx = desc->rect->b.x;
	ny = desc->rect->b.y;
	u0 = desc->uv->a.x;
	v0 = desc->uv->a.y;
	u1 = desc->uv->b.x;
	v1 = desc->uv->b.y;

	vec_t v;
	v = (vec_t) {  x,  y }; v_rotate(&v, ang); v_inc(&v, desc->pos);
	vertices[0] = (struct vertex) {
		v,
		{ u0, v0 },
		*desc->color,
	};
	v = (vec_t) {  x, ny }; v_rotate(&v, ang); v_inc(&v, desc->pos);
	vertices[1] = (struct vertex) {
		v,
		{ u0, v1 },
		*desc->color,
	};
	v = (vec_t) { nx, ny }; v_rotate(&v, ang); v_inc(&v, desc->pos);
	vertices[2] = (struct vertex) {
		v,
		{ u1, v1 },
		*desc->color,
	};
	v = (vec_t) {  x,  y }; v_rotate(&v, ang); v_inc(&v, desc->pos);
	vertices[3] = (struct vertex) {
		v,
		{ u0, v0 },
		*desc->color,
	};
	v = (vec_t) { nx, ny }; v_rotate(&v, ang); v_inc(&v, desc->pos);
	vertices[4] = (struct vertex) {
		v,
		{ u1, v1 },
		*desc->color,
	};
	v = (vec_t) { nx,  y }; v_rotate(&v, ang); v_inc(&v, desc->pos);
	vertices[5] = (struct vertex) {
		v,
		{ u1, v0 },
		*desc->color,
	};
}

void graphics_save_sshot(const char *fname)
{
	int w = video_width;
	int h = video_height;

	FILE *f = fopen(fname, "wb");
	HANDLE_CASE(f == NULL);
	fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);
	char *data = smalloc(w*h*3);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

	// Flip the rows
	for (int r1 = 0, r2 = h-1; r1 < r2; ++r1, --r2) {
		char *p = data + r1*w*3;
		char *q = data + r2*w*3;
		for (int i = 0; i < w*3; ++i) {
			char t = p[i];
			p[i] = q[i];
			q[i] = t;
		}
	}

	HANDLE_CASE(fwrite(data, w*h*3, 1, f) != 1);
	HANDLE_CASE(fclose(f) != 0);
}

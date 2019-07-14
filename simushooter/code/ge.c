#include "headers.h"

int
window_width,
window_height;

enum { MAX_QUADS = 1024 };

static struct {
	GLint att_pos;
	GLint att_texcoord;
	GLint att_color;
	GLint uni_proj;
	GLint uni_view;
	GLint uni_blackwhite_factor;
	matrix_t ortho_transform;
	matrix_t proj_transform;

	int static_size, dynamic_size, ortho_size;
	struct vertex static_vertices[MAX_QUADS*4];
	struct vertex dynamic_vertices[MAX_QUADS*4];
	struct vertex ortho_vertices[MAX_QUADS*4];
}
_v;

static const char
_vertex_shader_code[] =
	"uniform mat4 u_proj;                                               \n"
	"uniform mat4 u_view;                                               \n"
	"attribute vec4 a_pos;                                              \n"
	"attribute vec2 a_texcoord;                                         \n"
	"attribute vec4 a_color;                                            \n"
	"varying vec4 v_color;                                              \n"
	"varying vec2 v_texcoord;                                           \n"
	"                                                                   \n"
	"void main()                                                        \n"
	"{                                                                  \n"
	"	vec4 p = u_proj * u_view * a_pos;                           \n"
	"	gl_Position = p;                                            \n"
	"	v_color = a_color;                                          \n"
	"	v_texcoord = a_texcoord;                                    \n"
	"}                                                                  \n";

static const char
_fragment_shader_code[] =
	"varying vec4 v_color;                                              \n"
	"varying vec2 v_texcoord;                                           \n"
	"uniform sampler2D u_sampler;                                       \n"
	"                                                                   \n"
	"void main()                                                        \n"
	"{                                                                  \n"
	"	vec4 texcolor = texture2D(u_sampler, v_texcoord);           \n"
	"	if (texcolor.a != 1.0)                                      \n"
	"		discard;                                            \n"
	"	gl_FragColor = v_color * texcolor;                          \n"
	"}                                                                  \n";

static GLuint
_load_shader(GLenum type, const char *src)
{
	GLuint shader;
	GLint compiled;

	shader = glCreateShader(type);
	CHECK(shader != 0);
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

static void
_setup_shaders(void)
{
	GLuint vs, fs, po;
	vs = _load_shader(GL_VERTEX_SHADER, _vertex_shader_code);
	fs = _load_shader(GL_FRAGMENT_SHADER, _fragment_shader_code);
	po = glCreateProgram();
	CHECK(po != 0);
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

	_v.att_pos = glGetAttribLocation(po, "a_pos");
	CHECK(_v.att_pos != -1);
	glEnableVertexAttribArray(_v.att_pos);

	_v.att_color = glGetAttribLocation(po, "a_color");
	CHECK(_v.att_color != -1);
	glEnableVertexAttribArray(_v.att_color);

	_v.att_texcoord = glGetAttribLocation(po, "a_texcoord");
	CHECK(_v.att_texcoord != -1);
	glEnableVertexAttribArray(_v.att_texcoord);

	_v.uni_proj = glGetUniformLocation(po, "u_proj");
	CHECK(_v.uni_proj != -1);
	_v.uni_view = glGetUniformLocation(po, "u_view");
	CHECK(_v.uni_view != -1);
}

static void
_setup_proj_matrix(void)
{
	float max_dimension = maxf(window_width, window_height);
	float fov = g.fov;
	float ctg = 1.0f / tan(fov/2.0f);

	// near plane
	float n = 0.10f;
	// far plane
	float f = 200.0f;
	// fov fixup for x
	float rx = max_dimension / window_width;
	// fov fixup for y
	float ry = max_dimension / window_height;

	_v.proj_transform = m_zero;
	_v.proj_transform.m[0][0] = rx * ctg;
	_v.proj_transform.m[1][2] = ry * ctg;
	_v.proj_transform.m[2][1] = (f+n) / (f-n);
	_v.proj_transform.m[2][3] = -2.0f*f*n / (f-n);
	_v.proj_transform.m[3][1] = 1.0f;
}

void
ge_init(void)
{
	puts("Initializing the graphics subsystem.");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

	// Setup the index buffer object for the quads.
	short *indices;
	int sz = MAX_QUADS*6 * sizeof indices[0];
	indices = qalloc(sz);
	for (int q = 0; q < MAX_QUADS; q++) {
		indices[q*6+0] = 4*q+0;
		indices[q*6+1] = 4*q+1;
		indices[q*6+2] = 4*q+2;
		indices[q*6+3] = 4*q+0;
		indices[q*6+4] = 4*q+2;
		indices[q*6+5] = 4*q+3;
	}

	GLuint ibo;
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz, indices, GL_STATIC_DRAW);

	puts("\tCompiling the shaders.");
	_setup_shaders();

	ge_reinit_window();
}

void
ge_reinit_window(void)
{
	const char fmt[] = "Setting window size to %dx%d.\n";
	printf(fmt, window_width, window_height);
	glViewport(0, 0, window_width, window_height);
	_v.ortho_transform = (matrix_t) {{
		{ 2.0f / window_width, 0.0f, 0.0f, -1.0f },
		{ 0.0f, -2.0f / window_height, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f, -1.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f },
	}};
	_setup_proj_matrix();
	ge_check_errors();
}

void
ge_stop(void)
{
}

void
ge_check_errors(void)
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		printf("OpenGLES error occurred: %d.\n", error);
		exit(1);
	}
}

void
ge_load_static_geometry(void)
{
	puts("Loading world.geom.");

	FILE *f;
	CHECK((f = fopen("assets/world.geom", "r")) != NULL);

	int sz;
	float max_z = 0.0f;
	for (sz = 0; sz < MAX_QUADS*4; sz++) {
		float x, y, z;
		float r, g, b;
		if (fscanf(f, "%f %f %f", &x, &y, &z) != 3) break;
		if (fscanf(f, "%f %f %f", &r, &g, &b) != 3) break;
		char tex[64];
		float s, t;
		CHECK(fscanf(f, "%60s %f %f", tex, &s, &t) == 3);
		tex_get(tex, &s, &t);
		_v.static_vertices[sz] = (struct vertex) {
			.pos = {{ x, y, z, 1.0f }},
			.s = s,
			.t = t,
		};
		v_set(&_v.static_vertices[sz].color, r, g, b, 1.0f);
		if (z > max_z) {
			max_z = z;
		}
	}

	CHECK(sz < MAX_QUADS*4);
	CHECK(sz%4 == 0);
	_v.static_size = sz / 4;
	map_height = max_z;

	CHECK(fclose(f) == 0);
}

struct vertex *
ge_get_dynamic_vertices(int n)
{
	CHECK(_v.dynamic_size + n <= MAX_QUADS);
	_v.dynamic_size += n;
	return _v.dynamic_vertices + (_v.dynamic_size - n)*4;
}

struct vertex *
ge_get_ortho_vertices(int n)
{
	CHECK(_v.ortho_size + n <= MAX_QUADS);
	_v.ortho_size += n;
	return _v.ortho_vertices + (_v.ortho_size - n)*4;
}

static void
_set_matrix(GLint uni, const matrix_t *m)
{
	matrix_t t = *m;
	m_transpose(&t);
	glUniformMatrix4fv(uni, 1, GL_FALSE, &t.m[0][0]);
}

static void
_draw_vertices(const struct vertex *v, int n)
{
	if (n == 0)
		return;

	GLenum attr;

	const int sz = sizeof v[0];
	attr = _v.att_pos;
	glVertexAttribPointer(attr, 4, GL_FLOAT, GL_FALSE, sz, &v->pos);
	attr = _v.att_color;
	glVertexAttribPointer(attr, 4, GL_FLOAT, GL_FALSE, sz, &v->color);
	attr = _v.att_texcoord;
	glVertexAttribPointer(attr, 2, GL_FLOAT, GL_FALSE, sz, &v->s);

	glDrawElements(GL_TRIANGLES, n*6, GL_UNSIGNED_SHORT, NULL);
}

void
ge_submit(void)
{
	// Print the scene complexity (for debugging).
	const char *quad_count = qprintf("quads: %d", _v.dynamic_size);
	int xoff = window_width - strlen(quad_count)*font_x;
	font_draw(xoff, font_y, &color_black, quad_count);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	_setup_proj_matrix();
	_set_matrix(_v.uni_proj, &_v.proj_transform);

	// Calculate the view transformation.
	matrix_t zrot, xrot, rot, pos, view;
	m_set_zrot(&zrot, -g.camera.zrot + M_PI/2.0f);
	m_set_xrot(&xrot, -g.camera.xrot);
	m_mult(&rot, &xrot, &zrot);
	pos = m_id;
	pos.m[0][3] = -g.camera.pos.v[0];
	pos.m[1][3] = -g.camera.pos.v[1];
	pos.m[2][3] = -g.camera.pos.v[2];
	m_mult(&view, &rot, &pos);
	_set_matrix(_v.uni_view, &view);

	// TODO: 1. static VBO
	glEnable(GL_DEPTH_TEST);
	_draw_vertices(_v.static_vertices, _v.static_size);

	// 2. dynamic geometry
	_draw_vertices(_v.dynamic_vertices, _v.dynamic_size);
	_v.dynamic_size = 0;

	// 3. ortho geometry
	glDisable(GL_DEPTH_TEST);
	_set_matrix(_v.uni_proj, &_v.ortho_transform);
	_set_matrix(_v.uni_view, &m_id);
	_draw_vertices(_v.ortho_vertices, _v.ortho_size);
	_v.ortho_size = 0;

	ge_check_errors();
}

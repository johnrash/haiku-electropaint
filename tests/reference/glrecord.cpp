/*
 * Recording implementations of the OpenGL entry points the engine uses, of
 * ElectroPortis' wrap_gl*() functions that ep.c calls, and of
 * gluPerspective() (as in SGI's GLU) for ep.c's reshape().
 */
#include "glrecord.h"

#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>


GLTrace gTrace;


namespace {


enum {
	kColor3f, kPushMatrix, kRotatef, kTranslatef, kPopMatrix, kScalef,
	kBegin, kVertex2f, kEnd, kClear, kFinish, kViewport, kMatrixMode,
	kLoadIdentity, kMultMatrixd, kClearColor, kShadeModel
};

const char* kNames[] = {
	"glColor3f", "glPushMatrix", "glRotatef", "glTranslatef", "glPopMatrix",
	"glScalef", "glBegin", "glVertex2f", "glEnd", "glClear", "glFinish",
	"glViewport", "glMatrixMode", "glLoadIdentity", "glMultMatrixd",
	"glClearColor", "glShadeModel"
};

// Which arguments are floats (f), doubles (d) or integers (i).
const char* kTypes[] = {
	"fff", "", "ffff", "fff", "", "fff", "i", "ff", "", "i", "", "iiii", "i",
	"", "dddddddddddddddd", "ffff", "i"
};


uint64_t
bits(float value)
{
	uint32_t raw;
	memcpy(&raw, &value, sizeof(raw));
	return raw;
}


uint64_t
bits(double value)
{
	uint64_t raw;
	memcpy(&raw, &value, sizeof(raw));
	return raw;
}


void
record(int op, int count = 0, const uint64_t* args = NULL)
{
	GLCall call;
	memset(&call, 0, sizeof(call));
	call.op = op;
	call.count = count;
	for (int i = 0; i < count; i++)
		call.args[i] = args[i];
	gTrace.push_back(call);
}


void
record_floats(int op, float a, float b, float c)
{
	uint64_t args[3] = { bits(a), bits(b), bits(c) };
	record(op, 3, args);
}


void
record_floats(int op, float a, float b, float c, float d)
{
	uint64_t args[4] = { bits(a), bits(b), bits(c), bits(d) };
	record(op, 4, args);
}


}	// namespace


bool
GLCall::operator==(const GLCall& other) const
{
	return op == other.op && count == other.count
		&& memcmp(args, other.args, sizeof(args[0]) * count) == 0;
}


std::string
GLCall::ToString() const
{
	std::string string = kNames[op];
	string += "(";
	for (int i = 0; i < count; i++) {
		char buffer[64];
		char type = kTypes[op][i];
		if (type == 'f') {
			float value;
			uint32_t raw = (uint32_t)args[i];
			memcpy(&value, &raw, sizeof(value));
			snprintf(buffer, sizeof(buffer), "%.9g", value);
		} else if (type == 'd') {
			double value;
			memcpy(&value, &args[i], sizeof(value));
			snprintf(buffer, sizeof(buffer), "%.17g", value);
		} else
			snprintf(buffer, sizeof(buffer), "%lld", (long long)args[i]);
		if (i > 0)
			string += ", ";
		string += buffer;
	}
	return string + ")";
}


// #pragma mark - OpenGL


extern "C" {


void glColor3f(GLfloat r, GLfloat g, GLfloat b)
{ record_floats(kColor3f, r, g, b); }

void glPushMatrix(void)
{ record(kPushMatrix); }

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{ record_floats(kRotatef, angle, x, y, z); }

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{ record_floats(kTranslatef, x, y, z); }

void glPopMatrix(void)
{ record(kPopMatrix); }

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{ record_floats(kScalef, x, y, z); }

void glBegin(GLenum mode)
{ uint64_t args[1] = { mode }; record(kBegin, 1, args); }

void glVertex2f(GLfloat x, GLfloat y)
{ uint64_t args[2] = { bits(x), bits(y) }; record(kVertex2f, 2, args); }

void glEnd(void)
{ record(kEnd); }

void glClear(GLbitfield mask)
{ uint64_t args[1] = { mask }; record(kClear, 1, args); }

void glFinish(void)
{ record(kFinish); }

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	uint64_t args[4] = { (uint64_t)x, (uint64_t)y, (uint64_t)width,
		(uint64_t)height };
	record(kViewport, 4, args);
}

void glMatrixMode(GLenum mode)
{ uint64_t args[1] = { mode }; record(kMatrixMode, 1, args); }

void glLoadIdentity(void)
{ record(kLoadIdentity); }

void glMultMatrixd(const GLdouble* m)
{
	uint64_t args[16];
	for (int i = 0; i < 16; i++)
		args[i] = bits(m[i]);
	record(kMultMatrixd, 16, args);
}

void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{ record_floats(kClearColor, r, g, b, a); }

void glShadeModel(GLenum mode)
{ uint64_t args[1] = { mode }; record(kShadeModel, 1, args); }


// SGI GLU's gluPerspective(), libutil/project.c.
void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear,
	GLdouble zFar)
{
	GLdouble m[4][4];
	double sine, cotangent, deltaZ;
	double radians = fovy / 2 * 3.14159265358979323846 / 180;

	deltaZ = zFar - zNear;
	sine = sin(radians);
	if (deltaZ == 0 || sine == 0 || aspect == 0)
		return;
	cotangent = cos(radians) / sine;

	memset(m, 0, sizeof(m));
	m[0][0] = 1; m[1][1] = 1; m[2][2] = 1; m[3][3] = 1;
	m[0][0] = cotangent / aspect;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[2][3] = -1;
	m[3][2] = -2 * zNear * zFar / deltaZ;
	m[3][3] = 0;
	glMultMatrixd(&m[0][0]);
}


// #pragma mark - ElectroPortis' wrappers, called by ep.c


typedef unsigned long wincount_t;

void wrap_glColor3f(GLfloat r, GLfloat g, GLfloat b, const wincount_t)
{ glColor3f(r, g, b); }

void wrap_glPushMatrix(const wincount_t)
{ glPushMatrix(); }

void wrap_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z,
	const wincount_t)
{ glRotatef(angle, x, y, z); }

void wrap_glTranslatef(GLfloat x, GLfloat y, GLfloat z, const wincount_t)
{ glTranslatef(x, y, z); }

void wrap_glPopMatrix(const wincount_t)
{ glPopMatrix(); }

void wrap_glScalef(GLfloat x, GLfloat y, GLfloat z, const wincount_t)
{ glScalef(x, y, z); }

void wrap_glBegin(GLenum mode, const wincount_t)
{ glBegin(mode); }

void wrap_glVertex2f(GLfloat x, GLfloat y, const wincount_t)
{ glVertex2f(x, y); }

void wrap_glEnd(const wincount_t)
{ glEnd(); }

void wrap_glClear(GLbitfield mask, const wincount_t)
{ glClear(mask); }

void wrap_glFinish(const wincount_t)
{ glFinish(); }


}	// extern "C"

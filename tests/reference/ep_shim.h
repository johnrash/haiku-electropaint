/*
 * Replaces the Windows headers of ElectroPortis' ep.c so that the original
 * decompiled code builds on Linux x86_64. ep.c keeps pointers in 32-bit
 * MIPS registers: it only works when everything it points to lies below
 * 2 GB, so the test binary is linked without PIE (static data at 0x400000)
 * and malloc() comes from a MAP_32BIT arena (lowmem.c).
 */
#ifndef EP_SHIM_H
#define EP_SHIM_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glu.h>

typedef unsigned long wincount_t;

typedef union EPANOS_REG {
	uint8_t u8;
	int32_t i32;
	uint32_t u32;
	int64_t i64;
	uint64_t u64;
	float s;
	double d;
} EPANOS_REG;

typedef struct EPANOS_ARGS {
	EPANOS_REG v0;
	EPANOS_REG v1;
	EPANOS_REG a0;
	EPANOS_REG a1;
	EPANOS_REG a2;
	EPANOS_REG a3;
	EPANOS_REG a4;
	EPANOS_REG a5;
	EPANOS_REG a6;
	EPANOS_REG a7;
	EPANOS_REG f0;
	EPANOS_REG f2;
	EPANOS_REG f12;
	EPANOS_REG f13;
	EPANOS_REG f14;
	EPANOS_REG f15;
	EPANOS_REG f16;
	EPANOS_REG f17;
	EPANOS_REG f18;
	EPANOS_REG f19;
} EPANOS_ARGS;

void* lowmalloc(size_t size);
void lowfree(void* pointer);
#define malloc lowmalloc
#define free lowfree

extern long ep_ref_seed;

void wrap_glColor3f(GLfloat r, GLfloat g, GLfloat b, const wincount_t wincount);
void wrap_glPushMatrix(const wincount_t wincount);
void wrap_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z,
	const wincount_t wincount);
void wrap_glTranslatef(GLfloat x, GLfloat y, GLfloat z,
	const wincount_t wincount);
void wrap_glPopMatrix(const wincount_t wincount);
void wrap_glScalef(GLfloat x, GLfloat y, GLfloat z, const wincount_t wincount);
void wrap_glBegin(GLenum mode, const wincount_t wincount);
void wrap_glVertex2f(GLfloat x, GLfloat y, const wincount_t wincount);
void wrap_glEnd(const wincount_t wincount);
void wrap_glClear(GLbitfield mask, const wincount_t wincount);
void wrap_glFinish(const wincount_t wincount);

#endif /* EP_SHIM_H */

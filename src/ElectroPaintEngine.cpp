/*
 * ElectroPaint animation engine.
 *
 * This file is part of a decompilation of ElectroPaint, a work by David
 * Tristram, distributed with permission. ElectroPaint is a US trademark owned
 * by Tristram Visual, a sole proprietorship operating in California. Inquiries
 * may be directed to info@tristram.com and www.tristram.com.
 *
 * Some further clarification from an email conversation with David Tristram:
 *
 * "The implication regarding derived works is simply that any code that you may
 * produce that includes any part of ElectroPaint does not belong to you, so you
 * can't sell it, for example, or if you do, I'd have a legal interest in the
 * proceeds. If you are interested in modifying the program's behavior or
 * appearance, I think that's okay as long as you share the modified source with
 * me. All this includes decompiled source recovered from the binaries."
 *
 * Rewritten by hand from ElectroPortis' ep.c, the decompiled IRIX binary. The
 * decompiler kept MIPS registers and 32-bit pointers, so ep.c only runs where
 * pointers are 32 bits wide; this version uses plain structures instead. Every
 * floating point expression keeps the precision (float or double) and the
 * operand order of ep.c, the quirks of the original binary are reproduced on
 * purpose and marked "As in ep.c", and the random numbers come from the same
 * 48-bit generator as drand48(). tests/reference compares the GL calls of
 * this engine with those of ep.c.
 */


#include "ElectroPaintEngine.h"

#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


namespace ElectroPaint {


// The original's "actAnim": one animated parameter ("actor").
struct Engine::Actor {
	bool		active;		// animated by the sequences
	bool		moving;		// on its way to target
	float		direction;	// sign of step
	float		target;
	float		limit1;		// target range, set by actlim1/actlim2
	float		limit2;
	float		step;		// change per frame
	float		value;
	float		duration;	// frames per move, copied from the sequence
	float		variance;	// randomness of duration
	float		resetValue;	// value for actreset (never set: 0)
};


// The original's "animCommand": one script line, ordered by frame.
struct Engine::Command {
	int32_t		type;
	int32_t		index;		// actor or sequence number
	float		frame;		// relative to the sequence's start
	float		value;
	Sequence*	sequence;
	Command*	next;
};


// The original's "animSeq": a named list of commands.
struct Engine::Sequence {
	int32_t		id;
	bool		active;
	float		startFrame;
	float		length;		// relative frame of the last command added
	float		duration;
	float		variance;
	Command*	first;
	Command*	current;	// next command to run, NULL when stopped
	Command*	last;
	Sequence*	next;
	Sequence*	nextKilled;	// waiting to be freed
};


namespace {


enum {
	kFrame			= 0,	// "frame:" and "relframe:"
	kAbsFrame		= 1,
	kActLimit1		= 2,
	kActLimit2		= 3,
	kActSet			= 4,
	kActReset		= 5,
	kActResetAll	= 6,
	kActStop		= 7,
	kActStopAll		= 8,
	kSeqName		= 9,
	kSeqDo			= 10,
	kSeqStop		= 11,
	kSeqLoop		= 12,
	kSeqRepeat		= 13,
	kSeqKill		= 14,
	kSeqKillAll		= 15,
	kDuration		= 101,
	kVariance		= 102,
	kRandDelay		= 103
};

// The actors the original creates. The script also sets 17 ("auto") and 29
// ("full"), which do not exist.
enum {
	kActorOutline	= 18,
	kActorZoom		= 31,
	kActorTwist		= 38,
	kActorSpeed		= 45,
	kActorCount		= 52,	// "n": number of squares drawn
	kActorWheel		= 73,
	kActorSpin		= 80,
	kActorFlip		= 87,
	kActorArm		= 94,
	kActorWrist		= 101,
	kActorSize		= 108,	// set by the script, but never copied into fSize
	kActorHueRate	= 127,	// set by the script, never read
	kActorHue		= 123,
	kActorLight		= 130
};

const float kSquare[8] = {
	-0.1f, -0.1f, 0.1f, -0.1f, 0.1f, 0.1f, -0.1f, 0.1f
};

const char* kDefaultScript =
	"# created by mkmaster \n"
	"#\n"
	"# mello script\n"
	"#\n"
	"#\tDave Tristram\n"
	"#\n"
	"#\tthis script moves gently on the screen, and uses wheel to maintain\n"
	"#\ta 3D look.\n"
	"#\n"
	"#\n"
	"# constants\n"
	"#\n"
	"actset: 29, 1.0\t\t# \"full\"\n"
	"actset: 17, 0.0\t\t# \"auto\"\n"
	"actset: 18, 1.0\t\t# \"outline\"\n"
	"# actset: 73, 0.107258\t# \"whl\"\n"
	"actset: 31, 0.06\t# \"zoom\"\n"
	"actset: 38, 60.0\t# \"twst\"\n"
	"#actset: 80, 0.23\t# \"spn\" slow\n"
	"actset: 108, 1.2\t# \"size\"\n"
	"actset: 52, 40.0\t# \"n\"\n"
	"#\n"
	"seqdo: 1\t# wrist\n"
	"seqdo: 2\t# color\n"
	"seqdo: 3\t# wheel\n"
	"seqdo: 4\t# spin\n"
	"seqdo: 5\t# flip\n"
	"seqdo: 6\t# arm\n"
	"seqdo: 7\t# twist\n"
	"seqdo: 8\t# outline\n"
	"#\n"
	"# \n"
	"#\n"
	"# wrst anim: gentle radius modulation\n"
	"#\n"
	"seqname: 1\n"
	"#\n"
	"duration: 160\n"
	"actlim1: 101, -1.5\t\t# wrst\n"
	"actlim2: 101,  1.5\t\t# wrst\n"
	"#\n"
	"#\n"
	"#\n"
	"# color anim: hue and lightness motion\n"
	"#\n"
	"#\tthe hues chosen attempt to minimize \"green-out\"\n"
	"#\n"
	"seqname: 2\n"
	"#\n"
	"duration: 60\n"
	"actset: 127, 0.0\t\t# hue rate chan 0\n"
	"#\n"
	"actlim1: 123, 0.544053\t\t# hue chan 0 (just near cyan)\n"
	"actlim2: 123, 1.295\t\t# hue chan 0 (deep green)\n"
	"#\n"
	"#\n"
	"duration: 80\n"
	"#\n"
	"actlim1: 130, 0.0\t\t# lightness chan 0 (black)\n"
	"actlim2: 130, 1.0\t\t# lightness chan 0 (white)\n"
	"#\n"
	"#\n"
	"#\n"
	"# wheel anim: very slow, gentle rocking\n"
	"#\n"
	"seqname: 3\n"
	"#\n"
	"duration: 120\n"
	"#\n"
	"actlim1: 73, 0.137\t\t# wheel, real slow\n"
	"actlim2: 73, -0.137\t\t# wheel, real slow\n"
	"#\n"
	"#\n"
	"#\n"
	"# spin anim: somewhat fast occasionally\n"
	"#\n"
	"seqname: 4\n"
	"#\n"
	"duration: 100\n"
	"#\n"
	"actlim1: 80,  0.23\t\t# spn slow\n"
	"actlim2: 80, -0.23\t\t# spn slow\n"
	"#\n"
	"randdelay: 1000\n"
	"#\n"
	"#\n"
	"duration: 40\n"
	"#\n"
	"actlim1: 80,  5.23\t\t# spn fast\n"
	"actlim2: 80, -5.23\t\t# spn fast\n"
	"#\n"
	"#randdelay: 100\n"
	"randdelay: 200\n"
	"#\n"
	"seqloop:\n"
	"#\n"
	"#\n"
	"#\n"
	"# flip anim: somewhat fast occasionally\n"
	"#\n"
	"seqname: 5\n"
	"#\n"
	"duration: 50\n"
	"#\n"
	"actlim1: 87,  2.0\t\t# flip slow\n"
	"actlim2: 87, -2.0\t\t# flip slow\n"
	"#\n"
	"#randdelay: 500\n"
	"randdelay: 1200\n"
	"#\n"
	"#\n"
	"actlim1: 87,  10.0\t\t# flip fast\n"
	"actlim2: 87, -10.0\t\t# flip fast\n"
	"#\n"
	"#randdelay: 80\n"
	"randdelay: 220\n"
	"#\n"
	"#\n"
	"seqloop:\n"
	"#\n"
	"#\n"
	"#\n"
	"# arm anim: gentle radius modulation\n"
	"#\n"
	"seqname: 6\n"
	"#\n"
	"duration: 90\n"
	"actlim1: 94, -2.0\t\t# arm\n"
	"actlim2: 94,  2.0\t\t# arm\n"
	"#\n"
	"#\n"
	"#\n"
	"# twist anim:\n"
	"#\n"
	"seqname: 7\n"
	"#\n"
	"#duration: 1750\n"
	"duration: 2250\n"
	"actlim1: 38, 200.0\t\t# twst\n"
	"actlim2: 38, -200.0\t\t# twst\n"
	"#\n"
	"#\n"
	"#\n"
	"# outline anim: on and off infrequently, mostly on\n"
	"#\n"
	"#\n"
	"seqname: 8\n"
	"#\n"
	"duration: 5000\n"
	"#\n"
	"actlim1: 18, 0.2\t\t# outline\n"
	"actlim2: 18, 1.0\t\t# outline\n"
	"#\n"
	"#\n"
	"#\n"
	"# end of generated script";


// The history arrays are rings of 128 slots.
inline int
wrap(int i)
{
	int r = i % 128;
	return r < 0 ? r + 128 : r;
}


inline int
previous(int i)
{
	return i <= 0 ? i - 1 + 128 : i - 1;
}


// Linear interpolation between slot i and the slot before it.
inline float
twixt(int i, const float* a, float t)
{
	return a[i] * t + a[previous(i)] * (1.0f - t);
}


// Like twixt(), for values that wrap around at range (angles, hues).
inline float
foldtwixt(int i, const float* a, float t, float range)
{
	float current = a[i];
	float last = a[previous(i)];
	float delta = current - last;
	float rest = 1.0f - t;
	if (range * 0.5f < delta)
		return rest * (last + range) + current * t;
	if (delta < range * -0.5f)
		return rest * last + t * (current + range);
	return rest * last + current * t;
}


float
value(float n1, float n2, float hue)
{
	if (360.0f < hue)
		hue += -360.0f;
	if (hue < 0.0f)
		hue += 360.0f;
	if (hue < 60.0f)
		return (double)n1 + (double)(n2 - n1) * ((double)hue / 60.0);
	if (hue < 180.0f)
		return n2;
	if (hue < 240.0f)
		return (double)n1 + (double)(n2 - n1) * ((double)(240.0f - hue) / 60.0);
	return n1;
}


// hue in turns, lightness and saturation 0..1.
void
hls_to_rgb(float hue, float lightness, float saturation, float* red,
	float* green, float* blue)
{
	float m2;
	if ((double)lightness <= 0.5)
		m2 = lightness * (saturation + 1.0f);
	else
		m2 = (lightness + saturation) - lightness * saturation;

	if (saturation == 0.0f) {
		*blue = lightness;
		*green = lightness;
		*red = lightness;
		return;
	}

	float degrees = (double)hue * 360.0;
	float m1 = lightness * 2.0f - m2;
	*red = value(m1, m2, (double)degrees + 120.0);
	*green = value(m1, m2, degrees);
	*blue = value(m1, m2, (double)degrees + -120.0);
}


// MIPS trunc.w.d as the decompiler emulates it.
int32_t
trunc_w_d(double x)
{
	double r = trunc(x);
	if (r > INT32_MAX || r < INT32_MIN)
		return INT32_MAX;
	return (int32_t)r;
}


}	// namespace


Engine::Engine(uint32_t seed, const char* script)
	:
	fSequences(NULL),
	fEditSequence(NULL),
	fKilled(NULL),
	fCurrentFrame(0.0f),
	fRelFrame(0.0f),
	fBaseFrame(0.0f),
	fAbsFrame(0.0f),
	fWheel(0.0f),
	fDeltaWheel(0.0f),
	fGlobalFlip(0.0f),
	fDeltaFlip(0.0f),
	fGlobalSpin(0.0f),
	fDeltaSpin(0.0f),
	fHead(0),
	fLimit(0),
	fTween(0.0f),
	fDrawInvisibleCopies(false)
{
	memset(fActors, 0, sizeof(fActors));
	memset(fX, 0, sizeof(fX));
	memset(fY, 0, sizeof(fY));
	memset(fZoom, 0, sizeof(fZoom));
	memset(fArm, 0, sizeof(fArm));
	memset(fWrist, 0, sizeof(fWrist));
	memset(fTwist, 0, sizeof(fTwist));
	memset(fFlip, 0, sizeof(fFlip));
	memset(fSpin, 0, sizeof(fSpin));
	memset(fHue, 0, sizeof(fHue));
	memset(fLight, 0, sizeof(fLight));
	memset(fAlpha, 0, sizeof(fAlpha));
	memset(fAlphaOut, 0, sizeof(fAlphaOut));
	memset(fSize, 0, sizeof(fSize));
	memset(fOutline, 1, sizeof(fOutline));
	memset(fFill, 1, sizeof(fFill));
	memset(fColor, 0, sizeof(fColor));
	memset(fOutlineColor, 0, sizeof(fOutlineColor));

	// init_ep()
	_CreateActors();
	_ReadAnimation(script != NULL ? script : kDefaultScript);

	// srand48(seed)
	fRandomState = ((uint64_t)seed << 16) | 0x330e;

	fLimit = 52;
	fWheel = 0.0f;
}


Engine::~Engine()
{
	while (fSequences != NULL) {
		Sequence* next = fSequences->next;
		_FreeSequence(fSequences);
		fSequences = next;
	}
	_FreeKilledSequences();
	for (int i = 0; i < kActors; i++)
		delete fActors[i];
}


void
Engine::SetDrawInvisibleCopies(bool draw)
{
	fDrawInvisibleCopies = draw;
}


/*static*/ const char*
Engine::DefaultScript()
{
	return kDefaultScript;
}


void
Engine::InitGL()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glShadeModel(GL_FLAT);
	glClear(GL_COLOR_BUFFER_BIT);
}


void
Engine::Reshape(int width, int height)
{
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// gluPerspective(300.0, aspect, 0.01, 10000.0), computed like GLU does.
	// A 300 degree field of view has a negative cotangent: the picture is
	// turned upside down, as in the original.
	const double fovy = 300.0;
	const double aspect = (double)((float)width / (float)height);
	const double zNear = 0.01;
	const double zFar = 10000.0;
	double radians = fovy / 2 * M_PI / 180;
	double deltaZ = zFar - zNear;
	double cotangent = cos(radians) / sin(radians);
	GLdouble m[16] = {
		cotangent / aspect, 0, 0, 0,
		0, cotangent, 0, 0,
		0, 0, -(zFar + zNear) / deltaZ, -1,
		0, 0, -2 * zNear * zFar / deltaZ, 0
	};
	glMultMatrixd(m);

	glTranslatef(0.0f, 0.0f, -4.0f);
	glMatrixMode(GL_MODELVIEW);
}


void
Engine::Display(bool draw)
{
	if (draw) {
		glClear(GL_COLOR_BUFFER_BIT);
		glColor3f(1.0f, 1.0f, 1.0f);
	}

	float tween = fTween + fActors[kActorSpeed]->value;
	int next = (fHead + 1) % kHistory;
	bool advanced = false;
	if (1.0 <= (double)tween) {
		fTween = 0.0f;
		fHead = next;
		advanced = true;
	}

	_TasteQueue();
	_SetActorTargets();
	_AnimateActors();

	int i = fHead;
	fY[i] = 0.0f;
	fX[i] = 0.0f;
	fZoom[i] = fActors[kActorZoom]->value;
	fArm[i] = fActors[kActorArm]->value;
	fWrist[i] = fActors[kActorWrist]->value;
	fDeltaWheel = fActors[kActorWheel]->value;
	fTwist[i] = fActors[kActorTwist]->value;
	fLimit = trunc_w_d(floor((double)fActors[kActorCount]->value));
	fOutline[i] = 0.0 < (double)fActors[kActorOutline]->value ? 1 : 0;

	float hue = fActors[kActorHue]->value;
	fHue[i] = hue;
	if (1.0f <= hue) {
		hue = hue + -1.0f;
		fHue[i] = hue;
	}
	// As in ep.c: this makes every hue 1..2 instead of 0..1, which the
	// colour code wraps back.
	if (hue <= 1.0f)
		fHue[i] = hue + 1.0f;

	fLight[i] = fActors[kActorLight]->value;

	if (advanced) {
		fDeltaFlip = fActors[kActorFlip]->value;
		fDeltaSpin = fActors[kActorSpin]->value;
		fGlobalFlip = fmodf(fGlobalFlip + fDeltaFlip, 360.0f);
		fGlobalSpin = fmodf(fGlobalSpin + fDeltaSpin, 360.0f);
		fFlip[wrap(i)] = fGlobalFlip;
		fSpin[wrap(i)] = fGlobalSpin;
	}

	fWheel = fmodf(fWheel - fDeltaWheel, 360.0f);

	if (draw)
		_DrawIt();

	fTween = fTween + fActors[kActorSpeed]->value;

	if (draw)
		glFinish();
}


// #pragma mark - drawing


void
Engine::_DrawIt()
{
	glColor3f(1.0f, 1.0f, 1.0f);
	glPushMatrix();
	glRotatef(fWheel, 1.0f, 0.0f, 0.0f);

	// Move back by half the length of the tube.
	for (int i = fHead; i > fHead - fLimit; i--)
		glTranslatef(0.0f, 0.0f, twixt(wrap(i), fZoom, fTween) * -0.5f);

	// From the newest square to the oldest.
	for (int i = fHead; i > fHead - fLimit; i--) {
		int k = wrap(i);
		float t = fTween;
		float x = twixt(k, fX, t);
		float y = twixt(k, fY, t);
		float zoom = twixt(k, fZoom, t);
		float arm = twixt(k, fArm, t);
		float wrist = twixt(k, fWrist, t);
		float size = twixt(k, fSize, t);
		float spin = foldtwixt(k, fSpin, t, 360.0f);
		float flip = foldtwixt(k, fFlip, t, 360.0f);
		float twist = twixt(k, fTwist, t);
		float hue = foldtwixt(k, fHue, t, 1.0f);
		float alpha = twixt(k, fAlpha, t);
		float light = foldtwixt(k, fLight, t, 1.0f);
		float alphaOut = twixt(k, fAlphaOut, t);

		hls_to_rgb(hue, light, 1.0f, &fColor[0], &fColor[1], &fColor[2]);
		float outlineHue = hue + 0.5f;
		fOutlineColor[3] = alphaOut;
		fColor[3] = alpha;
		if (1.0 < (double)outlineHue)
			outlineHue = outlineHue + -1.0f;
		hls_to_rgb(outlineHue, 1.0f - light, 1.0f, &fOutlineColor[0],
			&fOutlineColor[1], &fOutlineColor[2]);

		_DrawCopy(k, x, y, spin, arm, flip, wrist, NULL);

		// The three mirrored copies are scaled by fSize, which nothing ever
		// sets: they collapse to a point and are not drawn unless
		// fDrawInvisibleCopies is set. The calls between them still turn
		// the coordinate system for the next square.
		glScalef(1.0f, -1.0f, 1.0f);
		_DrawCopy(k, x, y, spin, arm, flip, wrist, &size);
		glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
		_DrawCopy(k, x, y, spin, arm, flip, wrist, &size);
		glScalef(1.0f, -1.0f, 1.0f);
		_DrawCopy(k, x, y, spin, arm, flip, wrist, &size);
		glRotatef(twist, 0.0f, 0.0f, 1.0f);
		glTranslatef(0.0f, 0.0f, zoom);
	}

	glPopMatrix();
}


void
Engine::_DrawCopy(int index, float x, float y, float spin, float arm,
	float flip, float wrist, const float* scale)
{
	if (scale != NULL && *scale == 0.0f && !fDrawInvisibleCopies)
		return;

	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	glRotatef(spin, 0.0f, 0.0f, 1.0f);
	glTranslatef(0.0f, arm, 0.0f);
	glRotatef(flip, 0.0f, 1.0f, 0.0f);
	glTranslatef(wrist, 0.0f, 0.0f);
	if (scale != NULL)
		glScalef(*scale, *scale, 1.0f);

	if (fFill[index]) {
		glColor3f(fColor[0], fColor[1], fColor[2]);
		_DrawShape(true);
	}
	if (fOutline[index]) {
		glColor3f(fOutlineColor[0], fOutlineColor[1], fOutlineColor[2]);
		_DrawShape(false);
	}
	glPopMatrix();
}


void
Engine::_DrawShape(bool filled)
{
	if (filled) {
		glBegin(GL_POLYGON);
		glVertex2f(kSquare[0], kSquare[1]);
		glVertex2f(kSquare[2], kSquare[3]);
		glVertex2f(kSquare[4], kSquare[5]);
		glVertex2f(kSquare[6], kSquare[7]);
		glEnd();
	} else {
		glBegin(GL_LINES);
		glVertex2f(kSquare[0], kSquare[1]);
		glVertex2f(kSquare[2], kSquare[3]);
		glVertex2f(kSquare[2], kSquare[3]);
		glVertex2f(kSquare[4], kSquare[5]);
		glVertex2f(kSquare[4], kSquare[5]);
		glVertex2f(kSquare[6], kSquare[7]);
		glVertex2f(kSquare[6], kSquare[7]);
		glVertex2f(kSquare[0], kSquare[1]);
		glEnd();
	}
}


// #pragma mark - actors


Engine::Actor*
Engine::_NewActor()
{
	Actor* actor = new Actor;
	memset(actor, 0, sizeof(Actor));
	actor->direction = 1.0f;
	actor->variance = 0.6f;
	return actor;
}


Engine::Actor*
Engine::_ActorAt(int32_t index) const
{
	if (index < 0 || index >= kActors)
		return NULL;
	return fActors[index];
}


void
Engine::_CreateActors()
{
	static const int kIndices[] = {
		kActorOutline, kActorWheel, kActorZoom, kActorTwist, kActorSpin,
		kActorSize, kActorCount, kActorWrist, kActorHueRate, kActorHue,
		kActorFlip, kActorArm, kActorSpeed, kActorLight
	};
	for (size_t i = 0; i < sizeof(kIndices) / sizeof(kIndices[0]); i++)
		fActors[kIndices[i]] = _NewActor();

	fActors[kActorCount]->value = 52.0f;
	fActors[kActorSpeed]->value = 1.0f;
}


void
Engine::_SetActorTargets()
{
	for (int i = 0; i < kActors; i++) {
		Actor* actor = fActors[i];
		if (actor == NULL || !actor->active || actor->moving)
			continue;

		actor->moving = true;
		float r = _Random();
		actor->target = actor->limit1 * r + actor->limit2 * (1.0f - r);

		// As in ep.c, a MAX() macro that evaluates its argument twice: the
		// number of steps is drawn again unless the first draw is below one.
		float steps = actor->duration * (1.0f - actor->variance)
			+ actor->variance * (actor->duration * (_ExpRandom(1.0f) * 2.0f));
		if (steps < 1.0f) {
			steps = 1.0f;
		} else {
			steps = actor->duration * (1.0f - actor->variance)
				+ actor->variance
					* (actor->duration * (_ExpRandom(1.0f) * 2.0f));
		}

		// As in ep.c: from the old step, not from the current value.
		actor->step = (actor->target - actor->step) / steps;
		actor->direction = 0.0f < actor->step ? 1.0f : -1.0f;
	}
}


void
Engine::_AnimateActors()
{
	for (int i = 0; i < kActors; i++) {
		Actor* actor = fActors[i];
		if (actor == NULL || !actor->active || !actor->moving)
			continue;

		float value = actor->step + actor->value;
		if ((double)(actor->direction * (actor->target - value)) < 0.0) {
			value = actor->target;
			actor->moving = false;
		}
		actor->value = value;
	}
}


void
Engine::_StopAnimation()
{
	for (int i = 0; i < kActors; i++) {
		if (fActors[i] != NULL)
			fActors[i]->active = false;
	}
}


// #pragma mark - sequences


Engine::Sequence*
Engine::_NewSequence(int32_t id)
{
	Sequence* sequence = new Sequence;
	memset(sequence, 0, sizeof(Sequence));
	sequence->id = id;
	sequence->active = true;
	sequence->startFrame = 0.0f;
	sequence->duration = 300.0f;
	sequence->variance = 1.0f;
	sequence->next = fSequences;
	fSequences = sequence;
	return sequence;
}


Engine::Sequence*
Engine::_FindSequence(int32_t id) const
{
	Sequence* sequence = fSequences;
	while (sequence != NULL && sequence->id != id)
		sequence = sequence->next;
	return sequence;
}


// Inserts the command after all commands with the same or an earlier frame.
void
Engine::_AddToSequence(Sequence* sequence, Command* command)
{
	command->sequence = sequence;

	Command* first = sequence->first;
	if (first == NULL) {
		sequence->last = command;
		sequence->first = command;
		return;
	}

	float frame = command->frame;
	if (frame < first->frame) {
		command->next = first;
		sequence->first = command;
		return;
	}

	Command* previous = first;
	Command* current = first->next;
	if (current == NULL) {
		previous->next = command;
		sequence->last = command;
		return;
	}

	while (current != NULL) {
		if (frame < current->frame) {
			command->next = current;
			previous->next = command;
			break;
		}
		previous = current;
		current = current->next;
	}

	if (command->next != NULL)
		return;

	sequence->last->next = command;
	sequence->last = command;
}


// ep.c frees the commands of the wrong sequence when it kills the first one
// in the list and frees sequences that _TasteQueue() still walks; the port
// unlinks the sequence and frees it after _TasteQueue() is done.
void
Engine::_KillSequence(Sequence* sequence)
{
	if (sequence == NULL)
		return;

	sequence->active = false;

	if (fSequences == sequence)
		fSequences = sequence->next;
	else {
		Sequence* previous = fSequences;
		while (previous != NULL && previous->next != sequence)
			previous = previous->next;
		if (previous == NULL)
			return;
		previous->next = sequence->next;
	}

	if (fEditSequence == sequence)
		fEditSequence = NULL;

	// sequence->next stays intact for the loop in _TasteQueue().
	sequence->current = NULL;
	sequence->nextKilled = fKilled;
	fKilled = sequence;
}


void
Engine::_FreeKilledSequences()
{
	while (fKilled != NULL) {
		Sequence* next = fKilled->nextKilled;
		_FreeSequence(fKilled);
		fKilled = next;
	}
}


void
Engine::_FreeSequence(Sequence* sequence)
{
	Command* command = sequence->first;
	while (command != NULL) {
		Command* next = command->next;
		delete command;
		command = next;
	}
	delete sequence;
}


void
Engine::_TasteQueue()
{
	fCurrentFrame += 1.0f;

	for (Sequence* sequence = fSequences; sequence != NULL;
			sequence = sequence->next) {
		double now = (double)fCurrentFrame + 0.5;
		if (!((double)sequence->startFrame < now) || !sequence->active)
			continue;

		Command* command = sequence->current;
		if (command == NULL
			|| !((double)(command->frame + sequence->startFrame) < now))
			continue;

		do {
			_ProcessCommand(command);
			if (!sequence->active)
				break;
			command = sequence->current;
			if (command == NULL)
				break;
		} while ((double)(command->frame + sequence->startFrame)
			< (double)fCurrentFrame + 0.5);
	}

	_FreeKilledSequences();
}


void
Engine::_ProcessCommand(Command* command)
{
	Sequence* sequence = command->sequence;

	switch (command->type) {
		case kActLimit1:
		{
			Actor* actor = _ActorAt(command->index);
			if (actor != NULL) {
				float value = command->value;
				if (actor->limit2 < value)
					value = actor->limit2;
				actor->limit2 = value;
			}
			break;
		}

		case kActLimit2:
		{
			Actor* actor = _ActorAt(command->index);
			if (actor == NULL)
				break;

			// As in ep.c: actlim1 and actlim2 keep limit1 >= limit2 with
			// minimum and maximum operations on limits that start at 0.
			float value = command->value;
			float low = actor->limit2;
			float high = actor->limit1;
			if (low < value) {
				actor->limit1 = value < high ? high : value;
			} else {
				actor->limit1 = low < high ? high : low;
				actor->limit2 = low < value ? low : value;
			}
			actor->duration = sequence->duration;
			actor->variance = sequence->variance;
			actor->active = true;
			break;
		}

		case kActSet:
		{
			Actor* actor = _ActorAt(command->index);
			if (actor != NULL)
				actor->value = command->value;
			break;
		}

		case kActReset:
		{
			Actor* actor = _ActorAt(command->index);
			if (actor != NULL)
				actor->value = actor->resetValue;
			break;
		}

		case kActStop:
		{
			Actor* actor = _ActorAt(command->index);
			if (actor != NULL)
				actor->active = false;
			break;
		}

		case kActStopAll:
			_StopAnimation();
			break;

		case kSeqDo:
		{
			Sequence* other = _FindSequence(command->index);
			if (other != NULL) {
				other->current = other->first;
				other->startFrame = fCurrentFrame;
			}
			if (other == sequence)
				return;
			break;
		}

		case kSeqStop:
		{
			Sequence* other = _FindSequence(command->index);
			if (other != NULL)
				other->current = NULL;
			if (other == sequence)
				return;
			break;
		}

		case kSeqLoop:
			sequence->current = sequence->first;
			sequence->startFrame = sequence->startFrame + sequence->length;
			return;

		case kSeqKill:
		{
			// ep.c only handles a sequence killing itself; for any other
			// sequence it neither kills it nor moves on, and hangs.
			Sequence* other = _FindSequence(command->index);
			if (other == sequence) {
				_KillSequence(sequence);
				return;
			}
			_KillSequence(other);
			break;
		}

		case kSeqKillAll:
		{
			// ep.c kills one sequence and then moves on twice; the port
			// kills all other sequences.
			Sequence* other = fSequences;
			while (other != NULL) {
				Sequence* next = other->next;
				if (other != sequence)
					_KillSequence(other);
				other = next;
			}
			break;
		}

		case kDuration:
		{
			double duration = command->value;
			if (duration < 1.0)
				duration = 1.0;
			sequence->duration = duration;
			break;
		}

		case kVariance:
		{
			double variance = command->value;
			if (variance < 0.0)
				variance = 0.0;
			else if (1.0 < variance)
				variance = 1.0;
			sequence->variance = variance;
			break;
		}

		case kRandDelay:
		{
			float random = _ExpRandom(1.0f);
			float delay = command->value;
			float variance = sequence->variance;
			sequence->startFrame = (double)sequence->startFrame
				+ ((double)(delay * (1.0f - variance))
					+ (double)variance * ((double)delay * ((double)random * 2.0)));
			break;
		}

		case kFrame:
		case kAbsFrame:
		case kActResetAll:
			// As in ep.c, "actresetall:" does nothing.
		case kSeqName:
		case kSeqRepeat:
			break;

		default:
			fprintf(stderr, "warning: bad command type (%d) in sequence "
				"cmd->seq\n", (int)command->type);
			break;
	}

	sequence->current = sequence->current->next;
}


// #pragma mark - script


void
Engine::_ReadAnimation(const char* script)
{
	const char* s = script;
	char word[256] = "";

	while (*s != '\0') {
		sscanf(s, "%255s", word);
		s += strcspn(s, " \t\n");

		Command* command = new Command;
		memset(command, 0, sizeof(Command));
		command->frame = fAbsFrame;

		bool add = true;
		if (strcmp("frame:", word) == 0 || strcmp("relframe:", word) == 0) {
			command->type = kFrame;
			sscanf(s, "%f", &command->value);
			s += strcspn(s, " \t\n");
			fRelFrame = fRelFrame + command->value;
			fAbsFrame = fBaseFrame + fRelFrame;
		} else if (strcmp("absframe:", word) == 0) {
			command->type = kAbsFrame;
			sscanf(s, "%f", &command->value);
			s += strcspn(s, " \t\n");
			fRelFrame = 0.0f;
			fBaseFrame = command->value;
			fAbsFrame = command->value;
		} else if (strcmp("randdelay:", word) == 0
			|| strcmp("duration:", word) == 0
			|| strcmp("variance:", word) == 0) {
			command->type = word[0] == 'r' ? kRandDelay
				: word[0] == 'd' ? kDuration : kVariance;
			sscanf(s, "%f", &command->value);
			s += strcspn(s, " \t\n");
		} else if (strcmp("actlim1:", word) == 0
			|| strcmp("actlim2:", word) == 0
			|| strcmp("actset:", word) == 0) {
			command->type = word[3] == 's' ? kActSet
				: word[6] == '1' ? kActLimit1 : kActLimit2;
			int index = 0;
			sscanf(s, "%d, %f", &index, &command->value);
			command->index = index;
			s += strcspn(s, " \t\n");
		} else if (strcmp("actreset:", word) == 0
			|| strcmp("actstop:", word) == 0
			|| strcmp("seqdo:", word) == 0
			|| strcmp("seqstop:", word) == 0
			|| strcmp("seqkill:", word) == 0) {
			command->type = strcmp("actreset:", word) == 0 ? kActReset
				: strcmp("actstop:", word) == 0 ? kActStop
				: strcmp("seqdo:", word) == 0 ? kSeqDo
				: strcmp("seqstop:", word) == 0 ? kSeqStop : kSeqKill;
			int index = 0;
			sscanf(s, "%d", &index);
			command->index = index;
			s += strcspn(s, " \t\n");
		} else if (strcmp("actresetall:", word) == 0) {
			command->type = kActResetAll;
		} else if (strcmp("actstopall:", word) == 0) {
			command->type = kActStopAll;
		} else if (strcmp("seqloop:", word) == 0) {
			command->type = kSeqLoop;
		} else if (strcmp("seqkillall:", word) == 0) {
			command->type = kSeqKillAll;
		} else if (strcmp("seqname:", word) == 0) {
			int id = 0;
			sscanf(s, "%d", &id);
			s += strcspn(s, " \t\n");
			fBaseFrame = 0.0f;
			fAbsFrame = 0.0f;
			fRelFrame = 0.0f;
			fEditSequence = _FindSequence(id);
			if (fEditSequence == NULL)
				fEditSequence = _NewSequence(id);
			add = false;
		} else if (strcmp("seqrepeat:", word) == 0) {
			// ep.c: "warning: seqrepeat: not implemented"
			s += strcspn(s, " \t\n");
			add = false;
		} else {
			// A comment ("#...") or a bad command.
			add = false;
		}

		if (add) {
			if (fEditSequence == NULL)
				fEditSequence = _NewSequence(0);
			_AddToSequence(fEditSequence, command);
			fEditSequence->length = fRelFrame;
		} else
			delete command;

		// Next line. ep.c steps over the terminating null of a script that
		// does not end in a newline and reads on behind it.
		s += strcspn(s, "\n");
		if (*s == '\0')
			break;
		s++;
	}

	// Only sequence 0 runs from the start, the others wait for "seqdo:".
	for (Sequence* sequence = fSequences; sequence != NULL;
			sequence = sequence->next) {
		if (sequence->id == 0) {
			sequence->current = sequence->first;
			break;
		}
	}
}


// #pragma mark - random numbers


// drand48()
double
Engine::_Random()
{
	fRandomState = (fRandomState * 0x5deece66dULL + 0xb) & 0xffffffffffffULL;
	return ldexp((double)fRandomState, -48);
}


// Random numbers between 0 and (e^m - 1) / (e - 1), exponentially
// distributed.
float
Engine::_ExpRandom(float m)
{
	double scaled = expm1(_Random() * (double)m);
	return scaled / expm1(1.0);
}


}	// namespace ElectroPaint

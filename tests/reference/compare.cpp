/*
 * Runs ElectroPortis' decompiled ep.c and the Haiku port's engine side by
 * side with the same random seed and compares their OpenGL calls, frame by
 * frame and bit for bit.
 *
 * usage: epcompare [--seed N] [--frames N] [--skip K] [--script FILE]
 *                  [--min-colors N]
 *   --skip K       the engine draws only every K-th frame (Display(false) in
 *                  between); the drawn frames must still match
 *   --min-colors N fail unless at least N different fill colours were drawn
 *                  (guards against both sides doing nothing)
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <utility>

#include "ElectroPaintEngine.h"
#include "glrecord.h"


extern "C" {
// ep_ref.c (ep.c as patched by patch-ep.py) and lowmem.c
void init_ep(void);
void display__Gv(void* args, unsigned long wincount);
void reshape__GiT1(unsigned int width, unsigned int height);
extern long ep_ref_seed;
const char* ep_ref_default_script(void);
void ep_ref_set_script(char* script);
char* lowstrdup(const char* string);
}


namespace {


bool
compare_traces(const char* what, long frame, const GLTrace& reference,
	const GLTrace& engine)
{
	size_t count = reference.size() > engine.size()
		? reference.size() : engine.size();
	for (size_t i = 0; i < count; i++) {
		if (i < reference.size() && i < engine.size()
			&& reference[i] == engine[i])
			continue;

		fprintf(stderr, "MISMATCH in %s, frame %ld, call %zu of %zu/%zu:\n",
			what, frame, i, reference.size(), engine.size());
		size_t start = i >= 4 ? i - 4 : 0;
		for (size_t j = start; j < i + 3 && j < count; j++) {
			fprintf(stderr, "  %s%5zu  ep.c:   %s\n", j == i ? ">" : " ", j,
				j < reference.size() ? reference[j].ToString().c_str() : "-");
			fprintf(stderr, "  %s%5zu  engine: %s\n", j == i ? ">" : " ", j,
				j < engine.size() ? engine[j].ToString().c_str() : "-");
		}
		return false;
	}
	return true;
}


std::string
read_file(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "epcompare: %s: %s\n", path, strerror(errno));
		exit(2);
	}
	std::string text;
	char buffer[4096];
	size_t bytes;
	while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
		text.append(buffer, bytes);
	fclose(file);
	return text;
}


}	// namespace


int
main(int argc, char** argv)
{
	long seed = 12345;
	long frames = 3000;
	long skip = 1;
	long minColors = 0;
	const char* scriptPath = NULL;

	for (int i = 1; i < argc; i++) {
		if (i + 1 < argc && strcmp(argv[i], "--seed") == 0)
			seed = strtol(argv[++i], NULL, 0);
		else if (i + 1 < argc && strcmp(argv[i], "--frames") == 0)
			frames = strtol(argv[++i], NULL, 0);
		else if (i + 1 < argc && strcmp(argv[i], "--skip") == 0)
			skip = strtol(argv[++i], NULL, 0);
		else if (i + 1 < argc && strcmp(argv[i], "--script") == 0)
			scriptPath = argv[++i];
		else if (i + 1 < argc && strcmp(argv[i], "--min-colors") == 0)
			minColors = strtol(argv[++i], NULL, 0);
		else {
			fprintf(stderr, "usage: %s [--seed N] [--frames N] [--skip K] "
				"[--script FILE] [--min-colors N]\n", argv[0]);
			return 2;
		}
	}
	if (skip < 1)
		skip = 1;

	std::string script;
	if (scriptPath != NULL)
		script = read_file(scriptPath);
	else {
		script = ElectroPaint::Engine::DefaultScript();
		if (strcmp(ep_ref_default_script(), script.c_str()) != 0) {
			fprintf(stderr, "MISMATCH: the built-in script differs from "
				"ep.c's defaultScript\n");
			return 1;
		}
	}

	ep_ref_set_script(lowstrdup(script.c_str()));
	ep_ref_seed = seed;
	init_ep();
	ElectroPaint::Engine engine((uint32_t)seed, script.c_str());
	engine.SetDrawInvisibleCopies(true);

	// The projection for a few window sizes.
	static const int kSizes[][2] = {
		{ 1920, 1080 }, { 1024, 768 }, { 800, 600 }, { 130, 97 }, { 1, 1 }
	};
	for (size_t i = 0; i < sizeof(kSizes) / sizeof(kSizes[0]); i++) {
		gTrace.clear();
		reshape__GiT1(kSizes[i][0], kSizes[i][1]);
		GLTrace reference = gTrace;
		gTrace.clear();
		engine.Reshape(kSizes[i][0], kSizes[i][1]);
		if (!compare_traces("reshape", (long)i, reference, gTrace))
			return 1;
	}

	uint64_t args[64];
	memset(args, 0, sizeof(args));
	unsigned long calls = 0;
	long drawn = 0;
	std::set<std::pair<uint64_t, uint64_t> > colors;

	for (long frame = 0; frame < frames; frame++) {
		bool draw = frame % skip == skip - 1;

		gTrace.clear();
		display__Gv(args, 1);
		GLTrace reference = gTrace;

		gTrace.clear();
		engine.Display(draw);

		if (!draw) {
			if (!gTrace.empty()) {
				fprintf(stderr, "MISMATCH: Display(false) drew %zu calls in "
					"frame %ld\n", gTrace.size(), frame);
				return 1;
			}
			continue;
		}
		if (!compare_traces("display", frame, reference, gTrace))
			return 1;

		drawn++;
		calls += gTrace.size();
		// Fill colours: glColor3f right before glBegin(GL_POLYGON).
		for (size_t i = 0; i + 1 < gTrace.size(); i++) {
			if (gTrace[i].count == 3 && gTrace[i + 1].count == 1
				&& gTrace[i + 1].args[0] == 9 /* GL_POLYGON */) {
				colors.insert(std::make_pair(gTrace[i].args[0],
					(gTrace[i].args[1] << 32) | gTrace[i].args[2]));
			}
		}
	}

	printf("seed %ld: %ld frames (%ld drawn), %lu GL calls identical, "
		"%zu fill colours\n", seed, frames, drawn, calls, colors.size());

	if ((long)colors.size() < minColors) {
		fprintf(stderr, "FAIL: only %zu fill colours, expected at least %ld\n",
			colors.size(), minColors);
		return 1;
	}
	return 0;
}

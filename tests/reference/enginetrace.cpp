/*
 * Prints a hash of the engine's OpenGL calls every 600 frames. Built on
 * Linux (where epcompare checks the engine against ep.c) and on Haiku, the
 * two outputs must be identical: Haiku's compiler and math library then
 * produce the same animation as the verified Linux build.
 *
 * usage: enginetrace [--seed N] [--frames N]
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ElectroPaintEngine.h"
#include "glrecord.h"


int
main(int argc, char** argv)
{
	long seed = 2026;
	long frames = 36000;
	for (int i = 1; i + 1 < argc; i += 2) {
		if (strcmp(argv[i], "--seed") == 0)
			seed = strtol(argv[i + 1], NULL, 0);
		else if (strcmp(argv[i], "--frames") == 0)
			frames = strtol(argv[i + 1], NULL, 0);
	}

	ElectroPaint::Engine engine((uint32_t)seed);
	engine.SetDrawInvisibleCopies(true);
	engine.InitGL();
	engine.Reshape(1024, 768);

	uint64_t hash = 0xcbf29ce484222325ULL;
	unsigned long calls = 0;
	for (long frame = 1; frame <= frames; frame++) {
		gTrace.clear();
		engine.Display();
		for (size_t i = 0; i < gTrace.size(); i++) {
			const GLCall& call = gTrace[i];
			uint64_t words[2] = { (uint64_t)call.op, (uint64_t)call.count };
			const unsigned char* bytes = (const unsigned char*)words;
			for (size_t j = 0; j < sizeof(words); j++)
				hash = (hash ^ bytes[j]) * 0x100000001b3ULL;
			bytes = (const unsigned char*)call.args;
			for (size_t j = 0; j < sizeof(call.args[0]) * call.count; j++)
				hash = (hash ^ bytes[j]) * 0x100000001b3ULL;
		}
		calls += gTrace.size();
		if (frame % 600 == 0) {
			printf("seed %ld frame %ld calls %lu hash %016llx\n", seed, frame,
				calls, (unsigned long long)hash);
		}
	}
	return 0;
}

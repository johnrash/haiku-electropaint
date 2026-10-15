/*
 * Renders the animation off screen with Haiku's Mesa (OSMesa) twice, once
 * with all of the original's calls and once without the copies that are
 * scaled to nothing (the screen saver's default), and checks that every
 * pixel of every frame is the same.
 *
 * usage: pixelcompare [--seed N] [--frames N] [--size WIDTH HEIGHT]
 * Build: g++ -Isrc tests/haiku/pixelcompare.cpp src/ElectroPaintEngine.cpp
 *        -lOSMesa
 */
#include <GL/osmesa.h>

#include <OS.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "ElectroPaintEngine.h"


int
main(int argc, char** argv)
{
	long seed = 2026;
	long frames = 1800;
	int width = 640;
	int height = 480;
	for (int i = 1; i < argc; i++) {
		if (i + 1 < argc && strcmp(argv[i], "--seed") == 0)
			seed = strtol(argv[++i], NULL, 0);
		else if (i + 1 < argc && strcmp(argv[i], "--frames") == 0)
			frames = strtol(argv[++i], NULL, 0);
		else if (i + 2 < argc && strcmp(argv[i], "--size") == 0) {
			width = atoi(argv[++i]);
			height = atoi(argv[++i]);
		} else {
			fprintf(stderr, "usage: %s [--seed N] [--frames N] "
				"[--size WIDTH HEIGHT]\n", argv[0]);
			return 2;
		}
	}

	OSMesaContext exactContext = OSMesaCreateContextExt(OSMESA_RGBA, 0, 0, 0,
		NULL);
	OSMesaContext fastContext = OSMesaCreateContextExt(OSMESA_RGBA, 0, 0, 0,
		NULL);
	if (exactContext == NULL || fastContext == NULL) {
		fprintf(stderr, "pixelcompare: no OSMesa context\n");
		return 1;
	}
	std::vector<uint8_t> exactPixels(width * height * 4);
	std::vector<uint8_t> fastPixels(width * height * 4);

	ElectroPaint::Engine exact((uint32_t)seed);
	exact.SetDrawInvisibleCopies(true);
	ElectroPaint::Engine fast((uint32_t)seed);

	OSMesaMakeCurrent(exactContext, exactPixels.data(), GL_UNSIGNED_BYTE,
		width, height);
	exact.InitGL();
	exact.Reshape(width, height);
	OSMesaMakeCurrent(fastContext, fastPixels.data(), GL_UNSIGNED_BYTE,
		width, height);
	fast.InitGL();
	fast.Reshape(width, height);

	bigtime_t exactTime = 0;
	bigtime_t fastTime = 0;
	long litPixels = 0;
	for (long frame = 0; frame < frames; frame++) {
		bigtime_t start = system_time();
		OSMesaMakeCurrent(exactContext, exactPixels.data(), GL_UNSIGNED_BYTE,
			width, height);
		exact.Display();
		bigtime_t middle = system_time();
		OSMesaMakeCurrent(fastContext, fastPixels.data(), GL_UNSIGNED_BYTE,
			width, height);
		fast.Display();
		bigtime_t end = system_time();
		exactTime += middle - start;
		fastTime += end - middle;

		long differences = 0;
		for (size_t i = 0; i < exactPixels.size(); i += 4) {
			if (memcmp(&exactPixels[i], &fastPixels[i], 3) != 0)
				differences++;
			else if (exactPixels[i] | exactPixels[i + 1] | exactPixels[i + 2])
				litPixels++;
		}
		if (differences != 0) {
			fprintf(stderr, "MISMATCH: %ld pixels differ in frame %ld\n",
				differences, frame);
			return 1;
		}
	}

	printf("%dx%d, seed %ld: %ld frames pixel identical, %.1f lit pixels "
		"per frame; %.2f ms per frame with all copies, %.2f ms without\n",
		width, height, seed, frames, (double)litPixels / frames,
		exactTime / 1000.0 / frames, fastTime / 1000.0 / frames);

	OSMesaDestroyContext(exactContext);
	OSMesaDestroyContext(fastContext);
	return litPixels > 0 ? 0 : 1;
}

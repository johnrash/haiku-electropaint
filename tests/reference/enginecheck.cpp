/*
 * Runs the engine alone (built with AddressSanitizer and UBSan by
 * run-tests.sh) with the built-in script and the test scripts, including the
 * seqkill/seqkillall script that ep.c cannot run, and destroys it again.
 *
 * usage: enginecheck [--frames N] [SCRIPT...]
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "ElectroPaintEngine.h"
#include "glrecord.h"


static bool
run(const char* name, const char* script, long frames, bool exact)
{
	ElectroPaint::Engine engine(4711, script);
	engine.SetDrawInvisibleCopies(exact);
	engine.InitGL();
	engine.Reshape(640, 480);
	size_t calls = 0;
	for (long frame = 0; frame < frames; frame++) {
		gTrace.clear();
		engine.Display(frame % 5 != 1);
		calls += gTrace.size();
	}
	printf("%s%s: %ld frames, %zu GL calls\n", name,
		exact ? " (all copies)" : "", frames, calls);
	return calls > 0;
}


int
main(int argc, char** argv)
{
	long frames = 3000;
	int first = 1;
	if (argc > 2 && strcmp(argv[1], "--frames") == 0) {
		frames = strtol(argv[2], NULL, 0);
		first = 3;
	}

	bool ok = run("built-in script", NULL, frames, false)
		&& run("built-in script", NULL, frames, true);
	for (int i = first; i < argc; i++) {
		FILE* file = fopen(argv[i], "rb");
		if (file == NULL) {
			fprintf(stderr, "enginecheck: %s: %s\n", argv[i], strerror(errno));
			return 2;
		}
		std::string script;
		char buffer[4096];
		size_t bytes;
		while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
			script.append(buffer, bytes);
		fclose(file);
		ok = run(argv[i], script.c_str(), frames, false) && ok;
		ok = run(argv[i], script.c_str(), frames, true) && ok;
	}
	return ok ? 0 : 1;
}

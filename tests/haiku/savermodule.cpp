/*
 * Reads or changes ScreenSaver_settings, keeping every other setting.
 *
 * usage: savermodule                  prints the module name ("" if none)
 *        savermodule [--tab N] [--timeflags N] NAME
 *            selects module NAME; --tab 1 opens the preferences with the
 *            module list and preview, --timeflags 0 keeps input_server from
 *            starting the screen saver while a test runs
 */
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void
set_int32(BMessage& settings, const char* name, const char* value)
{
	int32 number = strtol(value, NULL, 0);
	if (settings.ReplaceInt32(name, number) != B_OK)
		settings.AddInt32(name, number);
}


int
main(int argc, char** argv)
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK
		|| path.Append("ScreenSaver_settings") != B_OK) {
		fprintf(stderr, "savermodule: no settings directory\n");
		return 1;
	}

	BMessage settings;
	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() == B_OK)
		settings.Unflatten(&file);
	file.Unset();

	if (argc < 2) {
		const char* name;
		if (settings.FindString("modulename", &name) != B_OK)
			name = "";
		printf("%s\n", name);
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		if (i + 1 < argc && strcmp(argv[i], "--tab") == 0)
			set_int32(settings, "windowtab", argv[++i]);
		else if (i + 1 < argc && strcmp(argv[i], "--timeflags") == 0)
			set_int32(settings, "timeflags", argv[++i]);
		else if (settings.ReplaceString("modulename", argv[i]) != B_OK)
			settings.AddString("modulename", argv[i]);
	}

	status_t status = file.SetTo(path.Path(),
		B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (status == B_OK)
		status = settings.Flatten(&file);
	if (status != B_OK) {
		fprintf(stderr, "savermodule: %s: %s\n", path.Path(),
			strerror(status));
		return 1;
	}
	return 0;
}

/*
 * Runs a screen saver add-on in an ordinary window the way Haiku's
 * ScreenSaverRunner does (a low priority thread that calls Draw() every tick
 * with the window locked), and reports how often Draw() really drew.
 *
 * usage: saverhost ADD-ON [SECONDS [WIDTH HEIGHT]]
 * Prints "drawn per second" from the Draw() calls that took longer than a
 * quick "nothing due yet" return.
 */
#include <Application.h>
#include <ScreenSaver.h>
#include <View.h>
#include <Window.h>

#include <image.h>
#include <OS.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const bigtime_t kDrawnThreshold = 200;	// microseconds


struct HostState {
	BScreenSaver*	saver;
	BWindow*		window;
	BView*			view;
	bigtime_t		duration;
	int				result;
};


static status_t
run_saver(void* data)
{
	HostState* state = (HostState*)data;

	// ScreenSaverRunner::_Run(), simplified.
	bigtime_t tickBase = 50000;
	bigtime_t tick = state->saver->TickSize();
	if (tick < tickBase)
		tickBase = tick;

	int64 calls = 0;
	int64 drawn = 0;
	bigtime_t drawnTime = 0;
	bigtime_t longest = 0;
	bigtime_t lastTick = 0;
	int32 frame = 0;
	bigtime_t start = system_time();
	bigtime_t warmUp = start + 1000000;
	int64 drawnAfterWarmUp = 0;

	while (system_time() - start < state->duration) {
		snooze(tickBase);
		if (system_time() - lastTick < tick)
			continue;
		lastTick = system_time();

		if (state->window->LockWithTimeout(5000) != B_OK)
			continue;
		bigtime_t before = system_time();
		state->saver->Draw(state->view, frame++);
		bigtime_t elapsed = system_time() - before;
		state->view->Sync();
		state->window->Unlock();

		calls++;
		if (elapsed >= kDrawnThreshold) {
			drawn++;
			drawnTime += elapsed;
			if (elapsed > longest)
				longest = elapsed;
			if (before >= warmUp)
				drawnAfterWarmUp++;
		}
	}

	double seconds = (system_time() - warmUp) / 1000000.0;
	double perSecond = drawnAfterWarmUp / seconds;
	printf("tick %lld us, %lld Draw() calls, %lld drew, %.1f drawn per "
		"second, %.2f ms average, %.2f ms longest\n", (long long)tick,
		(long long)calls, (long long)drawn, perSecond,
		drawn > 0 ? drawnTime / 1000.0 / drawn : 0.0, longest / 1000.0);

	state->result = perSecond > 0 ? 0 : 1;

	// Like the runner: stop the saver before its window goes away.
	if (state->window->Lock()) {
		state->saver->StopSaver();
		state->window->Unlock();
	}
	be_app->PostMessage(B_QUIT_REQUESTED);
	return B_OK;
}


int
main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s ADD-ON [SECONDS [WIDTH HEIGHT]]\n",
			argv[0]);
		return 2;
	}
	double seconds = argc > 2 ? atof(argv[2]) : 10;
	int width = argc > 4 ? atoi(argv[3]) : 640;
	int height = argc > 4 ? atoi(argv[4]) : 480;

	BApplication app("application/x-vnd.ElectroPaint-SaverHost");

	image_id image = load_add_on(argv[1]);
	if (image < 0) {
		fprintf(stderr, "saverhost: %s: %s\n", argv[1], strerror(image));
		return 1;
	}
	BScreenSaver* (*instantiate)(BMessage*, image_id);
	if (get_image_symbol(image, "instantiate_screen_saver", B_SYMBOL_TYPE_TEXT,
			(void**)&instantiate) != B_OK) {
		fprintf(stderr, "saverhost: no instantiate_screen_saver()\n");
		return 1;
	}

	BMessage archive;
	HostState state;
	state.saver = instantiate(&archive, image);
	if (state.saver == NULL || state.saver->InitCheck() != B_OK) {
		fprintf(stderr, "saverhost: the add-on failed to start\n");
		return 1;
	}
	state.duration = (bigtime_t)(seconds * 1000000);
	state.result = 1;

	state.window = new BWindow(BRect(80, 80, 80 + width - 1, 80 + height - 1),
		"ElectroPaint test", B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE);
	state.view = new BView(state.window->Bounds(), "saver", B_FOLLOW_ALL,
		B_WILL_DRAW);
	state.view->SetViewColor(0, 0, 0);
	state.window->AddChild(state.view);
	state.window->Show();

	state.window->Lock();
	status_t status = state.saver->StartSaver(state.view, false);
	state.window->Unlock();
	if (status != B_OK) {
		fprintf(stderr, "saverhost: StartSaver(): %s\n", strerror(status));
		return 1;
	}

	thread_id thread = spawn_thread(run_saver, "ScreenSaverRenderer",
		B_LOW_PRIORITY, &state);
	resume_thread(thread);
	app.Run();
	status_t exitValue;
	wait_for_thread(thread, &exitValue);

	// The application closed the window when it quit.
	delete state.saver;
	unload_add_on(image);
	return state.result;
}

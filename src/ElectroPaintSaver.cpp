/*
 * Copyright 2026, the ElectroPaint for Haiku authors.
 * Distributed under the terms of the MIT License.
 *
 * Screen saver add-on around the ElectroPaint engine.
 */


#include <GLView.h>
#include <LayoutBuilder.h>
#include <ScreenSaver.h>
#include <StringView.h>

#include <new>
#include <time.h>

#include "ElectroPaintEngine.h"


namespace {


// The runner calls Draw() at most this often; the view keeps the animation
// at 60 frames per second itself, like the original on an SGI monitor.
const bigtime_t kTickSize = 4000;
const int64 kFramesPerSecond = 60;
// A machine too slow to draw every frame skips at most this many frames in a
// row; when it falls further behind the animation slows down instead.
const int64 kMaxFramesPerDraw = 4;


}	// namespace


class ElectroPaintView : public BGLView {
public:
								ElectroPaintView(BRect frame);

	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);

			void				Animate();

private:
			ElectroPaint::Engine fEngine;
			bigtime_t			fStartTime;
			int64				fFrames;
};


class ElectroPaintSaver : public BScreenSaver {
public:
								ElectroPaintSaver(BMessage* archive,
									image_id image);

	virtual	void				StartConfig(BView* view);
	virtual	status_t			StartSaver(BView* view, bool preview);
	virtual	void				StopSaver();
	virtual	void				Draw(BView* view, int32 frame);
	virtual	void				DirectConnected(direct_buffer_info* info);

private:
			ElectroPaintView*	fView;
};


// #pragma mark - ElectroPaintView


ElectroPaintView::ElectroPaintView(BRect frame)
	:
	BGLView(frame, "ElectroPaint", B_FOLLOW_ALL_SIDES,
		B_WILL_DRAW | B_FRAME_EVENTS, BGL_RGB | BGL_DOUBLE),
	fEngine((uint32_t)time(NULL)),
	fStartTime(-1),
	fFrames(0)
{
}


void
ElectroPaintView::AttachedToWindow()
{
	LockGL();
	BGLView::AttachedToWindow();
	fEngine.InitGL();
	fEngine.Reshape(Bounds().IntegerWidth() + 1,
		Bounds().IntegerHeight() + 1);
	UnlockGL();
}


void
ElectroPaintView::FrameResized(float width, float height)
{
	LockGL();
	BGLView::FrameResized(width, height);
	fEngine.Reshape((int)width + 1, (int)height + 1);
	UnlockGL();
}


void
ElectroPaintView::Animate()
{
	bigtime_t now = system_time();
	if (fStartTime < 0)
		fStartTime = now;

	// Frame k is due k / 60 seconds after the start.
	int64 due = (now - fStartTime) * kFramesPerSecond / 1000000 + 1
		- fFrames;
	if (due <= 0)
		return;
	if (due > kMaxFramesPerDraw) {
		fStartTime = now
			- (fFrames * 1000000 + kFramesPerSecond - 1) / kFramesPerSecond;
		due = 1;
	}

	LockGL();
	// The skipped frames still advance the animation.
	for (int64 i = 1; i <= due; i++)
		fEngine.Display(i == due);
	SwapBuffers();
	UnlockGL();

	fFrames += due;
}


// #pragma mark - ElectroPaintSaver


ElectroPaintSaver::ElectroPaintSaver(BMessage* archive, image_id image)
	:
	BScreenSaver(archive, image),
	fView(NULL)
{
}


void
ElectroPaintSaver::StartConfig(BView* view)
{
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BStringView* name = new BStringView("name", "ElectroPaint");
	name->SetFont(be_bold_font);

	BLayoutBuilder::Group<>(view, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(name)
		.Add(new BStringView("author", "by David Tristram"))
		.Add(new BStringView("decompiled",
			"Decompiled from the IRIX binary by Mark Laws"))
		.Add(new BStringView("options", "ElectroPaint has no options."))
		.AddGlue()
		.SetInsets(B_USE_DEFAULT_SPACING)
		.End();
}


status_t
ElectroPaintSaver::StartSaver(BView* view, bool preview)
{
	SetTickSize(kTickSize);

	fView = new(std::nothrow) ElectroPaintView(view->Bounds());
	if (fView == NULL)
		return B_NO_MEMORY;

	view->AddChild(fView);
	return B_OK;
}


void
ElectroPaintSaver::StopSaver()
{
	if (fView != NULL)
		fView->EnableDirectMode(false);
}


void
ElectroPaintSaver::Draw(BView* view, int32 frame)
{
	if (fView != NULL)
		fView->Animate();
}


void
ElectroPaintSaver::DirectConnected(direct_buffer_info* info)
{
	if (fView != NULL) {
		fView->DirectConnected(info);
		fView->EnableDirectMode(true);
	}
}


extern "C" _EXPORT BScreenSaver*
instantiate_screen_saver(BMessage* archive, image_id image)
{
	return new ElectroPaintSaver(archive, image);
}

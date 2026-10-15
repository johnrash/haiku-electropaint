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
 * The engine is a hand-written, pointer-size independent rewrite of ep.c from
 * ElectroPortis (https://github.com/drvink/electroportis), Mark Laws' machine
 * decompilation of the IRIX binary. It issues the same OpenGL calls with the
 * same arguments, bit for bit; tests/reference checks that against ep.c.
 */
#ifndef ELECTRO_PAINT_ENGINE_H
#define ELECTRO_PAINT_ENGINE_H


#include <stddef.h>
#include <stdint.h>


namespace ElectroPaint {


class Engine {
public:
	// script: an animation script in ElectroPaint's own language, NULL for
	// the built-in one ("mello script" by Dave Tristram).
								Engine(uint32_t seed,
									const char* script = NULL);
								~Engine();

	// GL state the original sets once after creating its context.
			void				InitGL();
	// The projection, for a drawable of the given size in pixels.
			void				Reshape(int width, int height);

	// One frame of the original's display(): advance the animation and
	// draw it into the current GL context. With draw false the animation
	// advances exactly the same way but nothing is drawn, so a slow machine
	// can skip frames without changing the animation's course.
			void				Display(bool draw = true);

	// The original draws three mirrored copies of every square, scaled by a
	// size that is always 0: they never produce a pixel, but take most of
	// the drawing time. The engine leaves them out unless asked to repeat
	// the original's calls exactly (as tests/reference does).
			void				SetDrawInvisibleCopies(bool draw);

	static	const char*			DefaultScript();

private:
	struct Actor;
	struct Command;
	struct Sequence;

	enum {
		kHistory = 128,
		kActors = 1024
	};

			Actor*				_NewActor();
			Actor*				_ActorAt(int32_t index) const;
			void				_CreateActors();
			void				_ReadAnimation(const char* script);
			Sequence*			_NewSequence(int32_t id);
			Sequence*			_FindSequence(int32_t id) const;
			void				_AddToSequence(Sequence* sequence,
									Command* command);
			void				_KillSequence(Sequence* sequence);
			void				_FreeKilledSequences();
			void				_FreeSequence(Sequence* sequence);

			void				_TasteQueue();
			void				_ProcessCommand(Command* command);
			void				_SetActorTargets();
			void				_AnimateActors();
			void				_StopAnimation();

			void				_DrawIt();
			void				_DrawCopy(int index, float x, float y,
									float spin, float arm, float flip,
									float wrist, const float* scale);
			void				_DrawShape(bool filled);

			double				_Random();
			float				_ExpRandom(float m);

private:
			Actor*				fActors[kActors];
			Sequence*			fSequences;
			Sequence*			fEditSequence;
			Sequence*			fKilled;

			float				fCurrentFrame;
			float				fRelFrame;
			float				fBaseFrame;
			float				fAbsFrame;

			float				fWheel;
			float				fDeltaWheel;
			float				fGlobalFlip;
			float				fDeltaFlip;
			float				fGlobalSpin;
			float				fDeltaSpin;
			int					fHead;		// "n": newest history slot
			int					fLimit;		// "nlimit": slots drawn
			float				fTween;		// "t"

			// Per-slot history of the animated parameters.
			float				fX[kHistory];
			float				fY[kHistory];
			float				fZoom[kHistory];
			float				fArm[kHistory];
			float				fWrist[kHistory];
			float				fTwist[kHistory];
			float				fFlip[kHistory];
			float				fSpin[kHistory];
			float				fHue[kHistory];
			float				fLight[kHistory];
			float				fAlpha[kHistory];
			float				fAlphaOut[kHistory];
			float				fSize[kHistory];
			char				fOutline[kHistory];
			char				fFill[kHistory];

			float				fColor[4];
			float				fOutlineColor[4];

			uint64_t			fRandomState;
			bool				fDrawInvisibleCopies;
};


}	// namespace ElectroPaint


#endif	// ELECTRO_PAINT_ENGINE_H

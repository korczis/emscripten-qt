#ifdef EMSCRIPTEN_NATIVE
#include "emscripten-canvas-sdl.h"
#include <QtCore/QDebug>
#include <SDL/SDL.h>

namespace
{
	int canvasWidthPixels = 800;
	int canvasHeightPixels = 640;
	SDL_Surface *sdlCanvas = NULL;
	SDL_TimerID callbackTimer = NULL;
	bool sdlInited = false;
}

extern "C" 
{
	void EMSCRIPTENQT_mouseCanvasPosChanged(int x, int y); 
	void EMSCRIPTENQT_mouseCanvasButtonChanged(int button, int state);
	void EMSCRIPTENQT_canvasKeyChanged(int unicode, int keycode, int modifiers, int isPress, int autoRepeat);


	void setpixel(SDL_Surface *screen, int x, int y, Uint8 r, Uint8 g, Uint8 b)
	{
		Uint32 *pixmem32;
		Uint32 colour;  

		colour = SDL_MapRGB( screen->format, r, g, b );

		pixmem32 = (Uint32*) screen->pixels  + y + x;
		*pixmem32 = colour;
	}

	Uint32 timerCallback(Uint32 interval, void *param)
	{
		qDebug() << "timerCallback";
		SDL_Event event;
		SDL_UserEvent userevent;

		userevent.type = SDL_USEREVENT;
		userevent.code = 0;
		userevent.data1 = NULL;
		userevent.data2 = NULL;

		event.type = SDL_USEREVENT;
		event.user = userevent;

		SDL_PushEvent(&event);
		return 0; // We don't want this timer to repeat, thanks!
	}

	void EMSCRIPTENQT_resetTimerCallback(long milliseconds)
	{
		if (!sdlInited)
		{	
			// Can't set SDL timers before SDL is inited; ignore.
			return;
		}
		if (callbackTimer)
		{
			SDL_RemoveTimer(callbackTimer);
			callbackTimer = NULL;
		}
		if (milliseconds == 0)
		{
			milliseconds = 1;
		}
		callbackTimer = SDL_AddTimer(milliseconds, timerCallback, NULL);
		if (callbackTimer == NULL)
		{
			qDebug() << "Error adding SDL timer: " << SDL_GetError();
		}
		else
		{
			qDebug() << "added sdl timer: " << callbackTimer;
		}
	}

	void EMSCRIPTENQT_timerCallback();

	int EMSCRIPTEN_canvas_width_pixels()
	{
		return canvasWidthPixels;
	}
	int EMSCRIPTEN_canvas_height_pixels()
	{
		return canvasHeightPixels;
	}
	int EMSCRIPTEN_flush_pixels(uchar* data)
	{
		int x, y, ytimesw;
		const int BPP = 4;

		if(SDL_MUSTLOCK(sdlCanvas)) 
		{
			if(SDL_LockSurface(sdlCanvas) < 0) return -1;
		}

		uchar *pos = data;
		for(y = 0; y < sdlCanvas->h; y++ ) 
		{
			ytimesw = y*sdlCanvas->pitch/BPP;
			for( x = 0; x < sdlCanvas->w; x++ ) 
			{
				const int b = *pos;
				pos++;
				const int g = *pos;
				pos++;
				const int r = *pos;
				pos++;
				pos++;
				setpixel(sdlCanvas, x, ytimesw, r, g, b);
			}
		}

		if(SDL_MUSTLOCK(sdlCanvas)) SDL_UnlockSurface(sdlCanvas);

		SDL_Flip(sdlCanvas); 
		return 0;
	}
	void EMSCRIPTENQT_cursorChanged(int)
	{
	}

}

Qt::Key sdlToQtKey(SDLKey sdlKey)
{
	if (sdlKey >= SDLK_a && sdlKey <= SDLK_z)
	{
		return static_cast<Qt::Key>((sdlKey - SDLK_a) + Qt::Key_A);
	}
	switch (sdlKey)
	{
	case SDLK_BACKSPACE:
		return Qt::Key_Backspace;
	case SDLK_LEFT:
		qDebug() << "Left";
		return Qt::Key_Left;
	case SDLK_RIGHT:
		return Qt::Key_Right;
	case SDLK_UP:
		return Qt::Key_Up;
	case SDLK_DOWN:
		return Qt::Key_Down;
	default:
		return static_cast<Qt::Key>(0);
	}
}

Qt::KeyboardModifiers sdlModifiersToQtModifiers(SDLMod sdlMod)
{
	Qt::KeyboardModifiers qtModifiers = Qt::NoModifier;
	if ((sdlMod & KMOD_RSHIFT) != 0 || (sdlMod & KMOD_LSHIFT) != 0)
	{
		qtModifiers = qtModifiers | Qt::ShiftModifier;
	}
	return qtModifiers;
}

int EmscriptenSDL::exec(int canvasWidthPixels, int canvasHeightPixels)
{
	::canvasWidthPixels = canvasWidthPixels;
	::canvasHeightPixels = canvasHeightPixels;
	if (SDL_Init( SDL_INIT_EVERYTHING ) == -1)
	{
		qDebug() << "SDL init failed!";
		return -1;
	}
	sdlCanvas = SDL_SetVideoMode( canvasWidthPixels, canvasHeightPixels, 32, SDL_SWSURFACE );
	if (!sdlCanvas)
	{
		qDebug() << "Creation of " << canvasWidthPixels << "x" << canvasHeightPixels << " SDL surface failed!";
		return -1;
	}
	sdlInited = true;

	qDebug() << "SDL - woo!";
	// Any requested timers from before we called SDL_Init would have been ignored, so let's
	// set up a synthetic one, now.
	EMSCRIPTENQT_resetTimerCallback(0);

	SDL_Event event;
	bool quit = false;
	SDL_EnableUNICODE( 1 );
	while (!quit)
	{
		SDL_WaitEvent(&event);
		if( event.type == SDL_QUIT )
		{
			qDebug() << "Quitting";
			quit = true;
		}    
		else if (event.type == SDL_USEREVENT)
		{
			// Qt timer.
			qDebug() << "Calling Qt event loop";
			EMSCRIPTENQT_timerCallback();
		}
		else if (event.type == SDL_MOUSEMOTION)
		{
			EMSCRIPTENQT_mouseCanvasPosChanged(event.motion.x, event.motion.y);
		}
		else if (event.type == SDL_MOUSEBUTTONUP)
		{
			EMSCRIPTENQT_mouseCanvasButtonChanged(1, 0);
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN)
		{
			EMSCRIPTENQT_mouseCanvasButtonChanged(1, 1);
		}
		else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
		{
			const int unicode = event.key.keysym.unicode;
			const SDLKey sdlKey = event.key.keysym.sym;
			const bool isPress = (event.type == SDL_KEYDOWN);
			const Qt::Key qtKey = sdlToQtKey(sdlKey);
			const Qt::KeyboardModifiers qtModifiers = sdlModifiersToQtModifiers(event.key.keysym.mod);
			EMSCRIPTENQT_canvasKeyChanged(unicode, qtKey, qtModifiers, isPress, false);
		}

	}
	qDebug() << "Exiting SDL::exec";
	return 0;
}


#endif

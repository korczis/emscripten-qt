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
		uchar *pos = data;
		QString rgb;
		for (int y = 0; y < 440; y++)
		{
			for (int x = 0; x < 520; x++)
			{
				pos++;
				rgb += QString::number(*pos) + " ";
				pos++;
				rgb += QString::number(*pos) + " ";
				pos++;
				rgb += QString::number(*pos) + " ";
				pos++;
			}
		}
		//		qDebug() << rgb;
		return 0;
	}
	void EMSCRIPTENQT_cursorChanged(int)
	{
	}

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

	}
	qDebug() << "Exiting SDL::exec";
	return 0;
}


#endif

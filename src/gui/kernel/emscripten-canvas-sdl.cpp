#ifdef EMSCRIPTEN_NATIVE
#include "emscripten-canvas-sdl.h"
#include <QtCore/QDebug>
#include <SDL/SDL.h>

namespace
{
	int canvasWidthPixels = 800;
	int canvasHeightPixels = 640;
	SDL_Surface *sdlCanvas = NULL;
}

extern "C" 
{
	void EMSCRIPTENQT_resetTimerCallback(long milliseconds)
	{
		qDebug() << "resetTimerCallback: " << milliseconds;
	}
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

	qDebug() << "SDL - woo!";

	SDL_Event event;
	bool quit = false;
	while (!quit)
	{
		while( SDL_PollEvent( &event ) )
		{
			if( event.type == SDL_QUIT )
			{
qDebug() << "Quitting";
				quit = true;
			}    
		}
	}
qDebug() << "Exiting SDL::exec";
	return 0;
}


#endif

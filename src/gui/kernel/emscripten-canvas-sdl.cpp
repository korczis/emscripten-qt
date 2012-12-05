#ifdef EMSCRIPTEN_NATIVE
#include "emscripten-canvas-sdl.h"
#include <QtCore/QDebug>
#include <SDL/SDL.h>

extern "C" 
{
	void EMSCRIPTENQT_resetTimerCallback(long milliseconds)
	{
		qDebug() << "resetTimerCallback: " << milliseconds;
	}
	int EMSCRIPTEN_canvas_width_pixels()
	{
		return 520;
	}
	int EMSCRIPTEN_canvas_height_pixels()
	{
		return 440;
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

int EmscriptenSDL::exec()
{
	SDL_Init( SDL_INIT_EVERYTHING );
	SDL_Surface *screen = SDL_SetVideoMode( 640, 480, 32, SDL_SWSURFACE );
	SDL_Event event;

	qDebug() << "SDL - woo!";

	while( SDL_PollEvent( &event ) )
        {
	}
	return 0;
}


#endif

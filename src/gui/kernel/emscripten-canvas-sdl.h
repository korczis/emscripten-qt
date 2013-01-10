#ifndef EMSCRIPTEN_CANVAS_SDL_H
#define EMSCRIPTEN_CANVAS_SDL_H

#include <QtCore/qglobal.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class Q_GUI_EXPORT EmscriptenSDL
{
public:
	static bool initScreen(int canvasWidthPixels, int canvasHeightPixels);
	static int exec();
    static void setAttemptedLocalEventLoopCallback(void (*callback)() );
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // EMSCRIPTEN_CANVAS_SDL_H



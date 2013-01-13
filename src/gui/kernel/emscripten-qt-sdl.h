#ifndef EMSCRIPTEN_QT_SDL_H
#define EMSCRIPTEN_QT_SDL_H

#include <QtCore/qglobal.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class Q_GUI_EXPORT EmscriptenSDL
{
public:
    static int run(int canvasWidthPixels, int canvasHeightPixels, int argc, char** argv);
    static void setAttemptedLocalEventLoopCallback(void (*callback)() );
private:
    static bool initScreen(int canvasWidthPixels, int canvasHeightPixels);
    static int exec();
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // EMSCRIPTEN_QT_SDL_H



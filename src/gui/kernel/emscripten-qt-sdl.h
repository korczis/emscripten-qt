#ifndef EMSCRIPTEN_QT_SDL_H
#define EMSCRIPTEN_QT_SDL_H

#include <QtCore/qglobal.h>
#include <QtGui/QImage>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class Q_GUI_EXPORT EmscriptenQtSDL
{
public:
    static int run(int canvasWidthPixels, int canvasHeightPixels, int argc, char** argv);

    typedef void(*Callback)();
    static void setAttemptedLocalEventLoopCallback(Callback callback);
    static const Callback TRIGGER_ASSERT;
    static QImage screenAsQImage();
private:
    static bool initScreen(int canvasWidthPixels, int canvasHeightPixels);
    static int exec();
    static void triggerAssert();
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // EMSCRIPTEN_QT_SDL_H



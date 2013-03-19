#include <QtGui/QWidget>
#include <QtGui/QApplication>

#include "testdriver.h"

#ifndef EMSCRIPTEN_NATIVE
int main(int argc, char *argv[])
#else
int emscriptenQtSDLMain(int argc, char *argv[])
#endif
{
    QApplication *app = new QApplication(argc, argv);
    QWidget *widget = new QWidget(0);
    widget->showFullScreen();

    TestDriver *testDriver = new TestDriver();
    testDriver->beginRunAllTestsAsync();

    return app->exec();
}

#ifdef EMSCRIPTEN_NATIVE
#include <QtGui/emscripten-qt-sdl.h>
int main(int argc, char *argv[])
{
        EmscriptenQtSDL::setAttemptedLocalEventLoopCallback(EmscriptenQtSDL::TRIGGER_ASSERT);
        return EmscriptenQtSDL::run(640, 480, argc, argv);
}
#endif

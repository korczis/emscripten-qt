#include <QtGui/QWidget>
#include <QtGui/QApplication>
#include <QtCore/QDebug>

#include "testdriver.h"
#include "../shared/canvasdimensions.h"

void myMessageOutput(QtMsgType type, const char *msg)
{
 switch (type) {
 case QtDebugMsg:
     fprintf(stdout, "%s\n", msg);
     fflush(stdout);
     break;
 case QtWarningMsg:
     fprintf(stderr, "Warning: %s\n", msg);
     break;
 case QtCriticalMsg:
     fprintf(stderr, "Critical: %s\n", msg);
     break;
 case QtFatalMsg:
     fprintf(stderr, "Fatal: %s\n", msg);
     abort();
 }
}

#ifndef EMSCRIPTEN_NATIVE
int main(int argc, char *argv[])
#else
int emscriptenQtSDLMain(int argc, char *argv[])
#endif
{
#ifdef EMSCRIPTEN_NATIVE
    // Slightly hackish way of determining if we are using the html5canvas.
    bool usingHtml5Canvas = false;
    for (int argIndex = 0; argIndex < argc; argIndex++)
    {
        if (QString(argv[argIndex]) == "html5canvas")
        {
            usingHtml5Canvas = true;
        }
    }
#else
    const bool usingHtml5Canvas = true;
#endif
    qInstallMsgHandler(myMessageOutput);
    QApplication *app = new QApplication(argc, argv);


    TestDriver *testDriver = new TestDriver(usingHtml5Canvas);
    testDriver->beginRunAllTestsAsync();

    return app->exec();
}

#ifdef EMSCRIPTEN_NATIVE
#include <QtGui/emscripten-qt-sdl.h>
int main(int argc, char *argv[])
{
        EmscriptenQtSDL::setAttemptedLocalEventLoopCallback(EmscriptenQtSDL::TRIGGER_ASSERT);
        return EmscriptenQtSDL::run(CANVAS_WIDTH, CANVAS_HEIGHT, argc, argv);
}
#endif

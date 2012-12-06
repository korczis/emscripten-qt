#include "qkbdemscriptencanvas_qws.h"
#include <qdebug.h>

extern "C"
{
    void EMSCRIPTENQT_canvasKeyChanged(int unicode, int keycode, int modifiers,
                                 int isPress, int autoRepeat) __attribute__((used));
    void EMSCRIPTENQT_canvasKeyChanged(int unicode, int keycode, int modifiers,
                                 int isPress, int autoRepeat)
    {
        QWSEmscriptenCanvasKeyboardHandler::canvasKeyChanged(unicode, keycode, modifiers, isPress, autoRepeat);
    }
}

QWSEmscriptenCanvasKeyboardHandler *QWSEmscriptenCanvasKeyboardHandler::m_instance = NULL;

QWSEmscriptenCanvasKeyboardHandler::QWSEmscriptenCanvasKeyboardHandler(const QString &device)
{
    qDebug() << "QWSEmscriptenCanvasKeyboardHandler::QWSEmscriptenCanvasKeyboardHandler device: " << device;
    m_instance = this;
}
QWSEmscriptenCanvasKeyboardHandler::~QWSEmscriptenCanvasKeyboardHandler()
{
    qDebug() << "QWSEmscriptenCanvasKeyboardHandler::~QWSEmscriptenCanvasKeyboardHandler";
}

void QWSEmscriptenCanvasKeyboardHandler::canvasKeyChanged(int unicode, int keycode, int modifiers, int isPress, int autoRepeat)
{
    m_instance->processKeyEvent(unicode, keycode, static_cast<Qt::KeyboardModifiers>(modifiers), isPress, autoRepeat);
}
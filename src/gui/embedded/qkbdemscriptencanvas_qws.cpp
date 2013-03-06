#include "qkbdemscriptencanvas_qws.h"
#include <qdebug.h>

extern bool mainEventLoopStarted;
extern "C"
{
    void EMSCRIPTENQT_canvasKeyChanged(int unicode, int keycode, int modifiers,
                                 int isPress, int autoRepeat) __attribute__((used));
    void EMSCRIPTENQT_canvasKeyChanged(int unicode, int keycode, int modifiers,
                                 int isPress, int autoRepeat)
    {
        if (!mainEventLoopStarted)
            return;
        if (keycode == 0)
        {
            // Try to reconstruct the keycode from the unicode.
            if (unicode <= 0x0ff) {
                if (unicode >= 'a' && unicode <= 'z')
                    keycode = Qt::Key_A + unicode - 'a';
                else
                    keycode = unicode;
            }
        }
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

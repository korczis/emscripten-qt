#include "qmouseemscriptencanvas_qws.h"
#include "private/qeventdispatcher_emscripten_p.h"
#include "qdebug.h"

namespace
{
    int currentMouseX = -1;
    int currentMouseY = -1;
    int mouseButtonsDown = 0;
}


extern "C"
{
    void EMSCRIPTENQT_mouseCanvasPosChanged(int x, int y) __attribute__((used))  ;
    void EMSCRIPTENQT_mouseCanvasButtonChanged(int button, int state, int wheel) __attribute__((used))  ;
    void EMSCRIPTENQT_mouseCanvasPosChanged(int x, int y)
    {
        currentMouseX = x;
        currentMouseY = y;
        QEmscriptenCanvasMouseHandler::canvasMouseChanged(x, y, mouseButtonsDown, 0);
    }
    void EMSCRIPTENQT_mouseCanvasButtonChanged(int button, int state, int wheel)
    {
        if (button)
        {
            if (state)
            {
                mouseButtonsDown |= button;
            }
            else
            {
                mouseButtonsDown = mouseButtonsDown & (7 - button);
            }
            }
        QEmscriptenCanvasMouseHandler::canvasMouseChanged(currentMouseX, currentMouseY, mouseButtonsDown, wheel/3);
    }
}

QEmscriptenCanvasMouseHandler* QEmscriptenCanvasMouseHandler::m_instance = NULL;


QEmscriptenCanvasMouseHandler::QEmscriptenCanvasMouseHandler(const QString &driver,
                                       const QString &device)
{
    qDebug() << "QEmscriptenCanvasMouseHandler::QEmscriptenCanvasMouseHandler driver: " << driver << " device: " << device;
    m_instance = this;
}
QEmscriptenCanvasMouseHandler::~QEmscriptenCanvasMouseHandler()
{
    qDebug() << "QEmscriptenCanvasMouseHandler::~QEmscriptenCanvasMouseHandler";
}

void QEmscriptenCanvasMouseHandler::resume()
{
    qDebug() << "QEmscriptenCanvasMouseHandler::resume";
}
void QEmscriptenCanvasMouseHandler::suspend()
{
    qDebug() << "QEmscriptenCanvasMouseHandler::suspend";
}

void QEmscriptenCanvasMouseHandler::canvasMouseChanged(int x, int y, int buttons, int wheel)
{
    QEventDispatcherEmscripten::setBatchProcessingEvents();
    m_instance->mouseChanged(QPoint(x, y), buttons, wheel);
    QEventDispatcherEmscripten::batchProcessEventsAndScheduleNextCallback();
}

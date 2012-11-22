#include "qmouseemscriptencanvas_qws.h"
#include "qdebug.h"

QEmscriptenCanvasMouseHandler::QEmscriptenCanvasMouseHandler(const QString &driver,
                                       const QString &device)
{
    qDebug() << "QEmscriptenCanvasMouseHandler::QEmscriptenCanvasMouseHandler driver: " << driver << " device: " << device;
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

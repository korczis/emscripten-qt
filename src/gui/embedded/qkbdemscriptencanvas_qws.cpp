#include "qkbdemscriptencanvas_qws.h"
#include <qdebug.h>

QWSEmscriptenCanvasKeyboardHandler::QWSEmscriptenCanvasKeyboardHandler(const QString &device)
{
    qDebug() << "QWSEmscriptenCanvasKeyboardHandler::QWSEmscriptenCanvasKeyboardHandler device: " << device;
}
QWSEmscriptenCanvasKeyboardHandler::~QWSEmscriptenCanvasKeyboardHandler()
{
    qDebug() << "QWSEmscriptenCanvasKeyboardHandler::~QWSEmscriptenCanvasKeyboardHandler";
}

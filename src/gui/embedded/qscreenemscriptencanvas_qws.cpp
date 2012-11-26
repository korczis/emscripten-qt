#include "qscreenemscriptencanvas_qws.h"
#include <qdebug.h>

extern "C"
{
    int EMSCRIPTEN_canvas_width_pixels();
    int EMSCRIPTEN_canvas_height_pixels();
}

QEmscriptenCanvasScreen::QEmscriptenCanvasScreen(int display_id)
    : QScreen(display_id, CustomClass)
{
    qDebug() << "QEmscriptenCanvasScreen::QEmscriptenCanvasScreen: display_id: " << display_id;
}
QEmscriptenCanvasScreen::~QEmscriptenCanvasScreen()
{
    qDebug() << "QEmscriptenCanvasScreen::~QEmscriptenCanvasScreen: display_id: ";
}
bool QEmscriptenCanvasScreen::initDevice()
{
    qDebug() << "QEmscriptenCanvasScreen::initDevice";
    return true;
}
bool QEmscriptenCanvasScreen::connect(const QString &displaySpec)
{
    qDebug() << "QEmscriptenCanvasScreen::connect: displaySpec: " << displaySpec;
    w = dw = EMSCRIPTEN_canvas_width_pixels();
    h = dh = EMSCRIPTEN_canvas_height_pixels();
    // assume 72 dpi as default, to calculate the physical dimensions if not specified
    const int defaultDpi = 72;
    // Handle display physical size
    physWidth = qRound(dw * 25.4 / defaultDpi);
    physHeight = qRound(dh * 25.4 / defaultDpi);
    return true;
}
void QEmscriptenCanvasScreen::disconnect()
{
    qDebug() << "QEmscriptenCanvasScreen::~disconnect";
}
void QEmscriptenCanvasScreen::shutdownDevice()
{
    qDebug() << "QEmscriptenCanvasScreen::shutdownDevice";
}
void QEmscriptenCanvasScreen::save()
{
    qDebug() << "QEmscriptenCanvasScreen::save";
}
void QEmscriptenCanvasScreen::restore()
{
    qDebug() << "QEmscriptenCanvasScreen::restore";
}
void QEmscriptenCanvasScreen::setMode(int nw,int nh,int nd)
{
    qDebug() << "QEmscriptenCanvasScreen::setMode: nw: " << nw << " nh: " << nh << " nd: " << nd;
}
void QEmscriptenCanvasScreen::setDirty(const QRect& r)
{
    qDebug() << "QEmscriptenCanvasScreen::setDirty: r: " << r;
}
void QEmscriptenCanvasScreen::blank(bool something)
{
    qDebug() << "QEmscriptenCanvasScreen::blank: something: " << something;
}
static void setBrightness(int b)
{
    qDebug() << "QEmscriptenCanvasScreen::setBrightness: b: " << b;
}


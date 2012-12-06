#include "qscreenemscriptencanvas_qws.h"
#include <qdebug.h>

extern "C"
{
    int EMSCRIPTEN_canvas_width_pixels();
    int EMSCRIPTEN_canvas_height_pixels();
    int EMSCRIPTEN_flush_pixels(uchar* data, int x, int y, int w, int h);
}

QEmscriptenCanvasScreen::QEmscriptenCanvasScreen(int display_id)
    : QScreen(display_id, CustomClass)
{
    qDebug() << "QEmscriptenCanvasScreen::QEmscriptenCanvasScreen: display_id: " << display_id;
}
QEmscriptenCanvasScreen::~QEmscriptenCanvasScreen()
{
    qDebug() << "QEmscriptenCanvasScreen::~QEmscriptenCanvasScreen: display_id: ";
    free(data);
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
    // Canvas is rgba
    d = 32;
    lstep = 4 * w;
    size = lstep * h;
    data = static_cast<uchar*>(malloc(size));
    //memset(static_cast<void*>(data), 255, size);
    setPixelFormat(QImage::Format_ARGB32);
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

void QEmscriptenCanvasScreen::exposeRegion(QRegion r, int changing)
{
    qDebug() << "QEmscriptenCanvasScreen::exposeRegion: region: " << r << " changing: " << changing;
    // first, call the parent implementation. The parent implementation will update
    // the region on our in-memory surface
    QScreen::exposeRegion(r, changing);
    EMSCRIPTEN_flush_pixels(data + r.boundingRect().top() * lstep + 4 * r.boundingRect().left(), r.boundingRect().left(), r.boundingRect().top(), r.boundingRect().width(), r.boundingRect().height());
}


static void setBrightness(int b)
{
    qDebug() << "QEmscriptenCanvasScreen::setBrightness: b: " << b;
}


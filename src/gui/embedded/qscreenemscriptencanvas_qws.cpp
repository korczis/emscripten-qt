#include "qscreenemscriptencanvas_qws.h"
#include "qwindowsystem_qws.h"
#include <painting/html5canvasinterface.h>
#include <painting/qwindowsurface_qws_p.h>
#include <kernel/qapplication_p.h>
#include <painting/qgraphicssystemfactory_p.h>
#include <painting/qgraphicssystem_html5canvas_p.h>
#include <qdebug.h>

extern "C"
{
    int EMSCRIPTENQT_canvas_width_pixels();
    int EMSCRIPTENQT_canvas_height_pixels();
    int EMSCRIPTENQT_flush_pixels(uchar* data, int x, int y, int w, int h);
}

QEmscriptenCanvasScreen::QEmscriptenCanvasScreen(int display_id)
    : QScreen(display_id, CustomClass)
{
    qDebug() << "QEmscriptenCanvasScreen::QEmscriptenCanvasScreen: display_id: " << display_id;
    qDebug() << "Initialising graphics system";
    QGraphicsSystem *graphics_system = QGraphicsSystemFactory::create(QApplicationPrivate::graphics_system_name);
    setGraphicsSystem(graphics_system);
    m_useRaster = (dynamic_cast<QHtml5CanvasGraphicsSystem*>(graphics_system) == NULL);
    qDebug() << "Raster " << m_useRaster;
    m_mainCanvasHandle = Html5CanvasInterface::handleForMainCanvas();
    qDebug() << "Main canvas handle: " << m_mainCanvasHandle;
    Q_ASSERT(m_mainCanvasHandle != -1);
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
    w = dw = EMSCRIPTENQT_canvas_width_pixels();
    h = dh = EMSCRIPTENQT_canvas_height_pixels();
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
    //qDebug() << "QEmscriptenCanvasScreen::setDirty: r: " << r;
}
void QEmscriptenCanvasScreen::blank(bool something)
{
    qDebug() << "QEmscriptenCanvasScreen::blank: something: " << something;
}

void QEmscriptenCanvasScreen::exposeRegion(QRegion r, int changing)
{
    if (m_useRaster)
    {
        // first, call the parent implementation. The parent implementation will update
        // the region on our in-memory surface
        QScreen::exposeRegion(r, changing);
        r = r.intersected(QRect(0, 0, EMSCRIPTENQT_canvas_width_pixels(), EMSCRIPTENQT_canvas_height_pixels()));
        EMSCRIPTENQT_flush_pixels(data, r.boundingRect().left(), r.boundingRect().top(), r.boundingRect().width(), r.boundingRect().height());
    }
    else
    {
        // TODO - needs a lot of work, here.  Just "compose" the windows for now by just blitting their
        // backing canvases to the main canvas: we ultimately need to deal with transparency, z-ordering, etc,
        // but most of all will need to write directly to the canvas when we can, eliminating the need for a blit.
       foreach(QWSWindow *win, qwsServer->clientWindows())
       {
           QWSHtml5CanvasSurface *windowSurface = dynamic_cast<QWSHtml5CanvasSurface*>(win->windowSurface());
           Q_ASSERT(windowSurface);

           const int winX = win->requestedRegion().boundingRect().left();
           const int winY = win->requestedRegion().boundingRect().top();
           qDebug() << "blah: " << win->requestedRegion().boundingRect();
           qDebug() << "surface: " << windowSurface;

           const int winBackingCanvasHandle = windowSurface->backingPixmapData()->canvasHandle();

           Html5CanvasInterface::drawCanvasOnMainCanvas(winBackingCanvasHandle, winX, winY);
       }
    }
}

QWSWindowSurface* QEmscriptenCanvasScreen::createSurface(const QString& key) const
{
    qDebug() << "QEmscriptenCanvasScreen::createSurface(const QString& key) key:" << key << " m_useRaster: " << m_useRaster;
    if (m_useRaster)
    {
        return QScreen::createSurface(key);
    }
    else
    {
        if (QApplication::type() == QApplication::GuiServer)
            return new QWSHtml5CanvasSurface();
    }
    return 0;
}

QWSWindowSurface* QEmscriptenCanvasScreen::createSurface(QWidget* widget) const
{
    qDebug() << "QEmscriptenCanvasScreen::createSurface(const QWidget *widget) m_useRaster: " << m_useRaster;
    if (m_useRaster)
    {
        return QScreen::createSurface(widget);
    }
    else
    {
        return new QWSHtml5CanvasSurface(widget);
    }
}

static void setBrightness(int b)
{
    qDebug() << "QEmscriptenCanvasScreen::setBrightness: b: " << b;
}


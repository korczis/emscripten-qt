#include <qscreen_qws.h>
#include "qgraphicssystem_html5canvas_p.h"
#include <private/qpixmap_raster_p.h>
#include <private/qwindowsurface_qws_p.h>

QT_BEGIN_NAMESPACE

QPixmapData *QHtml5CanvasGraphicsSystem::createPixmapData(QPixmapData::PixelType type) const
{
    qDebug() << "QHtml5CanvasGraphicsSystem::createPixmapData: " << type;
    if (screen->pixmapDataFactory())
        return screen->pixmapDataFactory()->create(type); //### For 4.4 compatibility
    else
        return new QRasterPixmapData(type);
}

QWindowSurface *QHtml5CanvasGraphicsSystem::createWindowSurface(QWidget *widget) const
{
    qDebug() << "QHtml5CanvasGraphicsSystem::createWindowSurface: ";
    return screen->createSurface(widget);
}

QT_END_NAMESPACE

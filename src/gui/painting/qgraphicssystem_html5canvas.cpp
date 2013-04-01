#include <qscreen_qws.h>
#include "qgraphicssystem_html5canvas_p.h"
#include <image/qpixmap_html5canvas_p.h>
#include <private/qpixmap_raster_p.h>
#include <private/qwindowsurface_qws_p.h>

QT_BEGIN_NAMESPACE

QPixmapData *QHtml5CanvasGraphicsSystem::createPixmapData(QPixmapData::PixelType type) const
{
    qDebug() << "Returning QHtml5CanvasPixmapData";
    return new QHtml5CanvasPixmapData(type);
}

QWindowSurface *QHtml5CanvasGraphicsSystem::createWindowSurface(QWidget *widget) const
{
    qDebug() << "QHtml5CanvasGraphicsSystem::createWindowSurface: ";
    return QScreen::instance()->createSurface(widget);
}

QT_END_NAMESPACE

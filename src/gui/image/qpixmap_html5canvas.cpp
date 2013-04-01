#include "qpixmap.h"

#include <private/qfont_p.h>

#include "qnativeimage_p.h"
#include "qimage_p.h"
#include "qpaintengine.h"

#include "qbitmap.h"
#include "qimage.h"
#include <QBuffer>
#include <QImageReader>
#include "qpixmap_html5canvas_p.h"
#include <private/qimage_p.h>
#include <private/qwidget_p.h>
#include <private/qdrawhelper_p.h>


QT_BEGIN_NAMESPACE

QHtml5CanvasPixmapData::QHtml5CanvasPixmapData(PixelType type)
    : QPixmapData(type, RasterClass)
{
}

QHtml5CanvasPixmapData::~QHtml5CanvasPixmapData()
{
}

QPixmapData *QHtml5CanvasPixmapData::createCompatiblePixmapData() const
{
    return new QHtml5CanvasPixmapData(pixelType());
}

void QHtml5CanvasPixmapData::resize(int width, int height)
{
    qDebug() << "QHtml5CanvasPixmapData::resize(int width, int height)";
}

bool QHtml5CanvasPixmapData::fromData(const uchar *buffer, uint len, const char *format,
                      Qt::ImageConversionFlags flags)
{
    QByteArray a = QByteArray::fromRawData(reinterpret_cast<const char *>(buffer), len);
    QBuffer b(&a);
    b.open(QIODevice::ReadOnly);
    QImage image = QImageReader(&b, format).read();
    if (image.isNull())
        return false;

    createPixmapForImage(image, flags, /* inplace = */true);
    return !isNull();
}

void QHtml5CanvasPixmapData::fromImage(const QImage &sourceImage,
                                  Qt::ImageConversionFlags flags)
{
    Q_UNUSED(flags);
    QImage image = sourceImage;
    createPixmapForImage(image, flags, /* inplace = */false);
}

void QHtml5CanvasPixmapData::fromImageReader(QImageReader *imageReader,
                                        Qt::ImageConversionFlags flags)
{
    Q_UNUSED(flags);
    QImage image = imageReader->read();
    if (image.isNull())
        return;

    createPixmapForImage(image, flags, /* inplace = */true);
}

// from qwindowsurface.cpp
extern void qt_scrollRectInImage(QImage &img, const QRect &rect, const QPoint &offset);

void QHtml5CanvasPixmapData::copy(const QPixmapData *data, const QRect &rect)
{
    fromImage(data->toImage(rect).copy(), Qt::NoOpaqueDetection);
}

bool QHtml5CanvasPixmapData::scroll(int dx, int dy, const QRect &rect)
{
    qDebug() << "QHtml5CanvasPixmapData::scroll(int dx, int dy, const QRect &rect)";
    return true;
}

void QHtml5CanvasPixmapData::fill(const QColor &color)
{
    qDebug() << "QHtml5CanvasPixmapData::fill(const QColor &color)";
}

void QHtml5CanvasPixmapData::setMask(const QBitmap &mask)
{
    qDebug() << "QHtml5CanvasPixmapData::setMask(const QBitmap &mask)";
}

bool QHtml5CanvasPixmapData::hasAlphaChannel() const
{
    qDebug() << "QHtml5CanvasPixmapData::hasAlphaChannel()";
    return true;
}

QImage QHtml5CanvasPixmapData::toImage() const
{
    qDebug() << "QHtml5CanvasPixmapData::toImage()";
    return QImage();
}

QImage QHtml5CanvasPixmapData::toImage(const QRect &rect) const
{
    qDebug() << "QHtml5CanvasPixmapData::toImage(const QRect &rect)";
}

void QHtml5CanvasPixmapData::setAlphaChannel(const QPixmap &alphaChannel)
{
    qDebug() << "QHtml5CanvasPixmapData::setAlphaChannel(const QPixmap &alphaChannel)";
}

QPaintEngine* QHtml5CanvasPixmapData::paintEngine() const
{
    qDebug() << "QHtml5CanvasPixmapData::paintEngine()";
    return NULL;
}

int QHtml5CanvasPixmapData::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    qDebug() << "QHtml5CanvasPixmapData::metric(QPaintDevice::PaintDeviceMetric metric)";
    return 0;
}

void QHtml5CanvasPixmapData::createPixmapForImage(QImage &sourceImage, Qt::ImageConversionFlags flags, bool inPlace)
{
    qDebug() << "QHtml5CanvasPixmapData::createPixmapForImage(QImage &sourceImage, Qt::ImageConversionFlags flags, bool inPlace)";
}

QT_END_NAMESPACE

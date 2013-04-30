#include "qpixmap_html5canvas_p.h"
#ifndef QT_NO_GRAPHICSSYSTEM_HTML5CANVAS
#include "qpixmap.h"

#include <private/qfont_p.h>

#include "qnativeimage_p.h"
#include "qimage_p.h"
#include "qpaintengine.h"

#include "qbitmap.h"
#include "qimage.h"
#include <QBuffer>
#include <QImageReader>
#include "painting/qpaintengine_html5canvas_p.h"
#include "painting/html5canvasinterface.h"
#include <private/qimage_p.h>
#include <private/qwidget_p.h>
#include <private/qdrawhelper_p.h>


QT_BEGIN_NAMESPACE

QHtml5CanvasPixmapData::QHtml5CanvasPixmapData(PixelType type)
    : QPixmapData(type, Html5CanvasClass),
      pengine(NULL),
      m_canvasHandle(-1)
{
    is_null = true;
    qDebug() << "Created QHtml5CanvasPixmapData : " << (void*)this;
}

QHtml5CanvasPixmapData::~QHtml5CanvasPixmapData()
{
    qDebug() << "Destroyed QHtml5CanvasPixmapData : " << (void*)this;
}

QPixmapData *QHtml5CanvasPixmapData::createCompatiblePixmapData() const
{
    return new QHtml5CanvasPixmapData(pixelType());
}

void QHtml5CanvasPixmapData::resize(int width, int height)
{
    qDebug() << "QHtml5CanvasPixmapData::resize(int width, int height) width: " << width << " height: " << height;
    w = width;
    h = height;
    if (width <= 0 || height <= 0)
    {
        // TODO - destroy canvas.
        is_null = true;
    }
    else
    {
        // TODO - check for existing canvas!
        m_canvasHandle = Html5CanvasInterface::createCanvas(width, height);
        qDebug() << "Create canvas: got handle: " << m_canvasHandle;
        // TODO - width and height if failed?
        is_null = (m_canvasHandle == -1);
    }
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
    return QImage();
}

void QHtml5CanvasPixmapData::setAlphaChannel(const QPixmap &alphaChannel)
{
    qDebug() << "QHtml5CanvasPixmapData::setAlphaChannel(const QPixmap &alphaChannel)";
}

QPaintEngine* QHtml5CanvasPixmapData::paintEngine() const
{
    qDebug() << "QHtml5CanvasPixmapData::paintEngine()";
    if (!pengine)
    {
        pengine = new QHtml5CanvasPaintEngine;
    }
    return pengine;
}

int QHtml5CanvasPixmapData::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    qDebug() << "QHtml5CanvasPixmapData::metric(QPaintDevice::PaintDeviceMetric metric)";
    return 0;
}

CanvasHandle QHtml5CanvasPixmapData::canvasHandle() const
{
    return m_canvasHandle;
}

void QHtml5CanvasPixmapData::createPixmapForImage(QImage &sourceImage, Qt::ImageConversionFlags flags, bool inPlace)
{
    qDebug() << "QHtml5CanvasPixmapData::createPixmapForImage(QImage &sourceImage, Qt::ImageConversionFlags flags, bool inPlace) flags: " << flags << " inPlace: " << inPlace << " image size: " << sourceImage.size() << " image format: " << sourceImage.format();
    Q_ASSERT(sourceImage.format() == QImage::Format_ARGB32);
    resize(sourceImage.width(), sourceImage.height());
    if (is_null)
    {
        // Failed to resize; abort.
        return;
    }
    // Very crude initial stab - we can do better than this, I hope!
    const int width = sourceImage.width();
    const int height = sourceImage.height();
    uchar* rgbaData = static_cast<uchar*>(malloc(sourceImage.width() * sourceImage.height() * 4));
    uchar* rgbaDataWriter = rgbaData;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const QRgb rgba = sourceImage.pixel(x, y);
            *rgbaDataWriter = (uchar)qRed(rgba);
            rgbaDataWriter++;
            *rgbaDataWriter = (uchar)qGreen(rgba);
            rgbaDataWriter++;
            *rgbaDataWriter = (uchar)qBlue(rgba);
            rgbaDataWriter++;
            *rgbaDataWriter = (uchar)qAlpha(rgba);
            rgbaDataWriter++;
        }
    }
    Html5CanvasInterface::setCanvasPixelsRaw(m_canvasHandle, rgbaData, width, height);
    free(rgbaData);
}

QT_END_NAMESPACE
#endif
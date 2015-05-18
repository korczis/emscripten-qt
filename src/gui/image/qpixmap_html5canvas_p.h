#ifndef QPIXMAPDATA_HTML5CANVAS_P_H
#define QPIXMAPDATA_HTML5CANVAS_P_H

#ifndef QT_NO_GRAPHICSSYSTEM_HTML5CANVAS
//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qpixmapdata_p.h>
#include <QtGui/private/qpixmapdatafactory_p.h>
#include <painting/html5canvasinterface.h>

class QHtml5CanvasPaintEngine;

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QHtml5CanvasPixmapData : public QPixmapData
{
public:
    QHtml5CanvasPixmapData(PixelType type);
    ~QHtml5CanvasPixmapData();

    QPixmapData *createCompatiblePixmapData() const;

    void resize(int width, int height);
    void fromFile(const QString &filename, Qt::ImageConversionFlags flags);
    bool fromData(const uchar *buffer, uint len, const char *format, Qt::ImageConversionFlags flags);
    void fromImage(const QImage &image, Qt::ImageConversionFlags flags);
    void fromImageReader(QImageReader *imageReader, Qt::ImageConversionFlags flags);

    void copy(const QPixmapData *data, const QRect &rect);
    bool scroll(int dx, int dy, const QRect &rect);
    void fill(const QColor &color);
    void setMask(const QBitmap &mask);
    bool hasAlphaChannel() const;
    void setAlphaChannel(const QPixmap &alphaChannel);
    QImage toImage() const;
    QImage toImage(const QRect &rect) const;
    QPaintEngine* paintEngine() const;
    int metric(QPaintDevice::PaintDeviceMetric metric) const;

    CanvasHandle canvasHandle() const ;
private:
    void createPixmapForImage(QImage &sourceImage, Qt::ImageConversionFlags flags, bool inPlace);
    mutable QHtml5CanvasPaintEngine *pengine;
    CanvasHandle m_canvasHandle;

};

QT_END_NAMESPACE

#endif

#endif // QPIXMAPDATA_RASTER_P_H



#ifndef QPAINTENGINE_HTML5CANVAS_P_H
#define QPAINTENGINE_HTML5CANVAS_P_H

#ifndef QT_NO_GRAPHICSSYSTEM_HTML5CANVAS
//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "private/qpaintengineex_p.h"
#include "QtGui/qpainterpath.h"
#include "private/qdatabuffer_p.h"
#include "private/qdrawhelper_p.h"
#include "private/qpaintengine_p.h"
#include "private/qrasterizer_p.h"
#include "private/qstroker_p.h"
#include "private/qpainter_p.h"
#include "private/qtextureglyphcache_p.h"
#include "private/qoutlinemapper_p.h"
#include <painting/html5canvasinterface.h>

#include <qstack.h>

#include <stdlib.h>

QT_BEGIN_NAMESPACE

class QOutlineMapper;
class QHtml5CanvasPaintEnginePrivate;
class QRasterBuffer;
class QClipData;
class QCustomRasterPaintDevice;

class QHtml5CanvasPaintEngineState : public QPainterState
{
public:
    QHtml5CanvasPaintEngineState(QHtml5CanvasPaintEngineState &other);
    QHtml5CanvasPaintEngineState();
    ~QHtml5CanvasPaintEngineState();

};




/*******************************************************************************
 * QHtml5CanvasPaintEngine
 */
class
#ifdef Q_WS_QWS
Q_GUI_EXPORT
#endif
QHtml5CanvasPaintEngine : public QPaintEngineEx
{
    Q_DECLARE_PRIVATE(QHtml5CanvasPaintEngine)
public:

    QHtml5CanvasPaintEngine();
    ~QHtml5CanvasPaintEngine();
    bool begin(QPaintDevice *device);
    bool end();

    virtual void updateState(const QPaintEngineState &state);

    void penChanged();
    void brushChanged();
    void brushOriginChanged();
    void opacityChanged();
    void compositionModeChanged();
    void renderHintsChanged();
    void transformChanged();
    void clipEnabledChanged();

    void setState(QPainterState *s);
    QPainterState *createState(QPainterState *orig) const;
    inline QHtml5CanvasPaintEngineState *state() {
        return static_cast<QHtml5CanvasPaintEngineState *>(QPaintEngineEx::state());
    }
    inline const QHtml5CanvasPaintEngineState *state() const {
        return static_cast<const QHtml5CanvasPaintEngineState *>(QPaintEngineEx::state());
    }

    void updateBrush(const QBrush &brush);
    void updatePen(const QPen &pen);

    void updateMatrix(const QTransform &matrix);

    void drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode);
    void drawPolygon(const QPoint *points, int pointCount, PolygonDrawMode mode);
    void fillPath(const QPainterPath &path, QSpanData *fillData);
    void fillPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode);

    void drawEllipse(const QRectF &rect);

    void fillRect(const QRectF &rect, const QBrush &brush);
    void fillRect(const QRectF &rect, const QColor &color);

    void drawRects(const QRect  *rects, int rectCount);
    void drawRects(const QRectF *rects, int rectCount);

    void drawPixmap(const QPointF &p, const QPixmap &pm);
    void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr);
    void drawImage(const QPointF &p, const QImage &img);
    void drawImage(const QRectF &r, const QImage &pm, const QRectF &sr,
                   Qt::ImageConversionFlags falgs = Qt::AutoColor);
    void drawTiledPixmap(const QRectF &r, const QPixmap &pm, const QPointF &sr);
    void drawTextItem(const QPointF &p, const QTextItem &textItem);

    void drawLines(const QLine *line, int lineCount);
    void drawLines(const QLineF *line, int lineCount);

    void drawPoints(const QPointF *points, int pointCount);
    void drawPoints(const QPoint *points, int pointCount);

    void stroke(const QVectorPath &path, const QPen &pen);
    void fill(const QVectorPath &path, const QBrush &brush);

    void clip(const QVectorPath &path, Qt::ClipOperation op);
    void clip(const QRect &rect, Qt::ClipOperation op);
    void clip(const QRegion &region, Qt::ClipOperation op);

    void drawStaticTextItem(QStaticTextItem *textItem);

    QRect clipBoundingRect() const;

#ifdef Q_NO_USING_KEYWORD
    inline void drawEllipse(const QRect &rect) { QPaintEngineEx::drawEllipse(rect); }
#else
    using QPaintEngineEx::drawPolygon;
    using QPaintEngineEx::drawEllipse;
#endif

    void releaseBuffer();

    QSize size() const;

#ifndef QT_NO_DEBUG
    void saveBuffer(const QString &s) const;
#endif


    void alphaPenBlt(const void* src, int bpl, int depth, int rx,int ry,int w,int h);

    Type type() const { return Raster; }

    QPoint coordinateOffset() const;

    bool supportsTransformations(const QFontEngine *fontEngine) const;
    bool supportsTransformations(qreal pixelSize, const QTransform &m) const;

protected:
    QHtml5CanvasPaintEngine(QHtml5CanvasPaintEnginePrivate &d, QPaintDevice *);
private:
    void setHtml5Brush(const QBrush& brush);
};


class
#ifdef Q_WS_QWS
Q_GUI_EXPORT
#endif
QHtml5CanvasPaintEnginePrivate : public QPaintEngineExPrivate
{
    Q_DECLARE_PUBLIC(QHtml5CanvasPaintEngine)
public:
    QHtml5CanvasPaintEnginePrivate()
        : device(0),
          canvasHandle(-1),
          isReallyActive(false)
    {};
    QPaintDevice* device;
    CanvasHandle canvasHandle;
    bool isReallyActive;
    QStack<QPainterState*> savedStateHistory;
};

QT_END_NAMESPACE
#endif
#endif // QPAINTENGINE_RASTER_P_H

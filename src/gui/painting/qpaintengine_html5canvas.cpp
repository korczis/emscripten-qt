#include <QtCore/qglobal.h>

#define QT_FT_BEGIN_HEADER
#define QT_FT_END_HEADER
#define QT_DEBUG_DRAW

#include "qpaintengine_html5canvas_p.h"
#include <qpainterpath.h>
#include <qdebug.h>
#include <qhash.h>
#include <qlabel.h>
#include <qbitmap.h>
#include <qbrush.h>
#include <qmath.h>
#include "painting/html5canvasinterface.h"

#include <private/qmath_p.h>
#include <private/qtextengine_p.h>
#include <private/qfontengine_p.h>
#include <private/qimage_p.h>
#include <private/qstatictext_p.h>
#include <private/qcosmeticstroker_p.h>
#include "qmemrotate_p.h"

#include "qoutlinemapper_p.h"
#include "qpaintengine.h"
#include "../image/qpixmap_html5canvas_p.h"

#if !defined(QT_NO_FREETYPE)
#  include <private/qfontengine_ft_p.h>
#endif
#if !defined(QT_NO_QWS_QPF2)
#  include <private/qfontengine_qpf_p.h>
#endif
#include <private/qabstractfontengine_p.h>
#include <private/qfontengine_ft_p.h>

#include <limits.h>

QT_BEGIN_NAMESPACE

QHtml5CanvasPaintEngine::QHtml5CanvasPaintEngine()
    : QPaintEngineEx(*(new QHtml5CanvasPaintEnginePrivate))
{
}

QHtml5CanvasPaintEngine::QHtml5CanvasPaintEngine(QHtml5CanvasPaintEnginePrivate &dd, QPaintDevice *device)
    : QPaintEngineEx(dd)
{
}

/*!
    Destroys this paint engine.
*/
QHtml5CanvasPaintEngine::~QHtml5CanvasPaintEngine()
{
    Q_D(QHtml5CanvasPaintEngine);
    qDebug() << "QHtml5CanvasPaintEngine::~QHtml5CanvasPaintEngine()";
}

/*!
    \reimp
*/
bool QHtml5CanvasPaintEngine::begin(QPaintDevice *device)
{
    Q_D(QHtml5CanvasPaintEngine);
    printf("QHtml5CanvasPaintEngine::begin(QPaintDevice *device)\n");
    Q_ASSERT(device->devType() == QInternal::Pixmap);
    d->device = device;
    QPixmap *pixmap = static_cast<QPixmap *>(device);
    QPixmapData *pd = pixmap->pixmapData();
    QHtml5CanvasPixmapData *html5CanvasPixmap = static_cast<QHtml5CanvasPixmapData*>(pd);
    d->canvasHandle = html5CanvasPixmap->canvasHandle();
    printf("Restoring to (almost) empty paint stack\n");
    // Keep the first entry, as that represents the default state. TODO - check this;
    // it might be instead that we have to remember the top(), then restore to empty stack,
    // then push the old top() back on again.
    while (!d->savedStateHistory.size() > 1)
    {
        printf(" popped\n");
        Html5CanvasInterface::restorePaintState(d->canvasHandle);
        d->savedStateHistory.pop();
    }
    return true;
}

/*!
    \reimp
*/
bool QHtml5CanvasPaintEngine::end()
{
    qDebug() << "QHtml5CanvasPaintEngine::end()";
    return true;
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::releaseBuffer()
{
    qDebug()<< "QHtml5CanvasPaintEngine::releaseBuffer()";
}

/*!
    \internal
*/
QSize QHtml5CanvasPaintEngine::size() const
{
    Q_D(const QHtml5CanvasPaintEngine);
    qDebug()<< "QHtml5CanvasPaintEngine::size()";
    return QSize();
}

/*!
    \internal
*/
#ifndef QT_NO_DEBUG
void QHtml5CanvasPaintEngine::saveBuffer(const QString &s) const
{
    Q_D(const QHtml5CanvasPaintEngine);
    qDebug()<< "QHtml5CanvasPaintEngine::saveBuffer(const QString &s)";
}
#endif

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::updateMatrix(const QTransform &matrix)
{
    qDebug()<< "QHtml5CanvasPaintEngine::updateMatrix(const QTransform &matrix)";
}



QHtml5CanvasPaintEngineState::~QHtml5CanvasPaintEngineState()
{
    qDebug() << "QHtml5CanvasPaintEngineState::~QHtml5CanvasPaintEngineState()";
}


QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState()
{
    qDebug() << "QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState()";
}

QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState(QHtml5CanvasPaintEngineState &s)
    : QPainterState(s)
{
    qDebug() << "QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState(QHtml5CanvasPaintEngineState &s)";
}

/*!
    \internal
*/
QPainterState *QHtml5CanvasPaintEngine::createState(QPainterState *orig) const
{
    qDebug() << "QHtml5CanvasPaintEngine::createState(QPainterState *orig)";
    if (!orig)
        return new QHtml5CanvasPaintEngineState();
    else
        return new QHtml5CanvasPaintEngineState(*static_cast<QHtml5CanvasPaintEngineState *>(orig));
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::setState(QPainterState *s)
{
    Q_D(QHtml5CanvasPaintEngine);
    printf("QHtml5CanvasPaintEngine::setState(QPainterState *s): %p %d\n", (void*)s, d->canvasHandle);
    // I think setState should only be called at paint begin, or when save()ing or restore()ing a QPainter.
    QPaintEngineEx::setState(s);
    if (d->savedStateHistory.contains(s))
    {
        printf("Restoring\n");
        while (d->savedStateHistory.top() != s)
        {
            printf(" popped\n");
            d->savedStateHistory.pop();
            Html5CanvasInterface::restorePaintState(d->canvasHandle);
        }
    }
    else
    {
        printf("Saving\n");
        d->savedStateHistory.push(s);
        Html5CanvasInterface::savePaintState(d->canvasHandle);
    }
}

/*!
    \fn QHtml5CanvasPaintEngineState *QHtml5CanvasPaintEngine::state()
    \internal
*/

/*!
    \fn const QHtml5CanvasPaintEngineState *QHtml5CanvasPaintEngine::state() const
    \internal
*/

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::penChanged()
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::penChanged():" << state()->pen;
#endif
    const QColor penColor = state()->pen.color();
    Html5CanvasInterface::changePenColor(d->canvasHandle, penColor.red(), penColor.green(), penColor.blue());
    const qreal qtPenThickness = state()->pen.widthF();
    const qreal html5thickness = (qtPenThickness < 0.1) ? 1.0 : qtPenThickness;
    Html5CanvasInterface::changePenThickness(d->canvasHandle, html5thickness);
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::updatePen(const QPen &pen)
{
    qDebug() << "QHtml5CanvasPaintEngine::updatePen(const QPen &pen)";
}



/*!
    \internal
*/
void QHtml5CanvasPaintEngine::brushOriginChanged()
{
    qDebug() << "QHtml5CanvasPaintEngine::brushOriginChanged()";
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::brushChanged()
{
    Q_D(QHtml5CanvasPaintEngine);
    const QColor brushColor = state()->brush.color();
    qDebug() << "QHtml5CanvasPaintEngine::brushChanged(): " << state()->brush;
    Html5CanvasInterface::changeBrushColor(d->canvasHandle, brushColor.red(), brushColor.green(), brushColor.blue());
}




/*!
    \internal
*/
void QHtml5CanvasPaintEngine::updateBrush(const QBrush &brush)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::updateBrush()" << brush;
#endif
}

void QHtml5CanvasPaintEngine::updateState(const QPaintEngineState &state)
{
    qDebug() << "QHtml5CanvasPaintEngine::updateState()";
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::opacityChanged()
{
    qDebug() << "QHtml5CanvasPaintEngine::opacityChanged()";
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::compositionModeChanged()
{
    qDebug() << "QHtml5CanvasPaintEngine::compositionModeChanged()";
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::renderHintsChanged()
{
    qDebug() << "QHtml5CanvasPaintEngine::renderHintsChanged()";
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::transformChanged()
{
    qDebug() << "QHtml5CanvasPaintEngine::transformChanged()";
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::clipEnabledChanged()
{
    qDebug() << "QHtml5CanvasPaintEngine::clipEnabledChanged()";
}

// #define QT_CLIPPING_RATIOS

#ifdef QT_CLIPPING_RATIOS
int rectClips;
int regionClips;
int totalClips;

static void checkClipRatios(QHtml5CanvasPaintEnginePrivate *d)
{
    if (d->clip()->hasRectClip)
        rectClips++;
    if (d->clip()->hasRegionClip)
        regionClips++;
    totalClips++;

    if ((totalClips % 5000) == 0) {
        printf("Clipping ratio: rectangular=%f%%, region=%f%%, complex=%f%%\n",
               rectClips * 100.0 / (qreal) totalClips,
               regionClips * 100.0 / (qreal) totalClips,
               (totalClips - rectClips - regionClips) * 100.0 / (qreal) totalClips);
        totalClips = 0;
        rectClips = 0;
        regionClips = 0;
    }

}
#endif

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::clip(const QVectorPath &path, Qt::ClipOperation op)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::clip(): " << path << op;
#endif
}



/*!
    \internal
*/
void QHtml5CanvasPaintEngine::clip(const QRect &rect, Qt::ClipOperation op)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    printf("QHtml5CanvasPaintEngine::clip(): rect (%d, %d, %d, %d) , %d \n", rect.x(), rect.y(), rect.width(), rect.height(), op);
#endif
    Html5CanvasInterface::setClipRect(d->canvasHandle, rect.x(), rect.y(), rect.width(), rect.height());
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::clip(const QRegion &region, Qt::ClipOperation op)
{
#ifdef QT_DEBUG_DRAW
    printf("QHtml5CanvasPaintEngine::clip(): region bounding rect (%d, %d, %d, %d) , %d \n", region.boundingRect().x(), region.boundingRect().y(), region.boundingRect().width(), region.boundingRect().height(), op);
#endif
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::fillPath(const QPainterPath &path, QSpanData *fillData)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " --- fillPath, bounds=" << path.boundingRect();
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawRects(const QRect *rects, int rectCount)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    printf("QHtml5CanvasPaintEngine::drawRect(), rectCount=%d\n", rectCount);
#endif
    const bool fillRect = state()->brush.style() != Qt::NoBrush;
    for (int i = 0; i < rectCount; i++)
    {
        if (fillRect)
        {
            Html5CanvasInterface::fillRect(d->canvasHandle, rects[i].x(), rects[i].y(), rects[i].width(), rects[i].height());
        }
        Html5CanvasInterface::strokeRect(d->canvasHandle, rects[i].x(), rects[i].y(), rects[i].width(), rects[i].height());
    }
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawRects(const QRectF *rects, int rectCount)
{
#ifdef QT_DEBUG_DRAW
    qDebug(" - QHtml5CanvasPaintEngine::drawRect(QRectF*), rectCount=%d", rectCount);
#endif
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::stroke(const QVectorPath &path, const QPen &pen)
{
    qDebug() << "QHtml5CanvasPaintEngine::stroke(const QVectorPath &path, const QPen &pen)";
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::fill(const QVectorPath &path, const QBrush &brush)
{
    if (path.isEmpty())
        return;
#ifdef QT_DEBUG_DRAW
    QRectF rf = path.controlPointRect();
    qDebug() << "QHtml5CanvasPaintEngine::fill(): "
             << "size=" << path.elementCount()
             << ", hints=" << hex << path.hints()
             << rf << brush;
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::fillRect(const QRectF &r, const QBrush &brush)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::fillRecct(): " << r << brush;
#endif
    if (brush.style() != Qt::SolidPattern)
    {
        qDebug() << "fillRect with non SolidPattern QBrush not yet supported!";
        return;
    }
    Html5CanvasInterface::fillSolidRect(d->canvasHandle, brush.color().red(), brush.color().green(), brush.color().blue(), r.left(), r.top(), r.width(), r.height());
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::fillRect(const QRectF &r, const QColor &color)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::fillRect(): " << r << color;
#endif
    Html5CanvasInterface::fillSolidRect(d->canvasHandle, color.red(), color.green(), color.blue(), r.left(), r.top(), r.width(), r.height());
}

/*!
  \internal
 */
void QHtml5CanvasPaintEngine::fillPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode)
{
    qDebug() << "QHtml5CanvasPaintEngine::fillPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode)";
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode)
{
#ifdef QT_DEBUG_DRAW
    qDebug(" - QHtml5CanvasPaintEngine::drawPolygon(F), pointCount=%d", pointCount);
    for (int i=0; i<pointCount; ++i)
        qDebug() << "   - " << points[i];
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawPolygon(const QPoint *points, int pointCount, PolygonDrawMode mode)
{
#ifdef QT_DEBUG_DRAW
    qDebug(" - QHtml5CanvasPaintEngine::drawPolygon(I), pointCount=%d", pointCount);
    for (int i=0; i<pointCount; ++i)
        qDebug() << "   - " << points[i];
#endif
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::drawPixmap(const QPointF &pos, const QPixmap &pixmap)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawPixmap(), pos=" << pos << " pixmap=" << pixmap.size() << "depth=" << pixmap.depth();
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawPixmap(const QRectF &r, const QPixmap &pixmap, const QRectF &sr)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawPixmap(), r=" << r << " sr=" << sr << " pixmap=" << pixmap.size() << "depth=" << pixmap.depth();
#endif
}

// assumes that rect has positive width and height

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::drawImage(const QPointF &p, const QImage &img)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawImage(), p=" <<  p << " image=" << img.size() << "depth=" << img.depth();
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawImage(const QRectF &r, const QImage &img, const QRectF &sr,
                                   Qt::ImageConversionFlags)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawImage(), r=" << r << " sr=" << sr << " image=" << img.size() << "depth=" << img.depth();
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawTiledPixmap(const QRectF &r, const QPixmap &pixmap, const QPointF &sr)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawTiledPixmap(), r=" << r << "pixmap=" << pixmap.size();
#endif
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::alphaPenBlt(const void* src, int bpl, int depth, int rx,int ry,int w,int h)
{
    qDebug() << "QHtml5CanvasPaintEngine::alphaPenBlt(const void* src, int bpl, int depth, int rx,int ry,int w,int h)";
}

/*!
   \reimp
*/
void QHtml5CanvasPaintEngine::drawStaticTextItem(QStaticTextItem *textItem)
{
    qDebug() << "QHtml5CanvasPaintEngine::drawStaticTextItem(QStaticTextItem *textItem)";
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawTextItem(const QPointF &p, const QTextItem &textItem)
{
#ifdef QT_DEBUG_DRAW
    Q_D(QHtml5CanvasPaintEngine);
    fprintf(stderr," - QHtml5CanvasPaintEngine::drawTextItem(), (%.2f,%.2f), string=%s \n",
           p.x(), p.y(), textItem.text().toLatin1().data()
           );
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawPoints(const QPointF *points, int pointCount)
{
    qDebug() << "QHtml5CanvasPaintEngine::drawPoints(const QPointF *points, int pointCount)";
}


void QHtml5CanvasPaintEngine::drawPoints(const QPoint *points, int pointCount)
{
    qDebug() << "QHtml5CanvasPaintEngine::drawPoints(const QPoint *points, int pointCount)";
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawLines(const QLine *lines, int lineCount)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawLines(QLine*)" << lineCount;
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawLines(const QLineF *lines, int lineCount)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawLines(QLineF *)" << lineCount;
#endif
}


/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawEllipse(const QRectF &rect)
{
    qDebug() << "QHtml5CanvasPaintEngine::drawEllipse(const QRectF &rect)";
}

/*!
    \internal
*/

bool QHtml5CanvasPaintEngine::supportsTransformations(const QFontEngine *fontEngine) const
{
    qDebug() << "QHtml5CanvasPaintEngine::supportsTransformations(const QFontEngine *fontEngine)";
    return true;
}

bool QHtml5CanvasPaintEngine::supportsTransformations(qreal pixelSize, const QTransform &m) const
{
    qDebug() << "QHtml5CanvasPaintEngine::supportsTransformations(qreal pixelSize, const QTransform &m)";
    return true;
}

/*!
    \internal
*/
QPoint QHtml5CanvasPaintEngine::coordinateOffset() const
{
    qDebug() << "QHtml5CanvasPaintEngine::coordinateOffset()";
    return QPoint(0, 0);
}

/*!
    \internal
    Returns the bounding rect of the currently set clip.
*/
QRect QHtml5CanvasPaintEngine::clipBoundingRect() const
{
    qDebug() << "QHtml5CanvasPaintEngine::clipBoundingRect()";
    return QRect();
}

/*!
    \fn void QHtml5CanvasPaintEngine::drawPoints(const QPoint *points, int pointCount)
    \overload

    Draws the first \a pointCount points in the buffer \a points

    The default implementation converts the first \a pointCount QPoints in \a points
    to QPointFs and calls the floating point version of drawPoints.
*/

/*!
    \fn void QHtml5CanvasPaintEngine::drawEllipse(const QRect &rect)
    \overload

    Reimplement this function to draw the largest ellipse that can be
    contained within rectangle \a rect.
*/

QT_END_NAMESPACE

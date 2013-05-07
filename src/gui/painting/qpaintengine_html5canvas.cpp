#include <QtCore/qglobal.h>

#define QT_FT_BEGIN_HEADER
#define QT_FT_END_HEADER

#ifdef EMSCRIPTEN_NATIVE
#define QT_DEBUG_DRAW
#endif

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

#ifndef QT_NO_GRAPHICSSYSTEM_HTML5CANVAS

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT extern QImage qt_imageForBrush(int brushStyle, bool invert);

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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::~QHtml5CanvasPaintEngine()";
#endif
}

/*!
    \reimp
*/
bool QHtml5CanvasPaintEngine::begin(QPaintDevice *device)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    printf("QHtml5CanvasPaintEngine::begin(QPaintDevice *device)\n");
#endif
    Q_ASSERT(device->devType() == QInternal::Pixmap);
    d->device = device;
    QPixmap *pixmap = static_cast<QPixmap *>(device);
    QPixmapData *pd = pixmap->pixmapData();
    QHtml5CanvasPixmapData *html5CanvasPixmap = static_cast<QHtml5CanvasPixmapData*>(pd);
    d->canvasHandle = html5CanvasPixmap->canvasHandle();
#ifdef QT_DEBUG_DRAW
    qDebug() << "Restoring to (almost) empty paint stack canvasHandle: " << d->canvasHandle;
#endif
    // oldInitialState would have been set by QPainter::begin, before QHtml5CanvasPaintEngine::begin() was called;
    // so we need to hang onto it and re-add it to the saved states after wiping the old saved states.
    QPainterState *oldInitialState = d->savedStateHistory.top();
    d->savedStateHistory.clear();
    d->savedStateHistory.push(oldInitialState);
    Html5CanvasInterface::restoreToOriginalState(d->canvasHandle);
    d->isReallyActive = true;
    return true;
}

/*!
    \reimp
*/
bool QHtml5CanvasPaintEngine::end()
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::end()";
#endif
    d->isReallyActive = false;
    return true;
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::releaseBuffer()
{
#ifdef QT_DEBUG_DRAW
    qDebug()<< "QHtml5CanvasPaintEngine::releaseBuffer()";
#endif
}

/*!
    \internal
*/
QSize QHtml5CanvasPaintEngine::size() const
{
    Q_D(const QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug()<< "QHtml5CanvasPaintEngine::size()";
#endif
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
#ifdef QT_DEBUG_DRAW
    qDebug()<< "QHtml5CanvasPaintEngine::updateMatrix(const QTransform &matrix)";
#endif
}



QHtml5CanvasPaintEngineState::~QHtml5CanvasPaintEngineState()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngineState::~QHtml5CanvasPaintEngineState()";
#endif
}


QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState()";
#endif
}

QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState(QHtml5CanvasPaintEngineState &s)
    : QPainterState(s)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngineState::QHtml5CanvasPaintEngineState(QHtml5CanvasPaintEngineState &s)";
#endif
}

/*!
    \internal
*/
QPainterState *QHtml5CanvasPaintEngine::createState(QPainterState *orig) const
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::createState(QPainterState *orig)";
#endif
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

#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::setState(QPainterState *s):" <<  (void*)s << d->canvasHandle;
#endif
    // I think setState should only be called at paint begin, or when save()ing or restore()ing a QPainter.
    QPaintEngineEx::setState(s);
    if (d->savedStateHistory.contains(s))
    {
#ifdef QT_DEBUG_DRAW
        printf("Restoring\n");
#endif
        while (d->savedStateHistory.top() != s)
        {
            d->savedStateHistory.pop();
            if (d->isReallyActive)
            {
                Html5CanvasInterface::restorePaintState(d->canvasHandle);
            }
#ifdef QT_DEBUG_DRAW
            printf(" popped; now %d elements, with %p at top\n", d->savedStateHistory.size(), d->savedStateHistory.top());
#endif
        }
    }
    else
    {
#ifdef QT_DEBUG_DRAW
        qDebug() << "Saving";
#endif
        d->savedStateHistory.push(s);
        if (d->isReallyActive)
        {
            Html5CanvasInterface::savePaintState(d->canvasHandle);
        }
#ifdef QT_DEBUG_DRAW
        qDebug() << "Saved; now " << d->savedStateHistory.size() << " with " << (void*)d->savedStateHistory.top() << "at top";
#endif
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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::updatePen(const QPen &pen)";
#endif
}



/*!
    \internal
*/
void QHtml5CanvasPaintEngine::brushOriginChanged()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::brushOriginChanged()";
#endif
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::brushChanged()
{
    setHtml5Brush(state()->brush);
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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::updateState()";
#endif
}


/*!
    \internal
*/
void QHtml5CanvasPaintEngine::opacityChanged()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::opacityChanged()";
#endif
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::compositionModeChanged()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::compositionModeChanged()";
#endif
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::renderHintsChanged()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::renderHintsChanged()";
#endif
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::transformChanged()
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::transformChanged(): " << state()->transform();
#endif
//     if (state()->transform().isTranslating())
//     {
//         Html5CanvasInterface::translate(d->canvasHandle, state()->transform().dx(),  state()->transform().dy());
//     }
//     if (state()->transform().isRotating())
//     {
//         Html5CanvasInterface::translate(d->canvasHandle, state()->transform().dx(),  state()->transform().dy());
//     }
    // Map Qt's idea of a transform to HTML5's setTransform(a, b, c, d, e , f)'s idea.
    // Note that the HTML5's notion of a transform, unlike Qt's one, does not include scaling,
    // so we need to do that separately via HTML5's "scale" method.

    // Qt:          HTML5:
    // m11 m21 m31  a c e
    // m12 m22 m32  b d f
    // m13 m23  m33  0 0 1
    //   ???
    QTransform qtTransform = state()->transform();
    Q_ASSERT(qtTransform.m33() == 1 && qtTransform.m23() == 0 && qtTransform.m13() == 0); // Not sure how this would be accomplished in HTML5 - might have to use WebGL :/
    double a = qtTransform.m11();
    double b = qtTransform.m12();
    double c = qtTransform.m21();
    double d_ = qtTransform.m22();
    double e = qtTransform.dx(); // The docs state that e & f are equivalent to dx and dy, apparently(?)
    double f = qtTransform.dy();
    Html5CanvasInterface::setTransform(d->canvasHandle, a, b, c, d_, e, f);
}

/*!
    \internal
*/
void QHtml5CanvasPaintEngine::clipEnabledChanged()
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::clipEnabledChanged()";
#endif
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
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    printf("QHtml5CanvasPaintEngine::clip(): region bounding rect (%d, %d, %d, %d) , %d \n", region.boundingRect().x(), region.boundingRect().y(), region.boundingRect().width(), region.boundingRect().height(), op);
#endif
    // It looks like QRegion automatically "rasterises" stuff when it is built - so e.g. a polygon will be
    // broken down into a set of tiny rectangles, whereas we'd *really* like to get access to the original
    // polygon, which would be faster to clip as HTML5 canvas supports this natively.
    // The QPainter docs say:
    //  "Whether paths or regions are preferred (faster) depends on the underlying paintEngine()."
    // It looks like HTML5 canvas prefers paths.
    const int numRects = region.rectCount();
    if (numRects == 1) {
        // Optimise for common, easy case.
        const QRect rect = region.rects()[0];
        Html5CanvasInterface::setClipRect(d->canvasHandle, rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }
    else
    {
        Html5CanvasInterface::beginPath(d->canvasHandle);
        const QVector<QRect> rects = region.rects();
        for (int i = 0; i < numRects; i++)
        {
            Html5CanvasInterface::addRectToCurrentPath(rects[i].x(), rects[i].y(), rects[i].width(), rects[i].height());
        }
        Html5CanvasInterface::setClipToCurrentPath();
    }
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
#ifdef QT_DEBUG_DRAW
            qDebug()<< "QHtml5CanvasPaintEngine:  filling Rect " << (i + 1) << " of " << rectCount << " :" << rects[i];
#endif
            Html5CanvasInterface::fillRect(d->canvasHandle, rects[i].x(), rects[i].y(), rects[i].width(), rects[i].height());
        }
#ifdef QT_DEBUG_DRAW
            qDebug()<< "QHtml5CanvasPaintEngine:  drawing Rect " << (i + 1) << " of " << rectCount << " :" << rects[i];
#endif
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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::stroke(const QVectorPath &path, const QPen &pen)";
#endif
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
    if (path.shape() == QVectorPath::RectangleHint) {
        const qreal *p = path.points();
        QPointF tl = QPointF(p[0], p[1]);
        QPointF br = QPointF(p[4], p[5]);
        fillRect(QRectF(tl, br), brush);
    }
    else
    {
#ifdef QT_DEBUG_DRAW
        qDebug() << "Drawing this QVectorPath not currently supported!";
#endif
    }
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
    const QBrush originalBrush = state()->brush;
    setHtml5Brush(brush);
    Html5CanvasInterface::fillRect(d->canvasHandle, r.left(), r.top(), r.width(), r.height());
    setHtml5Brush(originalBrush);
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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::fillPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode)";
#endif
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
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawPixmap(), pos=" << pos << " pixmap=" << pixmap.size() << "depth=" << pixmap.depth();
#endif
    CanvasHandle pixmapCanvasHandle = static_cast<QHtml5CanvasPixmapData*>(pixmap.pixmapData())->canvasHandle();
    Html5CanvasInterface::drawCanvasOnCanvas(pixmapCanvasHandle, d->canvasHandle, pos.x(), pos.y());
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
    // TODO - we can do better than this! Render pixels direct to canvas rather than wasteful
    // conversion to QPixmap. Need to be careful with clipping if we do this, though, and hard
    // to support palettised QImages.
    QPixmap imageAsPixmap(QPixmap::fromImage(img));
    drawPixmap(p, imageAsPixmap);
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawImage(const QRectF &r, const QImage &img, const QRectF &sr,
                                   Qt::ImageConversionFlags conversionFlags)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawImage(), r=" << r << " sr=" << sr << " image=" << img.size() << "depth=" << img.depth();
#endif
    if (conversionFlags != Qt::AutoColor)
    {
#ifdef QT_DEBUG_DRAW
        qDebug() << "drawImage with conversionFlags " << conversionFlags << "  not yet supported!";
#endif
        return;
    }
    // TODO - we can do better than this! Render pixels direct to canvas rather than wasteful
    // conversion to QPixmap. Need to be careful with clipping if we do this, though, and hard
    // to support palettised QImages.
    QPixmap imageAsPixmap(QPixmap::fromImage(img));
    CanvasHandle pixmapCanvasHandle = static_cast<QHtml5CanvasPixmapData*>(imageAsPixmap.pixmapData())->canvasHandle();
    Html5CanvasInterface::drawStretchedCanvasPortionOnCanvas(pixmapCanvasHandle, d->canvasHandle, r.x(), r.y(), r.width(), r.height(), sr.x(), sr.y(), sr.width(), sr.height());
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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::alphaPenBlt(const void* src, int bpl, int depth, int rx,int ry,int w,int h)";
#endif
}

/*!
   \reimp
*/
void QHtml5CanvasPaintEngine::drawStaticTextItem(QStaticTextItem *textItem)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::drawStaticTextItem(QStaticTextItem *textItem)";
#endif
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
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::drawPoints(const QPointF *points, int pointCount)";
#endif
}


void QHtml5CanvasPaintEngine::drawPoints(const QPoint *points, int pointCount)
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::drawPoints(const QPoint *points, int pointCount)";
#endif
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawLines(const QLine *lines, int lineCount)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawLines(QLine*)" << lineCount;
#endif
    for (int i = 0; i < lineCount; i++)
    {
        Html5CanvasInterface::drawLine(d->canvasHandle, lines[i].x1(), lines[i].y1(), lines[i].x2(), lines[i].y2());
    }
}

/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawLines(const QLineF *lines, int lineCount)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << " - QHtml5CanvasPaintEngine::drawLines(QLineF *)" << lineCount;
#endif
    for (int i = 0; i < lineCount; i++)
    {
        Html5CanvasInterface::drawLine(d->canvasHandle, lines[i].x1(), lines[i].y1(), lines[i].x2(), lines[i].y2());
    }
}


/*!
    \reimp
*/
void QHtml5CanvasPaintEngine::drawEllipse(const QRectF &rect)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::drawEllipse(const QRectF &rect)";
#endif
    Html5CanvasInterface::strokeEllipse(d->canvasHandle, rect.center().x(), rect.center().y(), rect.width(), rect.height());
    Html5CanvasInterface::fillEllipse(d->canvasHandle, rect.center().x(), rect.center().y(), rect.width(), rect.height());
}

/*!
    \internal
*/

bool QHtml5CanvasPaintEngine::supportsTransformations(const QFontEngine *fontEngine) const
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::supportsTransformations(const QFontEngine *fontEngine)";
#endif
    return true;
}

bool QHtml5CanvasPaintEngine::supportsTransformations(qreal pixelSize, const QTransform &m) const
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::supportsTransformations(qreal pixelSize, const QTransform &m)";
#endif
    return true;
}

/*!
    \internal
*/
QPoint QHtml5CanvasPaintEngine::coordinateOffset() const
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::coordinateOffset()";
#endif
    return QPoint(0, 0);
}

/*!
    \internal
    Returns the bounding rect of the currently set clip.
*/
QRect QHtml5CanvasPaintEngine::clipBoundingRect() const
{
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::clipBoundingRect()";
#endif
    return QRect();
}

void QHtml5CanvasPaintEngine::setHtml5Brush(const QBrush& brush)
{
    Q_D(QHtml5CanvasPaintEngine);
#ifdef QT_DEBUG_DRAW
    qDebug() << "QHtml5CanvasPaintEngine::brushChanged(): " << state()->brush;
#endif
    switch(brush.style())
    {
        case Qt::SolidPattern:
        {
            const QColor brushColor = brush.color();
            Html5CanvasInterface::changeBrushColor(d->canvasHandle, brushColor.red(), brushColor.green(), brushColor.blue());
            break;
        }
        case Qt::TexturePattern:
        {
            CanvasHandle textureCanvasHandle = static_cast<QHtml5CanvasPixmapData*>(brush.texture().pixmapData())->canvasHandle();
            Html5CanvasInterface::changeBrushTexture(d->canvasHandle, textureCanvasHandle);
            break;
        }
        case Qt::LinearGradientPattern:
        {
            const QLinearGradient* brushGradient = static_cast<const QLinearGradient*>(brush.gradient());
            Html5CanvasInterface::createLinearGradient(d->canvasHandle, brushGradient->start().x(), brushGradient->start().y(),
                                                       brushGradient->finalStop().x(), brushGradient->finalStop().y());
            QVector<QGradientStop> gradientStops = brushGradient->stops();
            foreach(const QGradientStop& gradientStop, gradientStops)
            {
                const qreal position = gradientStop.first;
                const QColor stopColour = gradientStop.second;
                Html5CanvasInterface::addStopPointToCurrentGradient(position, stopColour.red(), stopColour.green(), stopColour.blue());
            }
            Html5CanvasInterface::setBrushToCurrentGradient(d->canvasHandle);
            break;
        }
        case Qt::Dense1Pattern:
        case Qt::Dense2Pattern:
        case Qt::Dense3Pattern:
        case Qt::Dense4Pattern:
        case Qt::Dense5Pattern:
        case Qt::Dense6Pattern:
        case Qt::Dense7Pattern:
        case Qt::HorPattern:
        case Qt::VerPattern:
        case Qt::CrossPattern:
        case Qt::BDiagPattern:
        case Qt::FDiagPattern:
        case Qt::DiagCrossPattern:
        {
            QImage patternImage = qt_imageForBrush(brush.style(), true);
            patternImage.setColor(0, Qt::transparent);
            patternImage.setColor(1, brush.color().rgba());
            QPixmap patternImageAsPixmap = QPixmap::fromImage(patternImage);
            CanvasHandle patternPixmapHandle = static_cast<QHtml5CanvasPixmapData*>(patternImageAsPixmap.pixmapData())->canvasHandle();
            Html5CanvasInterface::changeBrushTexture(d->canvasHandle, patternPixmapHandle);
            break;
        }
        default:
        {
#ifdef QT_DEBUG_DRAW
            qDebug() << "Brush style " << brush.style() << " currently unsupported";
#endif
        }
    }
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
#endif
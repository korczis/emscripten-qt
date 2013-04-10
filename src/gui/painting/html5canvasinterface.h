#ifndef HTML5CANVAS_INTERFACE_H
#define HTML5CANVAS_INTERFACE_H

#include <QtCore/qglobal.h>
QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

typedef qint32 CanvasHandle;
typedef quint32 Rgba;

class Q_GUI_EXPORT Html5CanvasInterface
{
public:
    /**
     * Get the handle for the Canvas element in the DOM that everything ends up being rendered to.
     * Returns -1 if no such Canvas element cannot be found.
     */
    static CanvasHandle handleForMainCanvas();

    static int mainCanvasWidth();

    static int mainCanvasHeight();

    /**
     * Create a Canvas (offscreen) off the given \a width and \a height, and return a handle to it,
     * or -1 if it could not be created.
     */
    static CanvasHandle createCanvas(int width, int height);
    /**
     * Fill the region specified by \a x, \a y, \a width and \a height with the given rgb values.
     * The canvas state (in particular, the fillStyle) is not affected by this method.
     */
    static void fillSolidRect(CanvasHandle canvasHandle, int r, int g, int b, double x, double y, double width, double height);
    /**
     * Draw the canvas with the requested \a canvasHandle on the main canvas at \a x, \a y
     */
    static void drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y);
    /**
     * Fill the main canvas with the colour \a rgba.  Mainly used by the html5canvas test suite.
     */
    static void clearMainCanvas(Rgba rgba);
};

QT_END_NAMESPACE

QT_END_HEADER

#endif
#ifndef HTML5CANVAS_INTERFACE_H
#define HTML5CANVAS_INTERFACE_H

#include <QtCore/qglobal.h>
/**
 * The native implementation is in emscripten-stuff/html5-graphicssystem-test-harness/html5-graphicssystem-test/
 * for now.
 */
QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class Q_GUI_EXPORT Html5CanvasInterface
{
public:
    /**
     * Get the handle for the Canvas element in the DOM that everything ends up being rendered to.
     * Returns -1 if no such Canvas element cannot be found.
     */
    static qint32 handleForMainCanvas();

    /**
     * Create a Canvas (offscreen) off the given \a width and \a height, and return a handle to it,
     * or -1 if it could not be created.
     */
    static qint32 createCanvas(int width, int height);
    /**
     * Fill the region specified by \a x, \a y, \a width and \a height with the given rgb values.
     * The canvas state (in particular, the fillStyle) is not affected by this method.
     */
    static void fillSolidRect(qint32 canvasHandle, int r, int g, int b, double x, double y, double width, double height);
};

QT_END_NAMESPACE

QT_END_HEADER

#endif
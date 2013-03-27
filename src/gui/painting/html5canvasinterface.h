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
     */
    static qint32 handleForMainCanvas();
};

QT_END_NAMESPACE

QT_END_HEADER

#endif
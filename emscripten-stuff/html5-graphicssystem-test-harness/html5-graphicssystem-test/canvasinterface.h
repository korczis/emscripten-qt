#include <QtCore/QtGlobal>
typedef quint32 Rgba;

class CanvasInterface
{
public:
    static void init();

    static void clearCanvas(Rgba rgba);
    
    /**
     * Caller assumes ownership of the returned array of canvas width * canvas height elements.
     */
    static Rgba* canvasContents();

    static void deInit();
};

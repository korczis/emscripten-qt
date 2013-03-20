#include "../shared/rgba.h"
#include <QtCore/QtGlobal>
#include <QtGui/QImage>

class CanvasInterface
{
public:
    static void init();

    static void clearCanvas(Rgba rgba);
    
    static QImage canvasContents();

    static void deInit();
};

#include "../shared/rgba.h"
#include <QtCore/QtGlobal>
#include <QtGui/QImage>

class CanvasTestInterface
{
public:
    static void init();

    static QImage canvasContents();

    static void deInit();
};

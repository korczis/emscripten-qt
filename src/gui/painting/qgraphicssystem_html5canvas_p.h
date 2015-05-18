#ifndef QGRAPHICSSYSTEM_HTML5CANVAS_P_H
#define QGRAPHICSSYSTEM_HTML5CANVAS_P_H

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

#include "qgraphicssystem_p.h"

#ifndef QT_NO_GRAPHICSSYSTEM_HTML5CANVAS

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QHtml5CanvasGraphicsSystem : public QGraphicsSystem
{
public:

    QHtml5CanvasGraphicsSystem() {};

    virtual QPixmapData *createPixmapData(QPixmapData::PixelType type) const;
    QWindowSurface *createWindowSurface(QWidget *widget) const;

private:
};

QT_END_NAMESPACE

#endif

#endif

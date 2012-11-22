#ifndef QKBDEMSCRIPTENCANVAS_QWS_H
#define QKBDEMSCRIPTENCANVAS_QWS_H

#include <QtGui/qapplication.h>
#include <QtGui/qkbd_qws.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)


class Q_GUI_EXPORT QWSEmscriptenCanvasKeyboardHandler : public QObject, public QWSKeyboardHandler
{
    Q_OBJECT
public:
    QWSEmscriptenCanvasKeyboardHandler(const QString &device);
    ~QWSEmscriptenCanvasKeyboardHandler();
};


QT_END_NAMESPACE

QT_END_HEADER

#endif // QKBDEMSCRIPTENCANVAS_QWS_H



#ifndef QMOUSE_EMSCRIPTENCANVAS_H
#define QMOUSE_EMSCRIPTENCANVAS_H

#include <QtCore/qobject.h>
#include <QtGui/qmouse_qws.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class Q_GUI_EXPORT QEmscriptenCanvasMouseHandler : public QObject, public QWSMouseHandler
{
    Q_OBJECT
public:
    explicit QEmscriptenCanvasMouseHandler(const QString &driver = QString(),
                              const QString &device = QString());
    ~QEmscriptenCanvasMouseHandler();

    void resume();
    void suspend();

    static void canvasMouseChanged(int x, int y, int buttons);
private:
    static QEmscriptenCanvasMouseHandler *m_instance;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QMOUSE_QWS_H

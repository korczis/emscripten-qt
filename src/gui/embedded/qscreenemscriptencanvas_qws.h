#ifndef QSCREENEMSCRIPTENFB_QWS_H
#define QSCREENEMSCRIPTENFB_QWS_H

#include <QtGui/qscreen_qws.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)



class Q_GUI_EXPORT QEmscriptenCanvasScreen : public QScreen
{
public:
    explicit QEmscriptenCanvasScreen(int display_id);
    virtual ~QEmscriptenCanvasScreen();
    virtual bool initDevice();
    virtual bool connect(const QString &displaySpec);
    virtual void disconnect();
    virtual void shutdownDevice();
    virtual void save();
    virtual void restore();
    virtual void setMode(int nw,int nh,int nd);
    virtual void setDirty(const QRect& r);
    virtual void blank(bool);
    virtual void exposeRegion(QRegion r, int changing);
    static void setBrightness(int b);
};


QT_END_NAMESPACE

QT_END_HEADER

#endif // QSCREENEMSCRIPTENFB_QWS_H



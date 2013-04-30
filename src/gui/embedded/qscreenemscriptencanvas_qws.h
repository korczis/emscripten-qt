#ifndef QSCREENEMSCRIPTENFB_QWS_H
#define QSCREENEMSCRIPTENFB_QWS_H

#include <QtGui/qscreen_qws.h>
#include <painting/html5canvasinterface.h>

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
    virtual QWSWindowSurface* createSurface(QWidget *widget) const;
    virtual QWSWindowSurface* createSurface(const QString &key) const;
    static void setBrightness(int b);
private:
#ifndef QT_NO_GRAPHICSSYSTEM_HTML5CANVAS
    bool m_useRaster;
#endif
    CanvasHandle m_mainCanvasHandle;
};


QT_END_NAMESPACE

QT_END_HEADER

#endif // QSCREENEMSCRIPTENFB_QWS_H



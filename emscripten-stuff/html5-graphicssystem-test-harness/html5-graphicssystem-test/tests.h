#include <QtCore/QObject>
#include <QtGui/QImage>

class QPainter;

class Html5GraphicsSystemTests : public QObject
{
Q_OBJECT
public slots:
    void testSanityTest();
public:
    void setExpectedImage(const QImage& expectedImage);
    QImage expectedImage();
    void setPainterForTest(QPainter* painter);
    QPainter *painter();
private:
    QImage m_expectedImage;
    QPainter *m_painter;
};

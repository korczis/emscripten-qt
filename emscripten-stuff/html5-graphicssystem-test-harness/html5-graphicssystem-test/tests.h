#include <QtCore/QObject>
#include <QtGui/QImage>

class Html5GraphicsSystemTests : public QObject
{
Q_OBJECT
public slots:
    void testSanityTest();
public:
    void setExpectedImage(const QImage& expectedImage);
    QImage expectedImage();
private:
    QImage m_expectedImage;
};

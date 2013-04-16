#include <QtCore/QObject>
#include <QtGui/QImage>

class QPainter;

class Html5GraphicsSystemTests : public QObject
{
Q_OBJECT
public:
    Html5GraphicsSystemTests(int widgetWidth, int widgetHeight);
public slots:
    void testSanityTest();
    void testTwoFilledRectangles();
    void testDrawRectangleDefaultPenAndBrush();
    void testDrawRectangleDefaultPenAndBrush2();
    void testDrawRectangleRedPenAndDefaultBrush();
    void testDrawRectangleRedPenAndBlueBrush();
    void testDrawRectangleWithThickLine();
    void testSaveAndRestoreDrawingState();
public:
    void setExpectedImage(const QImage& expectedImage);
    QImage expectedImage();
    void setPainterForTest(QPainter* painter);
    QPainter *painter();
    int widgetWidth();
    int widgetHeight();
private:
    QImage m_expectedImage;
    QPainter *m_painter;
    int m_widgetWidth;
    int m_widgetHeight;
};

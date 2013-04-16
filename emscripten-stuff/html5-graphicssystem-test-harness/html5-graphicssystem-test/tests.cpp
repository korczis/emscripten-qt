#include "tests.h"

#include <QtCore/QDebug>
#include <QtGui/QPainter>

#ifdef EMSCRIPTEN_NATIVE
const QString expectedTestImagesPath = EXPECTED_TEST_IMAGES_DIR "/";
#else
const QString expectedTestImagesPath = "/expected-test-images/";
#endif

Html5GraphicsSystemTests::Html5GraphicsSystemTests(int widgetWidth, int widgetHeight)
    : m_widgetWidth(widgetWidth),
      m_widgetHeight(widgetHeight)
{
}

void Html5GraphicsSystemTests::testSanityTest()
{
    qDebug() << "testSanityTest";
    painter()->fillRect(QRect(0, 0, widgetWidth(), widgetHeight()), QColor(0, 0, 255));
    qDebug() << "testSanityTest complete";
    setExpectedImage(QImage(expectedTestImagesPath + "sanityTestResult.png"));
}

void Html5GraphicsSystemTests::testTwoFilledRectangles()
{
    painter()->fillRect(QRect(10, 10, 20, 30), QColor(0, 0, 255));
    painter()->fillRect(QRect(30, 10, 20, 30), QColor(0, 255, 0));
    setExpectedImage(QImage(expectedTestImagesPath + "twoRectangles.png"));
}

void Html5GraphicsSystemTests::testDrawRectangleDefaultPenAndBrush()
{
    painter()->drawRect(10, 10, widgetWidth() - 20, widgetHeight() - 20);
    setExpectedImage(QImage(expectedTestImagesPath + "drawRectangleDefaultPenAndBrush.png"));
}

void Html5GraphicsSystemTests::testDrawRectangleDefaultPenAndBrush2()
{
    int squareLength = 3;
    while (true)
    {
       const QRect square = QRect(widgetWidth() / 2 - squareLength / 2, widgetHeight() / 2 - squareLength / 2, squareLength, squareLength);
       if (square.intersected(QRect(0, 0, widgetWidth(), widgetHeight())) != square)
       {
           break;
       }
       squareLength += 4; 
       painter()->drawRect(square);
    }
    setExpectedImage(QImage(expectedTestImagesPath + "drawRectangleDefaultPenAndBrush2.png"));
}

void Html5GraphicsSystemTests::testDrawRectangleRedPenAndDefaultBrush()
{
    painter()->setPen(QPen(Qt::red));
    int squareLength = 3;
    while (true)
    {
       const QRect square = QRect(widgetWidth() / 2 - squareLength / 2, widgetHeight() / 2 - squareLength / 2, squareLength, squareLength);
       if (square.intersected(QRect(0, 0, widgetWidth(), widgetHeight())) != square)
       {
           break;
       }
       squareLength += 4; 
       painter()->drawRect(square);
    }
    setExpectedImage(QImage(expectedTestImagesPath + "drawRectangleRedPenAndDefaultBrush.png"));
}

void Html5GraphicsSystemTests::testDrawRectangleRedPenAndBlueBrush()
{
    painter()->setPen(QPen(Qt::red));
    painter()->setBrush(Qt::blue);
    int squareLength = 7;

    for (int y = 0; y < widgetHeight(); y += squareLength + 2)
    {
        for (int x = 0; x < widgetWidth(); x += squareLength + 2)
        {
           const QRect square = QRect(x, y, squareLength, squareLength);
           painter()->drawRect(square);
        }
    }
    setExpectedImage(QImage(expectedTestImagesPath + "drawRectangleRedPenAndBlueBrush.png"));
}
void Html5GraphicsSystemTests::setExpectedImage(const QImage& expectedImage)
{
    m_expectedImage = expectedImage;
}

QImage Html5GraphicsSystemTests::expectedImage()
{
    return m_expectedImage;
}

void Html5GraphicsSystemTests::setPainterForTest(QPainter *painter)
{
    m_painter = painter;
}

QPainter *Html5GraphicsSystemTests::painter()
{
    return m_painter;
}

int Html5GraphicsSystemTests::widgetWidth()
{
    return m_widgetWidth;
}

int Html5GraphicsSystemTests::widgetHeight()
{
    return m_widgetHeight;
}

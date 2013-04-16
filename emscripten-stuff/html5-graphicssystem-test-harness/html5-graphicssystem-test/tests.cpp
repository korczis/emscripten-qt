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
    qDebug() << "Default brush: " << painter()->brush();
    qDebug() << "testSanityTest";
    painter()->fillRect(QRect(0, 0, widgetWidth(), widgetHeight()), QColor(0, 0, 255));
    qDebug() << "testSanityTest complete";
}

void Html5GraphicsSystemTests::testTwoFilledRectangles()
{
    painter()->fillRect(QRect(10, 10, 20, 30), QColor(0, 0, 255));
    painter()->fillRect(QRect(30, 10, 20, 30), QColor(0, 255, 0));
}

void Html5GraphicsSystemTests::testDrawRectangleDefaultPenAndBrush()
{
    painter()->drawRect(10, 10, widgetWidth() - 20, widgetHeight() - 20);
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
}
void Html5GraphicsSystemTests::testDrawRectangleWithThickLine()
{
    const int thickness = 5;
    int squareLength = 11;
    while (true)
    {
        const QRect square(widgetWidth() / 2 - (squareLength / 2), widgetHeight() / 2 - (squareLength / 2), squareLength, squareLength);
        // +/-2 due to thickness of outerSquare itself, plus the white boundary separating square from outer square.
        const QRect outerSquare = square.adjusted(-thickness / 2 - 2, -thickness / 2 - 2, thickness / 2 + 2, thickness / 2 + 2);
        if (outerSquare.intersected(QRect(0, 0, widgetWidth(), widgetHeight())) != outerSquare)
        {
            break;
        }
        painter()->setPen(QPen(Qt::red, 0));
        painter()->drawRect(outerSquare);

        painter()->setPen(QPen(Qt::black, thickness));
        painter()->drawRect(square);
        squareLength += 2 * (2 /* outerSquare, plus white boundary */ + thickness);
    }
}

void Html5GraphicsSystemTests::testSaveAndRestoreDrawingState()
{
    int x = 5;
    const int numRects = 7;
    const int y = 10;
    const int interRectXPad = 5;
    const int width = (widgetWidth() - numRects * interRectXPad) / numRects;
    const int height = (widgetHeight() - y) / 2;

    painter()->save();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->setPen(QPen(Qt::red, 3));
    painter()->setBrush(Qt::blue);
    painter()->save();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->setPen(QPen(Qt::green, 3));
    painter()->setBrush(Qt::yellow);
    painter()->save();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->setPen(QPen(Qt::gray, 3));
    painter()->setBrush(Qt::white);
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->restore();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->restore();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->restore();
    painter()->drawRect(x, y, width, height);

}

void Html5GraphicsSystemTests::testSingleClippedRectangle()
{
    const QRect clipRect(widgetWidth() / 4, widgetHeight() / 4, widgetWidth() / 2, widgetHeight() / 2);
    painter()->setClipRect(clipRect);
    painter()->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(Qt::red));
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

#include "tests.h"

#include <QtCore/QDebug>
#include <QtGui/QPainter>

#ifdef EMSCRIPTEN_NATIVE
const QString expectedTestImagesPath = EXPECTED_TEST_IMAGES_DIR "/";
#else
const QString expectedTestImagesPath = "/expected-test-images/";
#endif
#ifdef EMSCRIPTEN_NATIVE
const QString testDataPath = TEST_DATA_DIR "/";
#else
const QString testDataPath = "/testdata/";
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
    const QRect clipRect(widgetWidth() / 4, widgetHeight() / 8, widgetWidth() / 2, 3 * widgetHeight() / 4);
    painter()->setClipRect(clipRect);
    painter()->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(Qt::green));
}

void Html5GraphicsSystemTests::testTwoClippedRectangles()
{
    const QRect clipRect1(widgetWidth() / 8, widgetHeight() / 8, widgetWidth() / 4, 3 * widgetHeight() / 4);
    const QRect clipRect2(5 * widgetWidth() / 8, widgetHeight() / 8, widgetWidth() / 4, 3 * widgetHeight() / 4);
    painter()->setClipRect(clipRect1);
    painter()->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(Qt::red));

    painter()->setClipRect(clipRect2);
    painter()->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(Qt::blue));
}

void Html5GraphicsSystemTests::testChangingClipDoesntUndoPenAndBrush()
{
    const QRect clipRect1(widgetWidth() / 8, widgetHeight() / 8, widgetWidth() / 4, 3 * widgetHeight() / 4);
    const QRect clipRect2(5 * widgetWidth() / 8, widgetHeight() / 8, widgetWidth() / 4, 3 * widgetHeight() / 4);

    painter()->setPen(QPen(Qt::gray, 10));
    painter()->setBrush(QBrush(Qt::yellow));

    painter()->setClipRect(clipRect1);
    painter()->drawRect(0, widgetHeight() / 4, widgetWidth(), widgetHeight() / 2);

    painter()->setClipRect(clipRect2);
    painter()->drawRect(0, widgetHeight() / 4, widgetWidth(), widgetHeight() / 2);
}

void Html5GraphicsSystemTests::testChangingClipDoesntUndoSubsequentPenAndBrushChanges()
{
    const QRect clipRect1(widgetWidth() / 8, widgetHeight() / 8, widgetWidth() / 4, 3 * widgetHeight() / 4);
    const QRect clipRect2(5 * widgetWidth() / 8, widgetHeight() / 8, widgetWidth() / 4, 3 * widgetHeight() / 4);


    painter()->setClipRect(clipRect1);
    painter()->setPen(QPen(Qt::gray, 10));
    painter()->setBrush(QBrush(Qt::yellow));
    painter()->drawRect(0, widgetHeight() / 4, widgetWidth(), widgetHeight() / 2);

    painter()->setClipRect(clipRect2);
    painter()->drawRect(0, widgetHeight() / 4, widgetWidth(), widgetHeight() / 2);
}

void Html5GraphicsSystemTests::testDrawQPixmapWithVariableAlpha()
{
    const QPixmap pixmap(testDataPath + "qt-logo-variable-alpha.png");
    painter()->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(0, 0, 255));
    painter()->drawPixmap(5, 5, pixmap);
}

void Html5GraphicsSystemTests::testDrawQPixmapWithRGB32Format()
{
    const QPixmap pixmap(testDataPath + "cheese.jpg");
    painter()->fillRect(0, 0, widgetWidth(), widgetHeight(), QColor(0, 0, 255));
    painter()->drawPixmap(5, 5, pixmap);
}


void Html5GraphicsSystemTests::testSaveAndRestoreDrawingStateIncludesClip()
{
    int x = 5;
    const int numRects = 7;
    const int y = widgetHeight() / 6;
    const int interRectXPad = 5;
    const int width = (widgetWidth() - numRects * interRectXPad) / numRects;
    const int height = (widgetHeight() - 2 * y);

    painter()->setClipRect(0, widgetHeight() / 2 - widgetHeight() / 8, widgetWidth(), widgetHeight() / 4);
    painter()->save();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->setPen(QPen(Qt::red, 3));
    painter()->setBrush(Qt::blue);
    painter()->setClipRect(0, widgetHeight() / 2 - widgetHeight() / 6, widgetWidth(), widgetHeight() / 3);
    painter()->drawRect(x, y, width, height);
    painter()->save();

    x += width + 5;
    painter()->setClipRect(0, widgetHeight()  / 2 - widgetHeight() / 4, widgetWidth(), widgetHeight() / 2);
    painter()->setPen(QPen(Qt::green, 3));
    painter()->setBrush(Qt::yellow);
    painter()->save();
    painter()->drawRect(x, y, width, height);

    x += width + 5;
    painter()->setPen(QPen(Qt::gray, 3));
    painter()->setClipRect(0, 0, widgetWidth(), widgetHeight());
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

void Html5GraphicsSystemTests::testCanTranslateQPainter()
{
    painter()->translate(-widgetWidth() / 3, widgetHeight() / 4);
    painter()->setBrush(Qt::yellow);
    painter()->drawRect(widgetWidth() / 2, widgetHeight() / 2, widgetWidth() / 2, widgetHeight() / 2);
}

void Html5GraphicsSystemTests::testSettingClipBeforeTranslateDoesNotAlterClip()
{
    painter()->setBrush(Qt::green);
    painter()->setClipRect(widgetWidth() / 2 - widgetWidth() / 4, widgetHeight() * 6 / 8, widgetWidth() / 4, widgetHeight() / 8);
    painter()->translate(-widgetWidth() / 3, widgetHeight() / 4);
    painter()->drawRect(widgetWidth() / 2, widgetHeight() / 2, widgetWidth() / 2, widgetHeight() / 2);
}

void Html5GraphicsSystemTests::testClippingAfterTranslateGivesTranslatedClip()
{
    const int rectWidth = widgetWidth() / 2;
    const int rectHeight = widgetHeight() / 2;
    painter()->setBrush(Qt::red);
    painter()->translate(-widgetWidth() / 3, widgetHeight() / 4);
    painter()->setClipRect(widgetWidth() / 2, widgetHeight() / 2, rectWidth / 6, rectHeight / 6);
    painter()->drawRect(widgetWidth() / 2, widgetHeight() / 2, rectWidth, rectHeight);
}

void Html5GraphicsSystemTests::testDrawRectangleAfterRotateBySixtyDegreesOnUntranslatedCanvas()
{
    const int rectWidth = widgetWidth() / 2;
    const int rectHeight = widgetHeight() / 2;

    painter()->setBrush(Qt::red);
    painter()->rotate(60.0);
    painter()->drawRect(0, 0, rectWidth, rectHeight);
    // Draw a little green sub-rectangle in the corner, to help us visualise the orientation.
    painter()->setBrush(Qt::green);
    painter()->drawRect(rectWidth - rectWidth / 4, rectHeight - rectHeight / 4, rectWidth / 4, rectHeight / 4);
}

void Html5GraphicsSystemTests::testDrawEllipseNoFill()
{
    const int ellipseWidth = widgetWidth() / 2;
    const int ellipseHeight = widgetHeight() / 2;

    painter()->setPen(QPen(Qt::red, 20));
    painter()->drawEllipse((widgetWidth() - ellipseWidth) / 2, (widgetHeight() - ellipseHeight) / 2, ellipseWidth, ellipseHeight);
}

void Html5GraphicsSystemTests::testDrawEllipse()
{
    const int ellipseWidth = widgetWidth() / 2;
    const int ellipseHeight = widgetHeight() / 2;

    painter()->setPen(QPen(Qt::blue, 20));
    painter()->setBrush(Qt::green);
    painter()->drawEllipse((widgetWidth() - ellipseWidth) / 2, (widgetHeight() - ellipseHeight) / 2, ellipseWidth, ellipseHeight);
}

void Html5GraphicsSystemTests::testFillWithTexturedQBrush()
{
    
    const int numRectsHorizontal = 5;
    const int numRectsVertical = 5;
    const int interRectXPad = 10;
    const int interRectYPad = 10;

    const int rectWidth = (widgetWidth() - numRectsHorizontal * interRectXPad) / numRectsHorizontal;
    const int rectHeight = (widgetHeight() - numRectsVertical * interRectYPad) / numRectsVertical;

    const QPixmap pixmap(testDataPath + "qt-logo-variable-alpha.png");
    painter()->setBrush(QBrush(pixmap));
    

    for (int horiz = 0; horiz < numRectsHorizontal; horiz++)
    {
        for (int vert = 0; vert < numRectsVertical; vert++)
        {
            painter()->drawRect(horiz * (rectWidth + interRectXPad), vert * (rectHeight + interRectYPad), rectWidth, rectHeight);
        }
    }
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

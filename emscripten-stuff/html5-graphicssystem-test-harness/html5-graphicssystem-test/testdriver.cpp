#include "testdriver.h"
#include "tests.h"
#include "testwidget.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QMetaMethod>
#include <QtGui/html5canvasinterface.h>
#include <QtGui/QApplication>
#include <QtGui/emscripten-qt-sdl.h>

double percentagePixelsDifferent(const QImage& image1, const QImage& image2)
{
    if (image1.isNull() != image2.isNull())
    {
        return 100.0;
    }
    Q_ASSERT(image1.size() == image2.size());
    const int totalPixels = image1.width() * image1.height();
    int numDifferentPixels = 0;
    for (int y = 0; y < image1.height(); y++)
    {
        for (int x = 0; x < image1.width(); x++)
        {
            const QColor pixel1 = image1.pixel(x, y);
            const QColor pixel2 = image2.pixel(x, y);
            // We don't care about the alpha channel - it leads to false negatives.
            if (pixel1.rgb() != pixel2.rgb())
            {
                numDifferentPixels++;
            }
        }
    }
    return static_cast<double>(numDifferentPixels) * 100.0 / totalPixels;
}

TestDriver::TestDriver(bool usingHtml5Canvas)
    : QObject(), m_testIndex(0), m_usingHtml5Canvas(usingHtml5Canvas)
{
    m_testWidget = new TestWidget(this);
    m_testWidget->showFullScreen();
    m_tests = new Html5GraphicsSystemTests(m_testWidget->width(), m_testWidget->height());
}

void TestDriver::beginRunAllTestsAsync()
{
    QTimer::singleShot(0, this, SLOT(runNextTest()));
}

void TestDriver::runTestWithPainter(QPainter *painter)
{
    m_tests->setPainterForTest(painter);
    m_currentTestMethod.invoke(m_tests);
}

void TestDriver::runNextTest()
{
    const int nextTestMethodIndex = findNextTestMethodIndex();
    if (nextTestMethodIndex == -1)
    {
        qDebug() << "No more tests found";
        return;
    }

    m_currentTestMethod = m_tests->metaObject()->method(nextTestMethodIndex); 
    const QString currentTestName = QString(m_currentTestMethod.signature()).left(QString(m_currentTestMethod.signature()).indexOf("("));
    qDebug() << "Running test " << currentTestName << "...";
    Html5CanvasInterface::clearMainCanvas(0xFF0000FF);
    m_tests->setExpectedImage(QImage());

    m_testWidget->repaint();

    // The redraw of the widget is instantaneous, but we need to process events so that it is flushed to the screen :/
    while (QApplication::hasPendingEvents())
    {
        QApplication::processEvents();
    }
#ifdef EMSCRIPTEN_NATIVE
    QImage canvasContents = (m_usingHtml5Canvas ? Html5CanvasInterface::mainCanvasContents() : EmscriptenQtSDL::screenAsQImage());
#else
    QImage canvasContents = Html5CanvasInterface::mainCanvasContents();
#endif

    qDebug() << "Expected image size: " << m_tests->expectedImage().size();
    
    // We're not interested in the alpha component, so remove it when comparing the images - it can cause false negatives.
    const double percentageDifference = percentagePixelsDifferent(canvasContents, m_tests->expectedImage());
    if (qAbs(percentageDifference) < 0.1)
    {
        qDebug() << "Test " << currentTestName << " passed";
    }
    else
    {
        qDebug() << "Test " << currentTestName << " failed: " << percentageDifference << " pixels were different";
        canvasContents.save(currentTestName + "-actual.png");
    }

    m_testIndex++;
    QTimer::singleShot(0, this, SLOT(runNextTest()));
}

int TestDriver::findNextTestMethodIndex()
{
    int numTestMethodsFound = 0;
    const QMetaObject *testsMetaObject = m_tests->metaObject();
    for (int methodIndex = testsMetaObject->methodOffset(); methodIndex < testsMetaObject->methodCount(); methodIndex++)
    {
        QMetaMethod method = testsMetaObject->method(methodIndex); 
        if (QString(method.signature()).startsWith("test"))
        {
            if (numTestMethodsFound == m_testIndex)
            {
                return methodIndex;
            }
            numTestMethodsFound++;
        }
    }
    return -1;
}

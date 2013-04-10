#include "testdriver.h"
#include "tests.h"
#include "canvastestinterface.h"
#include "testwidget.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QMetaMethod>
#include <QtGui/html5canvasinterface.h>

TestDriver::TestDriver()
    : QObject(), m_testIndex(0) 
{
    m_testWidget = new TestWidget(this);
    m_testWidget->showFullScreen();
    m_tests = new Html5GraphicsSystemTests(m_testWidget->width(), m_testWidget->height());
}

void TestDriver::beginRunAllTestsAsync()
{
    CanvasTestInterface::init();
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
    Html5CanvasInterface::clearMainCanvas(0xFF0000FF);
    m_tests->setExpectedImage(QImage());

    m_testWidget->repaint();

    QImage canvasContents = CanvasTestInterface::canvasContents();
    canvasContents.save("flibble.png");

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

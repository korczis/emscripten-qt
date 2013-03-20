#include "testdriver.h"
#include "tests.h"
#include "canvastestinterface.h"
#include "testwidget.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QMetaMethod>

TestDriver::TestDriver()
    : QObject(), m_testIndex(0), m_tests(new Html5GraphicsSystemTests)
{
    m_testWidget = new TestWidget();
    m_testWidget->showFullScreen();
}

void TestDriver::beginRunAllTestsAsync()
{
    CanvasTestInterface::init();
    QTimer::singleShot(0, this, SLOT(runNextTest()));
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
    CanvasTestInterface::clearCanvas(0xFF0000FF);
    m_tests->setExpectedImage(QImage());
    m_currentTestMethod.invoke(m_tests);
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

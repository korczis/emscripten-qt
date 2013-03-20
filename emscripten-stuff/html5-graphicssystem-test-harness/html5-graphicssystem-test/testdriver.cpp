#include "testdriver.h"
#include "tests.h"
#include "canvasinterface.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QMetaMethod>

TestDriver::TestDriver()
    : QObject(), m_testIndex(0), m_tests(new Html5GraphicsSystemTests)
{
}

void TestDriver::beginRunAllTestsAsync()
{
    CanvasInterface::init();
    QTimer::singleShot(0, this, SLOT(runNextTest()));
}

void TestDriver::runNextTest()
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
                CanvasInterface::clearCanvas(0xFF0000FF);
                method.invoke(m_tests);
                m_testIndex++;
                QImage canvasContents = CanvasInterface::canvasContents();
                canvasContents.save("flibble.png");

                QTimer::singleShot(0, this, SLOT(runNextTest()));
                return;
            }
            numTestMethodsFound++;
        }
    }
    qDebug() << "No more tests found";
}

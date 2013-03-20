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
                CanvasInterface::clearCanvas(0xFF3344);
                method.invoke(m_tests);
                m_testIndex++;
                Rgba* canvasRgba = CanvasInterface::canvasContents();
                for (int i = 0; i < sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT; i++)
                {
                    qDebug() << (int)((char*)canvasRgba)[i];
                }
                free(canvasRgba);

                QTimer::singleShot(0, this, SLOT(runNextTest()));
                return;
            }
            numTestMethodsFound++;
        }
    }
    qDebug() << "No more tests found";
}

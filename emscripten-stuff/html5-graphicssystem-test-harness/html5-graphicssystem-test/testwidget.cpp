#include "testwidget.h"
#include "testdriver.h"

#include <QtGui/QPainter>

TestWidget::TestWidget(TestDriver *testDriver)
    : m_testDriver(testDriver)
{
}

void TestWidget::paintEvent(QPaintEvent* paintEvent)
{
    QPainter painter(this);
    m_testDriver->runTestWithPainter(&painter);
}

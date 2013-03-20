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
    painter.setBrush(QColor(255, 255, 255));
    painter.fillRect(QRect(0, 0, width(), height()), QColor(255, 255, 255));
    m_testDriver->runTestWithPainter(&painter);
}

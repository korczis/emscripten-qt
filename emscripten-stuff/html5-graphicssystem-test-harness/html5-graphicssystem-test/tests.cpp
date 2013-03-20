#include "tests.h"

#include <QtCore/QDebug>

void Html5GraphicsSystemTests::testSanityTest()
{
    qDebug() << "First test!";    
}

void Html5GraphicsSystemTests::setExpectedImage(const QImage& expectedImage)
{
    m_expectedImage = QImage();
}

QImage Html5GraphicsSystemTests::expectedImage()
{
    return m_expectedImage;
}

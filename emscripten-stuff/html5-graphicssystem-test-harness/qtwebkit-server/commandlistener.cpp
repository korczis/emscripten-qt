#include "commandlistener.h"
#include "../shared/command.h"
#include "../shared/canvasdimensions.h"
#include "../shared/rgba.h"

#include <QtNetwork/QTcpSocket>
#include <QtCore/QTimer>

CommandListener::CommandListener()
    : m_server(new QTcpServer), m_commandSource(NULL)
{
    m_server->listen(QHostAddress::LocalHost, 2222);
    Q_ASSERT(m_server->isListening());
    connect(m_server.data(), SIGNAL(newConnection()), this, SLOT(newConnection()));
}

void CommandListener::newConnection()
{
    Q_ASSERT(!m_commandSource && "Only one connection at a time, please!");
    m_commandSource = m_server->nextPendingConnection();
    Q_ASSERT(m_commandSource);
    qDebug() << "Whoo!" << m_commandSource;
    connect(m_commandSource, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(m_commandSource, SIGNAL(readyRead()), this, SLOT(newCommandIncoming()));
}

void CommandListener::disconnected()
{
    m_commandSource->deleteLater();
    m_commandSource = 0;
}

void CommandListener::newCommandIncoming()
{
    Q_ASSERT(m_commandSource->bytesAvailable() > 0);
    qDebug() << "newCommandIncoming";
    Command command = Command::createFrom(m_commandSource);
    qDebug() << "Fully read command: " << command.commandType();
    switch(command.commandType())
    {
        case Command::ClearCanvas:
            break;
        case Command::GetCanvasPixels:
            const qint64 bytesToWrite = sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT;
            qDebug() << "About to write " << bytesToWrite;
            Rgba* fakeRgba = static_cast<Rgba*>(malloc(bytesToWrite));
            for (int i = 0; i < bytesToWrite; i++)
            {
                ((char*)fakeRgba)[i] = i % 256;
            }
            const qint64 bytesWritten = m_commandSource->write((char*)fakeRgba, bytesToWrite);
            if (bytesWritten != bytesToWrite)
            {
                const bool succeeded = m_commandSource->waitForBytesWritten(-1); 
                Q_ASSERT(succeeded);
            }
            qDebug() << "Wrote " << bytesToWrite;
            break;
    }
    if (m_commandSource->bytesAvailable() > 0)
    {
        // Call ourselves again - we can't depend on readyRead callinng us.
        QTimer::singleShot(0, this, SLOT(newCommandIncoming()));
    }
}

#include "commandlistener.h"

#include <QtNetwork/QTcpSocket>

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
    QDataStream command(m_commandSource);
    while (m_commandSource->bytesAvailable() < sizeof(quint32))
    {
        m_commandSource->waitForReadyRead();
    }
    quint32 commandLength;
    command >> commandLength;
    while (m_commandSource->bytesAvailable() < commandLength)
    {
        m_commandSource->waitForReadyRead();
    }
    // Can now read whole command.
    quint32 commandType, colour;
    command >> commandType;
    command >> colour;
    qDebug() << "commandType: " << commandType << " colour: " << colour;
}

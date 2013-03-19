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
}

void CommandListener::disconnected()
{
    m_commandSource->deleteLater();
    m_commandSource = 0;
}

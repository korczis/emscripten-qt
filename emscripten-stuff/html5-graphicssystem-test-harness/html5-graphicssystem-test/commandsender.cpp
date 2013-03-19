#include "commandsender.h"

#include <QtCore/QDebug>

CommandSender::CommandSender()
    : m_commandServerSocket(NULL)
{
    qDebug() << "created command sender";
    IPaddress ip;
    if(SDLNet_ResolveHost(&ip,"localhost", 2222)==-1) {
        qDebug() << "Could not resolve server" << SDLNet_GetError();
        exit(1);
    }
    m_commandServerSocket = SDLNet_TCP_Open(&ip);
    if (!m_commandServerSocket)
    {
        qDebug() << "Could not connect to server";
        exit(1);
    }
}

void CommandSender::sendCommand(const QByteArray& commandAsBytes)
{
    const int bytesSent = SDLNet_TCP_Send(m_commandServerSocket, (void*)commandAsBytes.data(), commandAsBytes.size());
    Q_ASSERT(bytesSent == commandAsBytes.size());
    qDebug() << "Sent " << bytesSent;
}

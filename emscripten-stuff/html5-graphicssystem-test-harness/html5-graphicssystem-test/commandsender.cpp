#include "commandsender.h"

#include <QtCore/QDebug>

CommandSender::CommandSender()
    : m_commandServerSocket(NULL)
{
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

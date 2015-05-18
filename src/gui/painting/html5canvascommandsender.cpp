#if defined(EMSCRIPTEN_NATIVE) && !defined(QT_NO_GRAPHICSSYSTEM_HTML5CANVAS)
#include "html5canvascommandsender.h"
#include "html5canvascommand.h"

#include <qdebug.h>

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

void CommandSender::sendCommand(const Command& command)
{
    const QByteArray commandAsBytes = command.toData();
    const int bytesSent = SDLNet_TCP_Send(m_commandServerSocket, (void*)commandAsBytes.data(), commandAsBytes.size());
    Q_ASSERT(bytesSent == commandAsBytes.size());
    qDebug() << "Sent " << bytesSent;
}

void *CommandSender::readCommandResponse(const quint32 numBytes)
{
    qDebug() << "readCommandResponse: " << numBytes;
    void* destBuffer = malloc(numBytes);
    // *sigh* the docs for SDLNet_TCP_Recv are wrong: see http://comments.gmane.org/gmane.comp.lib.sdl/357
    int bytesRemaining = numBytes;
    char *destBufferWriter = (char*)destBuffer;
    while (bytesRemaining > 0)
    {
        const int bytesReceived = SDLNet_TCP_Recv(m_commandServerSocket, (void*)destBufferWriter, numBytes);
        bytesRemaining -= bytesReceived;
        destBufferWriter += bytesReceived;
        qDebug() << "Received " << bytesReceived << " bytes (remaining " << bytesRemaining << ")";
        Q_ASSERT(bytesReceived > 0);
    }
    return destBuffer;
}
#endif

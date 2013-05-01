#include <SDL_net.h>
#include <QtCore/QtGlobal>

class QByteArray;
class Command;

class CommandSender
{
public:
    CommandSender();
    void sendCommand(const Command& command);
    void* readCommandResponse(quint32 numBytes);
private:
    TCPsocket m_commandServerSocket;
};

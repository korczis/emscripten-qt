#include <SDL_net.h>

class QByteArray;
class Command;

class CommandSender
{
public:
    CommandSender();
    void sendCommand(const Command& command);
private:
    TCPsocket m_commandServerSocket;
};

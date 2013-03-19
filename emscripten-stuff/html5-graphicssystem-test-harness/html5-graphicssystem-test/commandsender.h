#include <SDL_net.h>

class QByteArray;

class CommandSender
{
public:
    CommandSender();
    void sendCommand(const QByteArray& commandAsBytes);
private:
    TCPsocket m_commandServerSocket;
};

#include <SDL_net.h>

class CommandSender
{
public:
    CommandSender();
private:
    TCPsocket m_commandServerSocket;
};

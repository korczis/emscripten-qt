#include <QtNetwork/QTcpServer>
#include <QtCore/QScopedPointer>

class CommandListener : public QObject
{
Q_OBJECT
public:
    CommandListener();
private:
    QScopedPointer<QTcpServer> m_server;
    QTcpSocket *m_commandSource;
private slots:
    void newConnection();
};

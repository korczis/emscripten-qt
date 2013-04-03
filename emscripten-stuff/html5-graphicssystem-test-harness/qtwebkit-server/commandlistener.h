#include <QtNetwork/QTcpServer>
#include <QtCore/QScopedPointer>

class QWebFrame;

class CommandListener : public QObject
{
Q_OBJECT
public:
    CommandListener(QWebFrame* canvasPageFrame);
private:
    QScopedPointer<QTcpServer> m_server;
    QTcpSocket *m_commandSource;
    QWebFrame *m_canvasPageFrame;

    QVariant evaluateJsStatements(const QString& jsStatements);
private slots:
    void newConnection();
    void disconnected();
    void newCommandIncoming();

};

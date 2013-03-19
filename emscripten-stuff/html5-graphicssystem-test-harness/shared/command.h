class QDataStream;
class QIODevice;
class CommandSender;

#include <QtCore/QByteArray>

class Command
{
public:
    enum CommandType { ClearCanvas };
    Command(CommandType commandType);
    ~Command();
    CommandType commandType() const;
    QDataStream& commandData();

    Command createFrom(QIODevice *commandSource);
    QByteArray toData() const;
private:
    Command(CommandType commandType, const QByteArray& commandData);
    Command(const Command& other);
    
    CommandType m_commandType;
    QDataStream *m_dataStream;
    QByteArray m_data;
};

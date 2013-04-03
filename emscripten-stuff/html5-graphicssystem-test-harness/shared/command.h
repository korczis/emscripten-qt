class QDataStream;
class QIODevice;
class CommandSender;

#include <QtCore/QByteArray>

class Command
{
public:
    enum CommandType { ClearCanvas, GetCanvasPixels, GetHandleForMainCanvas, CreateCanvas, FillSolidRect, DrawCanvasOnMainCanvas};
    Command(CommandType commandType);
    Command(const Command& other);
    ~Command();
    CommandType commandType() const;
    QDataStream& commandData();

    static Command createFrom(QIODevice *commandSource);
    QByteArray toData() const;
private:
    Command(CommandType commandType, const QByteArray& commandData);
    
    CommandType m_commandType;
    QDataStream *m_dataStream;
    QByteArray m_data;
};

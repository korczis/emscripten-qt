class QDataStream;
class QIODevice;
class CommandSender;

#include <QtCore/QByteArray>

class Command
{
public:
    enum CommandType { GetMainCanvasWidth, GetMainCanvasHeight, ClearCanvas, GetCanvasPixels, GetHandleForMainCanvas, CreateCanvas, FillSolidRect, StrokeRect, FillRect, ChangePenColor, ChangePenThickness, ChangeBrushColor, SavePaintState, RestorePaintState, RestoreToOriginalState, SetClipRect, Translate, SetCanvasPixelsRaw, DrawCanvasOnMainCanvas, DrawCanvasOnCanvas};
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

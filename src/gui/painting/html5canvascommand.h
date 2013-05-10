// Needs to be usable by both EMSCRIPTEN_NATIVE and also the qtwebkit-server, which does not
// define either of EMSCRIPTEN or EMSCRIPTEN_NATIVE
#if (defined(EMSCRIPTEN_NATIVE) || !defined(EMSCRIPTEN))  && !defined(QT_NO_GRAPHICSSYSTEM_HTML5CANVAS)
class QDataStream;
class QIODevice;
class CommandSender;

#include <QtCore/QByteArray>

class Command
{
public:
    enum CommandType { GetMainCanvasWidth, GetMainCanvasHeight, ClearCanvas, GetCanvasPixels, GetHandleForMainCanvas, CreateCanvas, FillSolidRect, StrokeRect, FillRect, StrokeEllipse, FillEllipse, DrawLine, ChangePenColor, ChangePenThickness, ChangeBrushColor, ChangeBrushTexture, CreateLinearGradient, AddStopPointToCurrentGradient, SetBrushToCurrentGradient, SavePaintState, RestorePaintState, RestoreToOriginalState, SetClipRect, RemoveClip, BeginPath, CurrentPathMoveTo, CurrentPathLineTo, CurrentPathCubicTo, AddRectToCurrentPath, SetClipToCurrentPath, StrokeCurrentPath, FillCurrentPath, SetTransform, SetCanvasPixelsRaw, DrawCanvasOnMainCanvas, DrawCanvasOnCanvas, DrawStretchedCanvasPortionOnCanvas, ProcessEvents};
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
#endif

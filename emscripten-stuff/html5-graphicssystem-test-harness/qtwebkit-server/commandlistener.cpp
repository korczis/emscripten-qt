#include "commandlistener.h"
#include "../../../src/gui/painting/html5canvascommand.h"
#include "../shared/rgba.h"

#include <QtNetwork/QTcpSocket>
#include <QtWebKit/QWebFrame>
#include <QtCore/QTimer>

typedef qint32 CanvasHandle;
namespace
{
    int canvasWidth = -1;
    int canvasHeight = -1;
}

CommandListener::CommandListener(QWebFrame *canvasPageFrame)
    : m_server(new QTcpServer), m_commandSource(NULL), m_canvasPageFrame(canvasPageFrame)
{
    m_server->listen(QHostAddress::LocalHost, 2222);
    Q_ASSERT(m_server->isListening());
    connect(m_server.data(), SIGNAL(newConnection()), this, SLOT(newConnection()));
    const QVariant canvasWidthQueryResult = evaluateJsStatements("return _EMSCRIPTENQT_mainCanvasWidth_internal();");
    if (canvasWidthQueryResult.canConvert<CanvasHandle>())
    {
        canvasWidth = canvasWidthQueryResult.toInt();
    }
    const QVariant canvasHeightQueryResult = evaluateJsStatements("return _EMSCRIPTENQT_mainCanvasHeight_internal();");
    if (canvasHeightQueryResult.canConvert<CanvasHandle>())
    {
        canvasHeight = canvasHeightQueryResult.toInt();
    }
}

void CommandListener::newConnection()
{
    Q_ASSERT(!m_commandSource && "Only one connection at a time, please!");
    m_commandSource = m_server->nextPendingConnection();
    Q_ASSERT(m_commandSource);
    qDebug() << "Whoo!" << m_commandSource;
    connect(m_commandSource, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(m_commandSource, SIGNAL(readyRead()), this, SLOT(newCommandIncoming()));
}

void CommandListener::disconnected()
{
    qDebug() << "Disconnected";
    m_commandSource->deleteLater();
    m_commandSource = 0;
}

void CommandListener::newCommandIncoming()
{
    qDebug() << "newCommandIncoming";
    while (m_commandSource->bytesAvailable() > 0)
    {
        Command command = Command::createFrom(m_commandSource);
        qDebug() << "Fully read command: " << command.commandType();
        switch(command.commandType())
        {
            case Command::ClearCanvas:
                {
                    Rgba newCanvasColour;
                    command.commandData() >> newCanvasColour;
                    const QString clearCanvasJs = QString("return _EMSCRIPTENNATIVEHELPER_clearCanvas_internal(%1);").arg(newCanvasColour);
                    const QVariant result = evaluateJsStatements(clearCanvasJs);
                    break;
                }
            case Command::GetCanvasPixels:
                {
                    const qint64 bytesToWrite = sizeof(Rgba) * canvasWidth * canvasHeight;
                    qDebug() << "About to write " << bytesToWrite;
                    Rgba* fakeRgba = static_cast<Rgba*>(malloc(bytesToWrite));
                    const QVariant result = evaluateJsStatements("return EMSCRIPTENNATIVEHELPER_canvasPixelsAsRGBAString();");
                    const QString rgbaHexString = result.toString();
                    for (int i = 0; i < bytesToWrite; i++)
                    {
                        ((char*)fakeRgba)[i] = rgbaHexString.mid(i * 2, 2).toInt(NULL, 16); 
                    }
                    const qint64 bytesWritten = m_commandSource->write((char*)fakeRgba, bytesToWrite);
                    if (bytesWritten != bytesToWrite)
                    {
                        const bool succeeded = m_commandSource->waitForBytesWritten(-1); 
                        Q_ASSERT(succeeded);
                    }
                    free(fakeRgba);
                    qDebug() << "Wrote " << bytesToWrite;
                    break;
                }
            case Command::GetHandleForMainCanvas:
                {
                    const QVariant result = evaluateJsStatements("return _EMSCRIPTENQT_handleForMainCanvas_internal();");
                    CanvasHandle handle = -1;
                    if (result.canConvert<CanvasHandle>())
                    {
                        handle = result.toInt();
                    }
                    const qint64 bytesToWrite = sizeof(CanvasHandle);
                    const qint64 bytesWritten = m_commandSource->write((char*)&handle, bytesToWrite);
                    if (bytesWritten != bytesToWrite)
                    {
                        const bool succeeded = m_commandSource->waitForBytesWritten(-1); 
                        Q_ASSERT(succeeded);
                    }
                    qDebug() << "Wrote " << bytesToWrite;
                    break;
                }
            case Command::GetMainCanvasWidth:
                {
                    const QVariant result = evaluateJsStatements("return _EMSCRIPTENQT_mainCanvasWidth_internal();");
                    int width = -1;
                    if (result.canConvert<CanvasHandle>())
                    {
                        width = result.toInt();
                    }
                    const qint64 bytesToWrite = sizeof(int);
                    const qint64 bytesWritten = m_commandSource->write((char*)&width, bytesToWrite);
                    if (bytesWritten != bytesToWrite)
                    {
                        const bool succeeded = m_commandSource->waitForBytesWritten(-1); 
                        Q_ASSERT(succeeded);
                    }
                    qDebug() << "Wrote " << bytesToWrite;
                    break;
                }
            case Command::GetMainCanvasHeight:
                {
                    const QVariant result = evaluateJsStatements("return _EMSCRIPTENQT_mainCanvasHeight_internal();");
                    int height = -1;
                    if (result.canConvert<CanvasHandle>())
                    {
                        height = result.toInt();
                    }
                    const qint64 bytesToWrite = sizeof(int);
                    const qint64 bytesWritten = m_commandSource->write((char*)&height, bytesToWrite);
                    if (bytesWritten != bytesToWrite)
                    {
                        const bool succeeded = m_commandSource->waitForBytesWritten(-1); 
                        Q_ASSERT(succeeded);
                    }
                    qDebug() << "Wrote " << bytesToWrite;
                    break;
                }
            case Command::CreateCanvas:
                {
                    int width, height;
                    command.commandData() >> width >> height;
                    const QVariant result = evaluateJsStatements(QString("return _EMSCRIPTENQT_createCanvas_internal(%1, %2);").arg(width).arg(height));
                    CanvasHandle handle = -1;
                    if (result.canConvert<CanvasHandle>())
                    {
                        handle = result.toInt();
                    }
                    const qint64 bytesToWrite = sizeof(CanvasHandle);
                    const qint64 bytesWritten = m_commandSource->write((char*)&handle, bytesToWrite);
                    if (bytesWritten != bytesToWrite)
                    {
                        const bool succeeded = m_commandSource->waitForBytesWritten(-1); 
                        Q_ASSERT(succeeded);
                    }
                    qDebug() << "Wrote " << bytesToWrite;
                    break;
                }
            case Command::FillSolidRect:
                CanvasHandle canvasHandle;
                int r, g, b;
                double x, y, width, height;
                command.commandData() >> canvasHandle >> r >> g >> b >> x >> y >> width >> height;
                evaluateJsStatements(QString("return _EMSCRIPTENQT_fillSolidRect_internal(%1, %2, %3, %4, %5, %6, %7, %8); ").arg(canvasHandle).arg(r).arg(g).arg(b).arg(x).arg(y).arg(width).arg(height));
                break;
            case Command::StrokeRect:
                {
                    CanvasHandle canvasHandle;
                    double x, y, width, height;
                    command.commandData() >> canvasHandle >> x >> y >> width >> height;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_strokeRect_internal(%1, %2, %3, %4, %5); ").arg(canvasHandle).arg(x).arg(y).arg(width).arg(height));
                    break;
                }
            case Command::FillRect:
                {
                    CanvasHandle canvasHandle;
                    double x, y, width, height;
                    command.commandData() >> canvasHandle >> x >> y >> width >> height;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_fillRect_internal(%1, %2, %3, %4, %5); ").arg(canvasHandle).arg(x).arg(y).arg(width).arg(height));
                    break;
                }
            case Command::StrokeEllipse:
                {
                    CanvasHandle canvasHandle;
                    double cx, cy, width, height;
                    command.commandData() >> canvasHandle >> cx >> cy >> width >> height;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_strokeEllipse_internal(%1, %2, %3, %4, %5); ").arg(canvasHandle).arg(cx).arg(cy).arg(width).arg(height));
                    break;
                }
            case Command::FillEllipse:
                {
                    CanvasHandle canvasHandle;
                    double cx, cy, width, height;
                    command.commandData() >> canvasHandle >> cx >> cy >> width >> height;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_fillEllipse_internal(%1, %2, %3, %4, %5); ").arg(canvasHandle).arg(cx).arg(cy).arg(width).arg(height));
                    break;
                }
            case Command::ChangePenColor:
                {
                    CanvasHandle canvasHandle;
                    int r, g, b;
                    command.commandData() >> canvasHandle >> r >> g >> b;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_changePenColor_internal(%1, %2, %3, %4); ").arg(canvasHandle).arg(r).arg(g).arg(b));
                    break;
                }
            case Command::ChangePenThickness:
                {
                    CanvasHandle canvasHandle;
                    double thickness;
                    command.commandData() >> canvasHandle >> thickness;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_changePenThickness_internal(%1, %2); ").arg(canvasHandle).arg(thickness));
                    break;
                }
            case Command::ChangeBrushColor:
                {
                    CanvasHandle canvasHandle;
                    int r, g, b;
                    command.commandData() >> canvasHandle >> r >> g >> b;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_changeBrushColor_internal(%1, %2, %3, %4); ").arg(canvasHandle).arg(r).arg(g).arg(b));
                    break;
                }
            case Command::ChangeBrushTexture:
                {
                    CanvasHandle canvasHandle, textureHandle;
                    command.commandData() >> canvasHandle >> textureHandle;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_changeBrushTexture_internal(%1, %2); ").arg(canvasHandle).arg(textureHandle));
                    break;
                }
            case Command::SavePaintState:
                {
                    CanvasHandle canvasHandle;
                    command.commandData() >> canvasHandle;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_savePaintState_internal(%1); ").arg(canvasHandle));
                    break;
                }
            case Command::RestorePaintState:
                {
                    CanvasHandle canvasHandle;
                    command.commandData() >> canvasHandle;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_restorePaintState_internal(%1); ").arg(canvasHandle));
                    break;
                }
            case Command::RestoreToOriginalState:
                {
                    CanvasHandle canvasHandle;
                    command.commandData() >> canvasHandle;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_restoreToOriginalState_internal(%1); ").arg(canvasHandle));
                    break;
                }
            case Command::SetClipRect:
                {
                    CanvasHandle canvasHandle;
                    double x, y, width, height;
                    command.commandData() >> canvasHandle >> x >> y >> width >> height;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_setClipRect_internal(%1, %2, %3, %4, %5); ").arg(canvasHandle).arg(x).arg(y).arg(width).arg(height));
                    break;
                }
            case Command::SetTransform:
                {
                    CanvasHandle canvasHandle;
                    double a, b, c, d, e, f;
                    command.commandData() >> canvasHandle >> a >> b >> c >> d >> e >> f;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_setTransform_internal(%1, %2, %3, %4, %5, %6, %7); ").arg(canvasHandle).arg(a).arg(b).arg(c).arg(d).arg(e).arg(f));
                    break;
                }
            case Command::SetCanvasPixelsRaw:
                {
                    CanvasHandle canvasHandle;
                    int width, height;
                    command.commandData() >> canvasHandle >>  width >> height;
                    // width * height * 4 2-digit hex numbers.
                    QString rgbaHexString;
                    for (int i = 0; i < width * height * 4; i++)
                    {
                        QString hex;
                        command.commandData() >> hex;
                        rgbaHexString += hex;
                    }
                    evaluateJsStatements(QString("return EMSCRIPTENNATIVEHELPER_setCanvasPixelsRaw(%1, \"%2\", %3, %4); ").arg(canvasHandle).arg(rgbaHexString).arg(width).arg(height));
                    break;
                }
            case Command::DrawCanvasOnCanvas:
                {   
                    CanvasHandle canvasHandleToDraw, canvasHandleToDrawOn;
                    double x, y;
                    command.commandData() >> canvasHandleToDraw >> canvasHandleToDrawOn >> x >> y;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_drawCanvasOnCanvas_internal(%1, %2, %3, %4); ").arg(canvasHandleToDraw).arg(canvasHandleToDrawOn).arg(x).arg(y));
                    break;
                }
            case Command::DrawStretchedCanvasPortionOnCanvas:
                {   
                    CanvasHandle canvasHandleToDraw, canvasHandleToDrawOn;
                    double targetX, targetY, targetWidth, targetHeight, sourceX, sourceY, sourceWidth, sourceHeight;
                    command.commandData() >> canvasHandleToDraw >> canvasHandleToDrawOn >> targetX >> targetY >> targetWidth >> targetHeight >> sourceX >> sourceY >> sourceWidth >> sourceHeight;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_drawStretchedCanvasPortionOnCanvas_internal(%1, %2, %3, %4, %5, %6, %7, %8, %9, %10); ").arg(canvasHandleToDraw).arg(canvasHandleToDrawOn).arg(targetX).arg(targetY).arg(targetWidth).arg(targetHeight).arg(sourceX).arg(sourceY).arg(sourceWidth).arg(sourceHeight));
                    break;
                }
            case Command::DrawCanvasOnMainCanvas:
                {
                    CanvasHandle canvasHandle;
                    int x, y;
                    command.commandData() >> canvasHandle >> x >> y;
                    evaluateJsStatements(QString("return _EMSCRIPTENQT_drawCanvasOnMainCanvas_internal(%1, %2, %3); ").arg(canvasHandle).arg(x).arg(y));
                    break;
                }
        }
    }
    qDebug() << "Command queue cleared";
}

QVariant CommandListener::evaluateJsStatements(const QString& jsStatements)
{
    qDebug() << "Evaluating js: " + jsStatements;
    const QVariant result = m_canvasPageFrame->evaluateJavaScript(QString("(function() {" + jsStatements + "})();"));
    //qDebug() << "result: " << result;
    QString debugResult;
    QDebug debugResultStream(&debugResult);
    debugResultStream << result;
    const int maxDebugChars = 500;
    if (debugResult.length() > maxDebugChars)
    {
        debugResult = debugResult.mid(0, maxDebugChars) + " ...";
    }
    qDebug() << "result: " << debugResult;
    return result;
}

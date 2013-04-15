#include "commandlistener.h"
#include "../shared/command.h"
#include "../shared/canvasdimensions.h"
#include "../shared/rgba.h"

#include <QtNetwork/QTcpSocket>
#include <QtWebKit/QWebFrame>
#include <QtCore/QTimer>

typedef qint32 CanvasHandle;

CommandListener::CommandListener(QWebFrame *canvasPageFrame)
    : m_server(new QTcpServer), m_commandSource(NULL), m_canvasPageFrame(canvasPageFrame)
{
    m_server->listen(QHostAddress::LocalHost, 2222);
    Q_ASSERT(m_server->isListening());
    connect(m_server.data(), SIGNAL(newConnection()), this, SLOT(newConnection()));
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
    m_commandSource->deleteLater();
    m_commandSource = 0;
}

void CommandListener::newCommandIncoming()
{
    Q_ASSERT(m_commandSource->bytesAvailable() > 0);
    qDebug() << "newCommandIncoming";
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
            const qint64 bytesToWrite = sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT;
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
        case Command::DrawCanvasOnMainCanvas:
        {
            CanvasHandle canvasHandle;
            int x, y;
            command.commandData() >> canvasHandle >> x >> y;
            evaluateJsStatements(QString("return _EMSCRIPTENQT_drawCanvasOnMainCanvas_internal(%1, %2, %3); ").arg(canvasHandle).arg(x).arg(y));
        }
    }
    if (m_commandSource->bytesAvailable() > 0)
    {
        // Call ourselves again - we can't depend on readyRead callinng us.
        QTimer::singleShot(0, this, SLOT(newCommandIncoming()));
    }
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

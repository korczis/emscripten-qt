#include "commandlistener.h"
#include "../shared/command.h"
#include "../shared/canvasdimensions.h"
#include "../shared/rgba.h"

#include <QtNetwork/QTcpSocket>
#include <QtWebKit/QWebFrame>
#include <QtCore/QTimer>

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
            const QString clearCanvasJS = QString("(function() { return EMSCRIPTENNATIVEHELPER_clearCanvas(%1); })();").arg(newCanvasColour);
            qDebug() << "Evaluating JS:" << clearCanvasJS;
            const QVariant result = m_canvasPageFrame->evaluateJavaScript(clearCanvasJS);
            qDebug() << "Result: " << result;
            break;
        }
        case Command::GetCanvasPixels:
            const qint64 bytesToWrite = sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT;
            qDebug() << "About to write " << bytesToWrite;
            Rgba* fakeRgba = static_cast<Rgba*>(malloc(bytesToWrite));
            const QVariant result = m_canvasPageFrame->evaluateJavaScript("(function() { return EMSCRIPTENNATIVEHELPER_canvasPixelsAsRGBAString(); })()");
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
            qDebug() << "Wrote " << bytesToWrite;
            break;
    }
    if (m_commandSource->bytesAvailable() > 0)
    {
        // Call ourselves again - we can't depend on readyRead callinng us.
        QTimer::singleShot(0, this, SLOT(newCommandIncoming()));
    }
}

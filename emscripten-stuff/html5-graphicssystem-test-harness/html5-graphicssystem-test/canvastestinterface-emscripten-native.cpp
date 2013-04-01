#include "canvastestinterface.h"
#include "commandsender.h"
#include "../shared/command.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QDataStream>
#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtGui/QColor>
#include <QtGui/html5canvasinterface.h>

namespace 
{
    CommandSender* commandSender()
    {
        static CommandSender *commandSender = NULL;
        if (!commandSender)
        {
            commandSender = new CommandSender;
        }
        return commandSender;
    }
}
void CanvasTestInterface::init()
{
}

void CanvasTestInterface::clearCanvas(Rgba colour)
{
    Command clearCanvasCommand(Command::ClearCanvas);
    clearCanvasCommand.commandData() << colour;
    commandSender()->sendCommand(clearCanvasCommand);
}

QImage CanvasTestInterface::canvasContents()
{
    Command getCanvasPixelsCommand(Command::GetCanvasPixels);
    commandSender()->sendCommand(getCanvasPixelsCommand);
    Rgba* rbga = static_cast<Rgba*>(commandSender()->readCommandResponse(sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT));
    
    QImage canvasContentsImage(CANVAS_WIDTH, CANVAS_HEIGHT, QImage::Format_ARGB32);
    for (int y = 0; y < CANVAS_HEIGHT; y++)
    {
        for (int x = 0; x < CANVAS_WIDTH; x++)
        {
            const Rgba pixelRgba = rbga[x + y * CANVAS_WIDTH];
            canvasContentsImage.setPixel(x, y, QColor(pixelRgba >> 24, (pixelRgba & 0xFF0000) >> 16, (pixelRgba & 0xFF00) >> 8, (pixelRgba & 0xFF) >> 0).rgba());
        }
    }
    return canvasContentsImage;
}

void CanvasTestInterface::deInit()
{
}

qint32 Html5CanvasInterface::handleForMainCanvas()
{
    Command getHandleForMainCanvas(Command::GetHandleForMainCanvas);
    commandSender()->sendCommand(getHandleForMainCanvas);
    qint32 *handlePtr = static_cast<qint32*>(commandSender()->readCommandResponse(sizeof(qint32)));
    const qint32 handle = *handlePtr;
    free(handlePtr);
    return handle;
}

qint32 Html5CanvasInterface::createCanvas(int width, int height)
{
    Command createCanvasCommand(Command::CreateCanvas);
    createCanvasCommand.commandData() << width << height;
    commandSender()->sendCommand(createCanvasCommand);
    qint32 *handlePtr = static_cast<qint32*>(commandSender()->readCommandResponse(sizeof(qint32)));
    const qint32 handle = *handlePtr;
    free(handlePtr);
    return handle;
}

#include "canvastestinterface.h"
#include "commandsender.h"
#include "../shared/command.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QDataStream>
#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtGui/QColor>

namespace 
{
    CommandSender *commandSender = NULL;
}
void CanvasTestInterface::init()
{
    commandSender = new CommandSender;
}

void CanvasTestInterface::clearCanvas(Rgba colour)
{
    Command clearCanvasCommand(Command::ClearCanvas);
    clearCanvasCommand.commandData() << colour;
    commandSender->sendCommand(clearCanvasCommand);
}

QImage CanvasTestInterface::canvasContents()
{
    Command getCanvasPixelsCommand(Command::GetCanvasPixels);
    commandSender->sendCommand(getCanvasPixelsCommand);
    Rgba* rbga = static_cast<Rgba*>(commandSender->readCommandResponse(sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT));
    
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
    delete commandSender;
    commandSender = NULL;
}

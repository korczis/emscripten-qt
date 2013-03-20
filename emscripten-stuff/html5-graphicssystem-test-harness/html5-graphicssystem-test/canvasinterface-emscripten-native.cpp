#include "canvasinterface.h"
#include "commandsender.h"
#include "../shared/command.h"
#include "../shared/canvasdimensions.h"

#include <QtCore/QDataStream>
#include <QtCore/QBuffer>
#include <QtCore/QDebug>

namespace 
{
    CommandSender *commandSender = NULL;
}
void CanvasInterface::init()
{
    commandSender = new CommandSender;
}

void CanvasInterface::clearCanvas(Rgba colour)
{
    Command clearCanvasCommand(Command::ClearCanvas);
    clearCanvasCommand.commandData() << colour;
    commandSender->sendCommand(clearCanvasCommand);
}

Rgba* CanvasInterface::canvasContents()
{
    Command getCanvasPixelsCommand(Command::GetCanvasPixels);
    commandSender->sendCommand(getCanvasPixelsCommand);
    return static_cast<Rgba*>(commandSender->readCommandResponse(sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT));
}

void CanvasInterface::deInit()
{
    delete commandSender;
    commandSender = NULL;
}

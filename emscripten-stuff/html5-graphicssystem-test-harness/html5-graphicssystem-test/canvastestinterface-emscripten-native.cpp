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


Rgba* Html5CanvasInterface::mainCanvasContentsRaw()
{
    Command getCanvasPixelsCommand(Command::GetCanvasPixels);
    commandSender()->sendCommand(getCanvasPixelsCommand);
    Rgba* rbga = static_cast<Rgba*>(commandSender()->readCommandResponse(sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT));
    return rbga;
}

void CanvasTestInterface::deInit()
{
}

void Html5CanvasInterface::clearMainCanvas(Rgba colour)
{
    Command clearCanvasCommand(Command::ClearCanvas);
    clearCanvasCommand.commandData() << colour;
    commandSender()->sendCommand(clearCanvasCommand);
}

CanvasHandle Html5CanvasInterface::handleForMainCanvas()
{
    Command getHandleForMainCanvas(Command::GetHandleForMainCanvas);
    commandSender()->sendCommand(getHandleForMainCanvas);
    CanvasHandle *handlePtr = static_cast<CanvasHandle*>(commandSender()->readCommandResponse(sizeof(CanvasHandle)));
    const CanvasHandle handle = *handlePtr;
    free(handlePtr);
    return handle;
}

int Html5CanvasInterface::mainCanvasWidth()
{
    Command getWidthOfMainCanvas(Command::GetMainCanvasWidth);
    commandSender()->sendCommand(getWidthOfMainCanvas);
    CanvasHandle *widthPtr = static_cast<int*>(commandSender()->readCommandResponse(sizeof(int)));
    const int width = *widthPtr;
    free(widthPtr);
    return width;
}

int Html5CanvasInterface::mainCanvasHeight()
{
    Command getHeightOfMainCanvas(Command::GetMainCanvasHeight);
    commandSender()->sendCommand(getHeightOfMainCanvas);
    CanvasHandle *heightPtr = static_cast<int*>(commandSender()->readCommandResponse(sizeof(int)));
    const int height = *heightPtr;
    free(heightPtr);
    return height;
}

CanvasHandle Html5CanvasInterface::createCanvas(int width, int height)
{
    Command createCanvasCommand(Command::CreateCanvas);
    createCanvasCommand.commandData() << width << height;
    commandSender()->sendCommand(createCanvasCommand);
    CanvasHandle *handlePtr = static_cast<CanvasHandle*>(commandSender()->readCommandResponse(sizeof(CanvasHandle)));
    const CanvasHandle handle = *handlePtr;
    free(handlePtr);
    return handle;
}

void Html5CanvasInterface::fillSolidRect(CanvasHandle canvasHandle, int r, int g, int b, double x, double y, double width, double height)
{
    Command fillSolidRectCommand(Command::FillSolidRect);
    fillSolidRectCommand.commandData() << canvasHandle << r << g << b << x << y << width << height;
    commandSender()->sendCommand(fillSolidRectCommand);
}

void Html5CanvasInterface::drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y)
{
    Command drawCanvasOnMainCanvasCommand(Command::DrawCanvasOnMainCanvas);
    drawCanvasOnMainCanvasCommand.commandData() << canvasHandle << x << y;
    commandSender()->sendCommand(drawCanvasOnMainCanvasCommand);
}


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

Rgba* Html5CanvasInterface::mainCanvasContentsRaw()
{
    Command getCanvasPixelsCommand(Command::GetCanvasPixels);
    commandSender()->sendCommand(getCanvasPixelsCommand);
    Rgba* rbga = static_cast<Rgba*>(commandSender()->readCommandResponse(sizeof(Rgba) * CANVAS_WIDTH * CANVAS_HEIGHT));
    return rbga;
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

void Html5CanvasInterface::strokeRect(CanvasHandle canvasHandle, double x, double y, double width, double height)
{
    Command strokeRectCommand(Command::StrokeRect);
    strokeRectCommand.commandData() << canvasHandle << x << y << width << height;
    commandSender()->sendCommand(strokeRectCommand);
}

void Html5CanvasInterface::fillRect(CanvasHandle canvasHandle, double x, double y, double width, double height)
{
    Command fillRectCommand(Command::FillRect);
    fillRectCommand.commandData() << canvasHandle << x << y << width << height;
    commandSender()->sendCommand(fillRectCommand);
}

void Html5CanvasInterface::changePenColor(CanvasHandle canvasHandle, int r, int g, int b)
{
    Command changePenColorCommand(Command::ChangePenColor);
    changePenColorCommand.commandData() << canvasHandle << r << g << b;
    commandSender()->sendCommand(changePenColorCommand);
}

void Html5CanvasInterface::changePenThickness(CanvasHandle canvasHandle, double thickness)
{
    Command changePenThicknessCommand(Command::ChangePenThickness);
    changePenThicknessCommand.commandData() << canvasHandle << thickness;
    commandSender()->sendCommand(changePenThicknessCommand);
}

void Html5CanvasInterface::changeBrushColor(CanvasHandle canvasHandle, int r, int g, int b)
{
    Command changeBrushColorCommand(Command::ChangeBrushColor);
    changeBrushColorCommand.commandData() << canvasHandle << r << g << b;
    commandSender()->sendCommand(changeBrushColorCommand);
}

void Html5CanvasInterface::savePaintState(CanvasHandle canvasHandle)
{
    Command savePaintStateCommand(Command::SavePaintState);
    savePaintStateCommand.commandData() << canvasHandle;
    commandSender()->sendCommand(savePaintStateCommand);
}

void Html5CanvasInterface::restorePaintState(CanvasHandle canvasHandle)
{
    Command restorePaintStateCommand(Command::RestorePaintState);
    restorePaintStateCommand.commandData() << canvasHandle;
    commandSender()->sendCommand(restorePaintStateCommand);
}

void Html5CanvasInterface::restoreToOriginalState(CanvasHandle canvasHandle)
{
    Command restoreToOriginalStateCommand(Command::RestoreToOriginalState);
    restoreToOriginalStateCommand.commandData() << canvasHandle;
    commandSender()->sendCommand(restoreToOriginalStateCommand);
}

void Html5CanvasInterface::setClipRect(CanvasHandle canvasHandle, double x, double y, double width, double height)
{
    Command setClipRectCommand(Command::SetClipRect);
    setClipRectCommand.commandData() << canvasHandle << x << y << width << height;
    commandSender()->sendCommand(setClipRectCommand);
}

void Html5CanvasInterface::setCanvasPixelsRaw(CanvasHandle canvasHandle, uchar* rgbaData, int width, int height)
{
    Command setCanvasPixelsRawCommand(Command::SetCanvasPixelsRaw);
    setCanvasPixelsRawCommand.commandData() << canvasHandle << width << height;
    // Serialise as width * height * 4 2-digit hex numbers.
    for (int i = 0; i < width * height * 4; i++)
    {
        QString hex = QString::number((int)(rgbaData[i]), 16);
        if (hex.length() == 1)
        {
            hex = "0" + hex;
        }
        setCanvasPixelsRawCommand.commandData() << hex;
    }
    commandSender()->sendCommand(setCanvasPixelsRawCommand);
}

void Html5CanvasInterface::drawCanvasOnCanvas(CanvasHandle canvasHandleToDraw, CanvasHandle canvasHandleToDrawOn, double x, double y)
{
    Command drawCanvasOnCanvasCommand(Command::DrawCanvasOnCanvas);
    drawCanvasOnCanvasCommand.commandData() << canvasHandleToDraw << canvasHandleToDrawOn << x << y;
    commandSender()->sendCommand(drawCanvasOnCanvasCommand);
}

void Html5CanvasInterface::drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y)
{
    Command drawCanvasOnMainCanvasCommand(Command::DrawCanvasOnMainCanvas);
    drawCanvasOnMainCanvasCommand.commandData() << canvasHandle << x << y;
    commandSender()->sendCommand(drawCanvasOnMainCanvasCommand);
}


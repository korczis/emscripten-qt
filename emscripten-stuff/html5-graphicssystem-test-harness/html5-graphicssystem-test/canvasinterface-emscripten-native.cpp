#include "canvasinterface.h"
#include "commandsender.h"

namespace 
{
    CommandSender *commandSender = new CommandSender;
}
void CanvasInterface::init()
{
    commandSender = new CommandSender;
}

void CanvasInterface::clearCanvas(Rgba colour)
{
}

Rgba* CanvasInterface::canvasContents()
{
    return NULL;
}

void CanvasInterface::deInit()
{
    delete commandSender;
    commandSender = NULL;
}

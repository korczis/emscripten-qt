#include "canvasinterface.h"
#include "commandsender.h"
#include "../shared/command.h"

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
    //QByteArray commandData;
    //QDataStream commandStream(&commandData, QIODevice::WriteOnly);
    //commandStream << (quint32)8;
    //commandStream << (quint32)0;
    //commandStream << (quint32)colour;
    //commandBuffer.close();
    //qDebug() << "Command consists of " << commandData.size() << " bytes";
    //commandSender->sendCommand(commandData);
    
    Command clearCanvasCommand(Command::ClearCanvas);
    clearCanvasCommand.commandData() << colour;
    commandSender->sendCommand(clearCanvasCommand);
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

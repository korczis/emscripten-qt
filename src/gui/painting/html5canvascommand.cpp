#include "html5canvascommand.h"

#include <QtCore/QIODevice>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>

Command::Command(CommandType commandType)
    : m_commandType(commandType), m_dataStream(NULL)
{
    m_dataStream = new QDataStream(&m_data, QIODevice::WriteOnly);
}

Command::Command(CommandType commandType, const QByteArray& data)
    : m_commandType(commandType), m_data(data), m_dataStream(NULL)
{
    m_dataStream = new QDataStream(&m_data, QIODevice::ReadOnly);
}

Command::~Command()
{
    delete m_dataStream;
}

Command::CommandType Command::commandType() const
{
    return m_commandType;
}

Command::Command(const Command& other)
    : m_commandType(other.m_commandType), m_data(other.m_data), m_dataStream(NULL)
{
    m_dataStream = new QDataStream(&m_data, QIODevice::ReadOnly);
}

QDataStream& Command::commandData()
{
    return *m_dataStream;
}

Command Command::createFrom(QIODevice* commandSource)
{
    QDataStream commandDataStream(commandSource);
    while (commandSource->bytesAvailable() < sizeof(quint32))
    {
        commandSource->waitForReadyRead(-1);
    }
    quint32 commandLength;
    commandDataStream >> commandLength;
    while (commandSource->bytesAvailable() < commandLength)
    {
        qDebug() << "bytesAvailable: " << commandSource->bytesAvailable() << " commandLength: " << commandLength;
        commandSource->waitForReadyRead(-1);
    }
    quint32 commandType;
    commandDataStream >> commandType;

    QByteArray commandData(commandLength - sizeof(quint32), '\0');
    commandSource->read(commandData.data(), commandData.size());

    return Command(static_cast<Command::CommandType>(commandType), commandData);
}

QByteArray Command::toData() const
{
    QByteArray fullCommandData;
    QDataStream fullCommandDataStream(&fullCommandData, QIODevice::WriteOnly);
    fullCommandDataStream << static_cast<quint32>(m_data.size() + sizeof(quint32));
    fullCommandDataStream << static_cast<quint32>(m_commandType);

    return fullCommandData.append(m_data);
}

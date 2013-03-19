#include "command.h"

#include <QtCore/QIODevice>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>

Command::Command(CommandType commandType)
    : m_commandType(commandType), m_dataStream(NULL)
{
    m_dataStream = new QDataStream(&m_data, QIODevice::WriteOnly);
}

Command::~Command()
{
    delete m_dataStream;
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
    return Command(ClearCanvas);
}

QByteArray Command::toData() const
{
    QByteArray fullCommandData;
    QDataStream fullCommandDataStream(&fullCommandData, QIODevice::WriteOnly);
    fullCommandDataStream << static_cast<quint32>(m_data.size() + sizeof(quint32));
    fullCommandDataStream << static_cast<quint32>(m_commandType);
    
    qDebug() << "command data size:"  << m_data.size();
    
    return fullCommandData.append(m_data);
}

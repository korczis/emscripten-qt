#include "qeventdispatcher_emscripten_p.h"
#include "qdebug.h"

QEventDispatcherEmscripten::QEventDispatcherEmscripten(QObject *parent)
{

}
QEventDispatcherEmscripten::~QEventDispatcherEmscripten()
{

}

bool QEventDispatcherEmscripten::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    qDebug() << "QEventDispatcherEmscripten::processEvents called";
    return false;
}
bool QEventDispatcherEmscripten::hasPendingEvents()
{
    qDebug() << "QEventDispatcherEmscripten::hasPendingEvents called";
    return false;
}

void QEventDispatcherEmscripten::registerSocketNotifier(QSocketNotifier *notifier)
{
    qDebug() << "QEventDispatcherEmscripten::registerSocketNotifier called";
}
void QEventDispatcherEmscripten::unregisterSocketNotifier(QSocketNotifier *notifier)
{
    qDebug() << "QEventDispatcherEmscripten::unregisterSocketNotifier called";
}

void QEventDispatcherEmscripten::registerTimer(int timerId, int interval, QObject *object)
{
    qDebug() << "QEventDispatcherEmscripten::registerTimer called";
}
bool QEventDispatcherEmscripten::unregisterTimer(int timerId)
{
    qDebug() << "QEventDispatcherEmscripten::unregisterTimer called";
    return false;
}
bool QEventDispatcherEmscripten::unregisterTimers(QObject *object)
{
    qDebug() << "QEventDispatcherEmscripten::unregisterTimers called";
    return false;
}
QList<QEventDispatcherEmscripten::TimerInfo> QEventDispatcherEmscripten::registeredTimers(QObject *object) const
{
    qDebug() << "QEventDispatcherEmscripten::registeredTimers called";
    return QList<TimerInfo>();

}

void QEventDispatcherEmscripten::wakeUp()
{
    qDebug() << "QEventDispatcherEmscripten::wakeUp called";
}
void QEventDispatcherEmscripten::interrupt()
{
    qDebug() << "QEventDispatcherEmscripten::interrupt called";
}
void QEventDispatcherEmscripten::flush()
{
    qDebug() << "QEventDispatcherEmscripten::flush called";
}
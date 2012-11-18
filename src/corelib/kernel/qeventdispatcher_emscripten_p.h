#ifndef QEVENTDISPATCHER_EMSCRIPTEN_P_H
#define QEVENTDISPATCHER_EMSCRIPTEN_P_H

#include "QtCore/qabstracteventdispatcher.h"
#include "private/qabstracteventdispatcher_p.h"

class Q_CORE_EXPORT QEventDispatcherEmscripten : public QAbstractEventDispatcher
{
    Q_OBJECT
public:
    explicit QEventDispatcherEmscripten(QObject *parent = 0);
    ~QEventDispatcherEmscripten();

    bool processEvents(QEventLoop::ProcessEventsFlags flags);
    bool hasPendingEvents();

    void registerSocketNotifier(QSocketNotifier *notifier);
    void unregisterSocketNotifier(QSocketNotifier *notifier);

    void registerTimer(int timerId, int interval, QObject *object);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(QObject *object);
    QList< TimerInfo > registeredTimers(QObject* object) const;

    void wakeUp();
    void interrupt();
    void flush();
};
#endif
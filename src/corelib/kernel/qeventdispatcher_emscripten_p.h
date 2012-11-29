#ifndef QEVENTDISPATCHER_EMSCRIPTEN_P_H
#define QEVENTDISPATCHER_EMSCRIPTEN_P_H

#include "QtCore/qabstracteventdispatcher.h"
#include "private/qabstracteventdispatcher_p.h"
#include <sys/time.h>
#include <time.h>

// internal timer info
struct QTimerInfo {
    int id;           // - timer identifier
    timeval interval; // - timer interval
    timeval timeout;  // - when to sent event
    QObject *obj;     // - object to receive event
    QTimerInfo **activateRef; // - ref from activateTimers
};

class QTimerInfoList : public QList<QTimerInfo*>
{
    #if ((_POSIX_MONOTONIC_CLOCK-0 <= 0) && !defined(Q_OS_MAC)) || defined(QT_BOOTSTRAPPED)
    timeval previousTime;
    clock_t previousTicks;
    int ticksPerSecond;
    int msPerTick;

    bool timeChanged(timeval *delta);
    #endif

    // state variables used by activateTimers()
    QTimerInfo *firstTimerInfo;

public:
    QTimerInfoList();

    timeval currentTime;
    timeval updateCurrentTime();

    // must call updateCurrentTime() first!
    void repairTimersIfNeeded();

    bool timerWait(timeval &);
    void timerInsert(QTimerInfo *);
    void timerRepair(const timeval &);

    void registerTimer(int timerId, int interval, QObject *object);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(QObject *object);
    QList<QPair<int, int> > registeredTimers(QObject *object) const;

    int activateTimers();
};

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

    void startingUp();
    void closingDown();

    void interrupt();
    void flush();

    static void emscriptenCallback();
    static void batchProcessEventsAndScheduleNextCallback();
    static void setBatchProcessingEvents() { m_instance->m_batchProcessingEvents = true; };
private:
    QTimerInfoList timerList;
    static QEventDispatcherEmscripten *m_instance;
    bool m_batchProcessingEvents;

    void processEmscriptenCallback();
    void batchProcessEvents();
    void scheduleNextCallback();
};
#endif
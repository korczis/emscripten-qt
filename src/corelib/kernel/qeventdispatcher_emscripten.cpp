#include "qeventdispatcher_emscripten_p.h"
#include "qcore_unix_p.h"
#include "qdebug.h"
#include "qcoreapplication.h"
#include "private/qcoreapplication_p.h"
#include "qelapsedtimer.h"
#include "private/qthread_p.h"
#include <sys/times.h>

Q_CORE_EXPORT bool qt_disable_lowpriority_timers=false;

inline QDebug operator<<(QDebug debug, timeval time)
{
    debug << time.tv_sec << "." << time.tv_usec;
    return debug;
}

extern "C"
{
    void EMSCRIPTENQT_resetTimerCallback(long milliseconds);
    void EMSCRIPTENQT_timerCallback() __attribute__((used))  ;
    void EMSCRIPTENQT_timerCallback()
    {
        QEventDispatcherEmscripten::emscriptenCallback();
    }
#ifdef QT_EMSCRIPTEN_NATIVE
    void EMSCRIPTENQT_resetTimerCallback(long milliseconds)
    {
    }
#endif
}

QEventDispatcherEmscripten* QEventDispatcherEmscripten::m_instance = NULL;

static inline void do_gettime(qint64 *sec, qint64 *frac)
{
    // use gettimeofday
    timeval tv;
    ::gettimeofday(&tv, 0);
    *sec = tv.tv_sec;
    *frac = tv.tv_usec;
}

QTimerInfoList::QTimerInfoList()
{
    #if (_POSIX_MONOTONIC_CLOCK-0 <= 0) && !defined(Q_OS_MAC) && !defined(Q_OS_NACL)
    if (!QElapsedTimer::isMonotonic()) {
        // not using monotonic timers, initialize the timeChanged() machinery
        previousTime = qt_gettime();

        tms unused;
        previousTicks = times(&unused);

        ticksPerSecond = sysconf(_SC_CLK_TCK);
        msPerTick = 1000/ticksPerSecond;
    } else {
        // detected monotonic timers
        previousTime.tv_sec = previousTime.tv_usec = 0;
        previousTicks = 0;
        ticksPerSecond = 0;
        msPerTick = 0;
    }
    #endif

    firstTimerInfo = 0;
}

timeval QTimerInfoList::updateCurrentTime()
{
    return (currentTime = qt_gettime());
}

#if ((_POSIX_MONOTONIC_CLOCK-0 <= 0) && !defined(Q_OS_MAC) && !defined(Q_OS_INTEGRITY)) || defined(QT_BOOTSTRAPPED)

template <>
timeval qAbs(const timeval &t)
{
    timeval tmp = t;
    if (tmp.tv_sec < 0) {
        tmp.tv_sec = -tmp.tv_sec - 1;
        tmp.tv_usec -= 1000000;
    }
    if (tmp.tv_sec == 0 && tmp.tv_usec < 0) {
        tmp.tv_usec = -tmp.tv_usec;
    }
    return normalizedTimeval(tmp);
}

/*
 *  Returns true if the real time clock has changed by more than 10%
 *  relative to the processor time since the last time this function was
 *  called. This presumably means that the system time has been changed.
 *
 *  If /a delta is nonzero, delta is set to our best guess at how much the system clock was changed.
 */
bool QTimerInfoList::timeChanged(timeval *delta)
{
    #ifdef Q_OS_NACL
    Q_UNUSED(delta)
    return false; // Calling "times" crashes.
    #endif
    struct tms unused;
    clock_t currentTicks = times(&unused);

    clock_t elapsedTicks = currentTicks - previousTicks;
    timeval elapsedTime = currentTime - previousTime;

    timeval elapsedTimeTicks;
    elapsedTimeTicks.tv_sec = elapsedTicks / ticksPerSecond;
    elapsedTimeTicks.tv_usec = (((elapsedTicks * 1000) / ticksPerSecond) % 1000) * 1000;

    timeval dummy;
    if (!delta)
        delta = &dummy;
    *delta = elapsedTime - elapsedTimeTicks;

    previousTicks = currentTicks;
    previousTime = currentTime;

    // If tick drift is more than 10% off compared to realtime, we assume that the clock has
    // been set. Of course, we have to allow for the tick granularity as well.
    timeval tickGranularity;
    tickGranularity.tv_sec = 0;
    tickGranularity.tv_usec = msPerTick * 1000;
    return elapsedTimeTicks < ((qAbs(*delta) - tickGranularity) * 10);
}

void QTimerInfoList::repairTimersIfNeeded()
{
    return; // TODO - remove all this - I'm not sure that "times(...)" is implemented in Emscripten.
    if (QElapsedTimer::isMonotonic())
        return;
    timeval delta;
    if (timeChanged(&delta))
    {
        timerRepair(delta);
    }
}

#else // !(_POSIX_MONOTONIC_CLOCK-0 <= 0) && !defined(QT_BOOTSTRAPPED)

void QTimerInfoList::repairTimersIfNeeded()
{
}

#endif

/*
 *  insert timer info into list
 */
void QTimerInfoList::timerInsert(QTimerInfo *ti)
{
    int index = size();
    while (index--) {
        register const QTimerInfo * const t = at(index);
        if (!(ti->timeout < t->timeout))
            break;
    }
    insert(index+1, ti);
}

/*
 *  repair broken timer
 */
void QTimerInfoList::timerRepair(const timeval &diff)
{
    // repair all timers
    for (int i = 0; i < size(); ++i) {
        register QTimerInfo *t = at(i);
        t->timeout = t->timeout + diff;
    }
}

/*
 *  Returns the time to wait for the next timer, or null if no timers
 *  are waiting.
 */
bool QTimerInfoList::timerWait(timeval &tm)
{
    timeval currentTime = updateCurrentTime();
    repairTimersIfNeeded();

    // Find first waiting timer not already active
    QTimerInfo *t = 0;
    for (QTimerInfoList::const_iterator it = constBegin(); it != constEnd(); ++it) {
        if (!(*it)->activateRef) {
            t = *it;
            break;
        }
    }

    if (!t)
        return false;

    if (currentTime < t->timeout) {
        // time to wait
        tm = t->timeout - currentTime;
    } else {
        // no time to wait
        tm.tv_sec  = 0;
        tm.tv_usec = 0;
    }

    return true;
}

void QTimerInfoList::registerTimer(int timerId, int interval, QObject *object)
{
    QTimerInfo *t = new QTimerInfo;
    t->id = timerId;
    t->interval.tv_sec  = interval / 1000;
    t->interval.tv_usec = (interval % 1000) * 1000;
    t->timeout = updateCurrentTime() + t->interval;
    t->obj = object;
    t->activateRef = 0;

    timerInsert(t);
}

bool QTimerInfoList::unregisterTimer(int timerId)
{
    // set timer inactive
    for (int i = 0; i < count(); ++i) {
        register QTimerInfo *t = at(i);
        if (t->id == timerId) {
            // found it
            removeAt(i);
            if (t == firstTimerInfo)
                firstTimerInfo = 0;
            if (t->activateRef)
                *(t->activateRef) = 0;

            // release the timer id
            if (!QObjectPrivate::get(t->obj)->inThreadChangeEvent)
                QAbstractEventDispatcherPrivate::releaseTimerId(timerId);

            delete t;
            return true;
        }
    }
    // id not found
    return false;
}

bool QTimerInfoList::unregisterTimers(QObject *object)
{
    if (isEmpty())
        return false;
    for (int i = 0; i < count(); ++i) {
        register QTimerInfo *t = at(i);
        if (t->obj == object) {
            // object found
            removeAt(i);
            if (t == firstTimerInfo)
                firstTimerInfo = 0;
            if (t->activateRef)
                *(t->activateRef) = 0;

            // release the timer id
            if (!QObjectPrivate::get(t->obj)->inThreadChangeEvent)
                QAbstractEventDispatcherPrivate::releaseTimerId(t->id);

            delete t;
            // move back one so that we don't skip the new current item
            --i;
        }
    }
    return true;
}

QList<QPair<int, int> > QTimerInfoList::registeredTimers(QObject *object) const
{
    QList<QPair<int, int> > list;
    for (int i = 0; i < count(); ++i) {
        register const QTimerInfo * const t = at(i);
        if (t->obj == object)
            list << QPair<int, int>(t->id, t->interval.tv_sec * 1000 + t->interval.tv_usec / 1000);
    }
    return list;
}

/*
 *    Activate pending timers, returning how many where activated.
 */
int QTimerInfoList::activateTimers()
{
    if (isEmpty())
    {
        return 0; // nothing to do
    }

        int n_act = 0, maxCount = 0;
    firstTimerInfo = 0;

    timeval currentTime = updateCurrentTime();
    repairTimersIfNeeded();


    // Find out how many timer have expired
    for (QTimerInfoList::const_iterator it = constBegin(); it != constEnd(); ++it) {
        if (currentTime < (*it)->timeout)
            break;
        maxCount++;
    }


    //fire the timers.
    while (maxCount--) {
        if (isEmpty())
            break;

        QTimerInfo *currentTimerInfo = first();
        if (currentTime < currentTimerInfo->timeout)
        {
            break; // no timer has expired
        }

            if (!firstTimerInfo) {
                firstTimerInfo = currentTimerInfo;
            } else if (firstTimerInfo == currentTimerInfo) {
                // avoid sending the same timer multiple times
                break;
            } else if (currentTimerInfo->interval <  firstTimerInfo->interval
                || currentTimerInfo->interval == firstTimerInfo->interval) {
                firstTimerInfo = currentTimerInfo;
                }

                // remove from list
                removeFirst();

            // determine next timeout time
            currentTimerInfo->timeout += currentTimerInfo->interval;
            if (currentTimerInfo->timeout < currentTime)
                currentTimerInfo->timeout = currentTime + currentTimerInfo->interval;

            // reinsert timer
            timerInsert(currentTimerInfo);
            if (currentTimerInfo->interval.tv_usec > 0 || currentTimerInfo->interval.tv_sec > 0)
                n_act++;

            if (!currentTimerInfo->activateRef) {
                // send event, but don't allow it to recurse
                currentTimerInfo->activateRef = &currentTimerInfo;

                QTimerEvent e(currentTimerInfo->id);
                QCoreApplication::sendEvent(currentTimerInfo->obj, &e);

                if (currentTimerInfo)
                    currentTimerInfo->activateRef = 0;
            }
    }

    firstTimerInfo = 0;
    return n_act;
}


QEventDispatcherEmscripten::QEventDispatcherEmscripten(QObject *parent)
{
    m_instance = this;
}
QEventDispatcherEmscripten::~QEventDispatcherEmscripten()
{

}

bool QEventDispatcherEmscripten::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    qDebug() << "QEventDispatcherEmscripten::processEvents called";
    QCoreApplicationPrivate::sendPostedEvents(0, 0, QThreadData::current());
    if (flags & QEventLoop::WaitForMoreEvents)
    {
        qWarning() << "WaitForMoreEvents is not supported in Emscripten";
        return false;
    }
    return true;
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
    timerList.registerTimer(timerId, interval, object);
    timeval wait_tm = { 0l, 0l };
    if (timerList.timerWait(wait_tm))
    {
        EMSCRIPTENQT_resetTimerCallback(wait_tm.tv_usec / 1000);
    }
}
bool QEventDispatcherEmscripten::unregisterTimer(int timerId)
{
    #ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherUNIX::unregisterTimer: invalid argument");
        return false;
    } else if (thread() != QThread::currentThread()) {
        qWarning("QObject::killTimer: timers cannot be stopped from another thread");
        return false;
    }
    #endif

    return timerList.unregisterTimer(timerId);
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
    EMSCRIPTENQT_resetTimerCallback(0);
}

void QEventDispatcherEmscripten::startingUp()
{
    qDebug() << "QEventDispatcherEmscripten::startingUp called";
}
void QEventDispatcherEmscripten::closingDown()
{
    qDebug() << "QEventDispatcherEmscripten::closingDown called";
}
void QEventDispatcherEmscripten::interrupt()
{
    qDebug() << "QEventDispatcherEmscripten::interrupt called";
}
void QEventDispatcherEmscripten::flush()
{
    qDebug() << "QEventDispatcherEmscripten::flush called";
}

void QEventDispatcherEmscripten::processEmscriptenCallback()
{
    processEvents(QEventLoop::AllEvents);
    timerList.activateTimers();
    timeval wait_tm = { 0l, 0l };
    if (timerList.timerWait(wait_tm))
    {
        EMSCRIPTENQT_resetTimerCallback(wait_tm.tv_usec / 1000);
    }
}
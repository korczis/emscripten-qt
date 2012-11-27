#include "qplatformdefs.h"
#include "qapplication.h"
#include "qeventdispatcher_emscripten_qws.h"
#include "private/qwscommand_qws_p.h"
#include "qwsdisplay_qws.h"
#include "qwsevent_qws.h"
#include "qwindowsystem_qws.h"
#include <qdebug.h>


// from qapplication_qws.cpp
extern QWSDisplay* qt_fbdpy; // QWS `display'

QEventDispatcherEmscripten* QEventDispatcherEmscriptenQWS::m_instance = NULL;

QEventDispatcherEmscriptenQWS::QEventDispatcherEmscriptenQWS(QObject *parent)
{
    qDebug() << "QEventDispatcherEmscriptenQWS::QEventDispatcherEmscriptenQWS";
    m_instance = this;
}
QEventDispatcherEmscriptenQWS::~QEventDispatcherEmscriptenQWS()
{
    qDebug() << "QEventDispatcherEmscriptenQWS::~QEventDispatcherEmscriptenQWS";
}

bool QEventDispatcherEmscriptenQWS::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    qDebug() << "QEventDispatcherEmscriptenQWS::processEvents flags: " << flags;
    if (flags & QEventLoop::WaitForMoreEvents)
    {
        qWarning() << "WaitForMoreEvents is not supported in Emscripten";
        return false;
    }

    // process events from the QWS server
    int           nevents = 0;

    // handle gui and posted events
    //d->interrupt = false; // XXX uncomment and fix this.
    QApplication::sendPostedEvents();

    // XXX There should be a while  d->interrupt here.
    while (true)
    {        // also flushes output buffer ###can be optimized
        QWSEvent *event;
        if (!(flags & QEventLoop::ExcludeUserInputEvents)
            && !queuedUserInputEvents.isEmpty()) {
                // process a pending user input event
                qDebug() << "queuedUserInputEvents size: " << queuedUserInputEvents.size();
                event = queuedUserInputEvents.takeFirst();
                qDebug() << "Got a queued user input event, apparently." << (void*)event;
                qDebug() << "Type is " << event->type;
        } else if  (qt_fbdpy->eventPending()) {
            event = qt_fbdpy->getEvent();        // get next event
            qDebug() << "Got a pending input event, apparently." << (void*)event;
            qDebug() << "Type is " << event->type;
            if (flags & QEventLoop::ExcludeUserInputEvents) {
                // queue user input events
                if (event->type == QWSEvent::Mouse || event->type == QWSEvent::Key) {
                    qDebug() << "It was a mouse or key: appending to queuedUserInputEvents";
                    queuedUserInputEvents.append(event);
                    continue;
                }
            }
        } else {
            break;
        }

        if (filterEvent(event)) {
            qDebug() << "Filtered event";
            delete event;
            continue;
        }
        nevents++;

        bool ret = qApp->qwsProcessEvent(event) == 1;
        qDebug() << "Processed event; got: " << ret;
        delete event;
        if (ret) {
            qDebug() << "QEventDispatcherEmscriptenQWS Exiting after successful event processing";
            return true;
        }
    }

    // XXX There should be a check of d->interrupt here.
    if (true)
    {
        extern QList<QWSCommand*> *qt_get_server_queue();
        if (!qt_get_server_queue()->isEmpty()) {
            QWSServer::processEventQueue();
        }

        if (QEventDispatcherEmscripten::processEvents(flags))
        {
            qDebug() << "QEventDispatcherEmscriptenQWS Exiting after allowing superclass to process events";
            return true;
        }
    }
    qDebug() << "QEventDispatcherEmscriptenQWS Exiting";
    return (nevents > 0);
}
bool QEventDispatcherEmscriptenQWS::hasPendingEvents()
{
    qDebug() << "QEventDispatcherEmscriptenQWS::hasPendingEvents()";
    extern uint qGlobalPostedEventsCount(); // from qapplication.cpp
    return qGlobalPostedEventsCount() || qt_fbdpy->eventPending();
}

void QEventDispatcherEmscriptenQWS::flush()
{
    qDebug() << "QEventDispatcherEmscriptenQWS::flush";
    if(qApp)
        qApp->sendPostedEvents();
    (void)qt_fbdpy->eventPending(); // flush
}

void QEventDispatcherEmscriptenQWS::startingUp()
{
    qDebug() << "QEventDispatcherEmscriptenQWS::startingUp";
}
void QEventDispatcherEmscriptenQWS::closingDown()
{
    qDebug() << "QEventDispatcherEmscriptenQWS::closingDown";
}

void QEventDispatcherEmscriptenQWS::newUserEventsToProcess()
{
    m_instance->wakeUp();
}
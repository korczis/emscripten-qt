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

QEventDispatcherEmscriptenQWS::QEventDispatcherEmscriptenQWS(QObject *parent)
{
    qDebug() << "QEventDispatcherEmscriptenQWS::QEventDispatcherEmscriptenQWS";
}
QEventDispatcherEmscriptenQWS::~QEventDispatcherEmscriptenQWS()
{
    qDebug() << "QEventDispatcherEmscriptenQWS::~QEventDispatcherEmscriptenQWS";
}

bool QEventDispatcherEmscriptenQWS::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    qDebug() << "QEventDispatcherEmscriptenQWS::processEvents";

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
            event = queuedUserInputEvents.takeFirst();
            } else if  (qt_fbdpy->eventPending()) {
                event = qt_fbdpy->getEvent();        // get next event
                if (flags & QEventLoop::ExcludeUserInputEvents) {
                    // queue user input events
                    if (event->type == QWSEvent::Mouse || event->type == QWSEvent::Key) {
                        queuedUserInputEvents.append(event);
                        continue;
                    }
                }
            } else {
                break;
            }

            if (filterEvent(event)) {
                delete event;
                continue;
            }
            nevents++;

            bool ret = qApp->qwsProcessEvent(event) == 1;
            delete event;
            if (ret) {
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
            return true;
    }
    return (nevents > 0);
}
bool QEventDispatcherEmscriptenQWS::hasPendingEvents()
{
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

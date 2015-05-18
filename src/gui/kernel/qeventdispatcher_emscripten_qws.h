#ifndef QEVENTDISPATCHER_EMSCRIPTEN_QWS_P_H
#define QEVENTDISPATCHER_EMSCRIPTEN_QWS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "private/qeventdispatcher_emscripten_p.h"

class QWSEvent;

QT_BEGIN_NAMESPACE


class QEventDispatcherEmscriptenQWS : public QEventDispatcherEmscripten
{
    Q_OBJECT
public:
    explicit QEventDispatcherEmscriptenQWS(QObject *parent = 0);
    ~QEventDispatcherEmscriptenQWS();

    bool processEvents(QEventLoop::ProcessEventsFlags flags);
    bool hasPendingEvents();

    void flush();

    void startingUp();
    void closingDown();

    static void newUserEventsToProcess();
private:
    QList<QWSEvent*> queuedUserInputEvents;
    static QEventDispatcherEmscripten* m_instance;
};

QT_END_NAMESPACE

#endif // QEVENTDISPATCHER_EMSCRIPTEN_QWS_P_H


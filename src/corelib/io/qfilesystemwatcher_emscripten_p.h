#ifndef QFILESYSTEMWATCHER_EMSCRIPTEN_P_H
#define QFILESYSTEMWATCHER_EMSCRIPTEN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLibrary class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qfilesystemwatcher_p.h"

#ifndef QT_NO_FILESYSTEMWATCHER

#include <qhash.h>
#include <qmutex.h>

QT_BEGIN_NAMESPACE

class QEmscriptenFileSystemWatcherEngine : public QFileSystemWatcherEngine
{
    Q_OBJECT

public:
    ~QEmscriptenFileSystemWatcherEngine();

    static QEmscriptenFileSystemWatcherEngine *create();

    void run();

    QStringList addPaths(const QStringList &paths, QStringList *files, QStringList *directories);
    QStringList removePaths(const QStringList &paths, QStringList *files, QStringList *directories);

    void stop();
private:
    QEmscriptenFileSystemWatcherEngine();
};


QT_END_NAMESPACE
#endif // QT_NO_FILESYSTEMWATCHER
#endif // QFILESYSTEMWATCHER_EMSCRIPTEN_P_H


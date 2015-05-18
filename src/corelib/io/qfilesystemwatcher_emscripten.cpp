#include "qfilesystemwatcher.h"
#include "qfilesystemwatcher_emscripten_p.h"

#ifndef QT_NO_FILESYSTEMWATCHER

QT_BEGIN_NAMESPACE

QEmscriptenFileSystemWatcherEngine *QEmscriptenFileSystemWatcherEngine::create()
{
    return new QEmscriptenFileSystemWatcherEngine();
}

QEmscriptenFileSystemWatcherEngine::QEmscriptenFileSystemWatcherEngine()
{
}

QEmscriptenFileSystemWatcherEngine::~QEmscriptenFileSystemWatcherEngine()
{
}

void QEmscriptenFileSystemWatcherEngine::run()
{
}

QStringList QEmscriptenFileSystemWatcherEngine::addPaths(const QStringList &paths,
                                                      QStringList *files,
                                                      QStringList *directories)
{
    return QStringList();
}

QStringList QEmscriptenFileSystemWatcherEngine::removePaths(const QStringList &paths,
                                                         QStringList *files,
                                                         QStringList *directories)
{
    return QStringList();
}

void QEmscriptenFileSystemWatcherEngine::stop()
{
}

QT_END_NAMESPACE

#endif // QT_NO_FILESYSTEMWATCHER

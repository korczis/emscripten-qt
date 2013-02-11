
/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qprocess.h"
#include "qprocess_p.h"

#include <qdebug.h>

#ifndef QT_NO_PROCESS

QT_BEGIN_NAMESPACE

//#define QPROCESS_DEBUG

#define NOTIFYTIMEOUT 100

void warnUnsupported()
{
    qWarning() << "QProcess not supported on Emscripten!";
}

bool QProcessPrivate::createChannel(Channel &channel)
{
    warnUnsupported();
    return false;
}

void QProcessPrivate::destroyPipe(Q_PIPE pipe[2])
{
    warnUnsupported();
}



QProcessEnvironment QProcessEnvironment::systemEnvironment()
{
    QProcessEnvironment env;
    warnUnsupported();
    return env;
}

void QProcessPrivate::startProcess()
{
    warnUnsupported();
}

bool QProcessPrivate::processStarted()
{
    warnUnsupported();
    return false;
}

qint64 QProcessPrivate::bytesAvailableFromStdout() const
{
    warnUnsupported();
    return 0;
}

qint64 QProcessPrivate::bytesAvailableFromStderr() const
{
    warnUnsupported();
    return 0;
}

qint64 QProcessPrivate::readFromStdout(char *data, qint64 maxlen)
{
    warnUnsupported();
    return -1;
}

qint64 QProcessPrivate::readFromStderr(char *data, qint64 maxlen)
{
    warnUnsupported();
    return -1;
}



void QProcessPrivate::terminateProcess()
{
    warnUnsupported();
}

void QProcessPrivate::killProcess()
{
    warnUnsupported();
}

bool QProcessPrivate::waitForStarted(int)
{
    warnUnsupported();
    return false;
}

bool QProcessPrivate::waitForReadyRead(int msecs)
{
    warnUnsupported();
    return false;
}

bool QProcessPrivate::waitForBytesWritten(int msecs)
{
    warnUnsupported();
    return false;
}


bool QProcessPrivate::waitForFinished(int msecs)
{
    warnUnsupported();
    return false;
}


void QProcessPrivate::findExitCode()
{
    warnUnsupported();
    exitCode = -1;
}

qint64 QProcessPrivate::writeToStdin(const char *data, qint64 maxlen)
{
    warnUnsupported();
    return -1;
}

bool QProcessPrivate::waitForWrite(int msecs)
{
    warnUnsupported();
    return false;
}

void QProcessPrivate::_q_notified()
{
    warnUnsupported();
}

bool QProcessPrivate::startDetached(const QString &program, const QStringList &arguments, const QString &workingDir, qint64 *pid)
{
    warnUnsupported();
    return false;
}

QT_END_NAMESPACE

#endif // QT_NO_PROCESS

/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the qmake spec of the Qt Toolkit.
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

#ifndef QPLATFORMDEFS_H
#define QPLATFORMDEFS_H

// Get Qt defines/settings
#include "../../common/posix/qplatformdefs.h"
#undef QT_OPEN_LARGEFILE
#undef QT_LARGEFILE_SUPPORT
#define QT_OPEN_LARGEFILE 0

//#define _POSIX_TIMERS
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "qglobal.h"

//#define QT_NO_SOCKET_H
//#define QT_NO_SETTINGS
//#define QT_NO_CODECS
//#define QT_NO_TEXTCODECPLUGIN
//#define QT_NO_SYSTEMLOCALE
//#define QT_NO_PROCESS
//#define QT_NO_QWS_MULTIPROCESS
//#define QT_NO_SOUND
//#define QT_NO_LIBRARY

#define DIR void *

#include <pthread.h>

#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#undef QT_LSTAT
#define QT_LSTAT                QT_STAT


#endif // QPLATFORMDEFS_H

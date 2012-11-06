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

#include "qthread.h"

#include "qthreadstorage.h"

#include "qthread_p.h"

#include "qdebug.h"



QT_BEGIN_NAMESPACE


static  QThreadData *currentThreadData = 0;


QThreadData *QThreadData::current()
{
	if (currentThreadData == NULL)
	{
		currentThreadData = new QThreadData;	
	}
	return currentThreadData;
}






/**************************************************************************
 ** QThread
 *************************************************************************/

Qt::HANDLE QThread::currentThreadId()
{
    return (Qt::HANDLE)NULL;
}


int QThread::idealThreadCount()
{
	return 1;
}

void QThread::yieldCurrentThread()
{
}

void QThread::sleep(unsigned long secs)
{
}

void QThread::msleep(unsigned long msecs)
{
}

void QThread::usleep(unsigned long usecs)
{
}


void QThread::start(Priority priority)
{
}

void QThread::terminate()
{
}

bool QThread::wait(unsigned long time)
{
return true;
}


void QThread::setTerminationEnabled(bool enabled)
{
}

void QThread::setPriority(Priority priority)
{
}


QT_END_NAMESPACE


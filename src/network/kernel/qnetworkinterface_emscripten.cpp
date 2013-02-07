/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNetwork of the Qt Toolkit.
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

//#define QNETWORKINTERFACE_DEBUG

#include "qnetworkinterface.h"
#include "qnetworkinterface_p.h"

#ifndef QT_NO_NETWORKINTERFACE

QT_BEGIN_NAMESPACE

QList<QNetworkInterfacePrivate *> QNetworkInterfaceManager::scan()
{
    QList<QNetworkInterfacePrivate*> interfaces;
    QNetworkInterfacePrivate* eth0Fake = new QNetworkInterfacePrivate;
    eth0Fake->index = 0;
    eth0Fake->name = "eth0";
    eth0Fake->flags = QNetworkInterface::IsUp | QNetworkInterface::CanBroadcast | QNetworkInterface::CanMulticast;
    eth0Fake->hardwareAddress = "65:6d:73:63:72:71:74";

    QNetworkAddressEntry fakeEntry;
    fakeEntry.setPrefixLength(64); //most common
    fakeEntry.setIp(QHostAddress("192.168.0.1"));
    fakeEntry.setNetmask(QHostAddress("255.255.255.0"));
    fakeEntry.setBroadcast(QHostAddress("192.168.0.255"));

    eth0Fake->addressEntries << fakeEntry;
    interfaces << eth0Fake;

    return interfaces;
}

QT_END_NAMESPACE

#endif // QT_NO_NETWORKINTERFACE

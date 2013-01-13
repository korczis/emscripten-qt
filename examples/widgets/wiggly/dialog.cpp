/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtGui>

#include "dialog.h"
#include "wigglywidget.h"

// This isn't necessary for porting, but it's a fun demo of how an emscripten-qt
// app can interact with the rest of the webpage.
Dialog *instance = NULL;
extern "C"
{
	void EMSCRIPTENQT_qtTextChanged(const char* newText)
#ifdef EMSCRIPTEN_NATIVE
	{
		qDebug() << "Ignoring text change to " << newText << " as I am not a web browser!";
	}
#endif
	;
        void EMSCRIPTENQT_htmlTextChanged(const char* newText)	__attribute__((used));
        void EMSCRIPTENQT_htmlTextChanged(const char* newText)
	{
		instance->htmlTextChanged(QString(newText));
	}
}
void Dialog::qtTextChanged(const QString& newText)
{
	EMSCRIPTENQT_qtTextChanged(newText.toUtf8().data());
}
void Dialog::htmlTextChanged(const QString& newText)
{
	lineEdit->setText(newText);
}

//! [0]
Dialog::Dialog(QWidget *parent, bool smallScreen)
    : QDialog(parent)
{
    instance = this;
    WigglyWidget *wigglyWidget = new WigglyWidget;
    lineEdit = new QLineEdit;

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(wigglyWidget);
    layout->addWidget(lineEdit);
    setLayout(layout);

#ifdef QT_SOFTKEYS_ENABLED
    QAction *exitAction = new QAction(tr("Exit"), this);
    exitAction->setSoftKeyRole(QAction::NegativeSoftKey);
    connect (exitAction, SIGNAL(triggered()),this, SLOT(close()));
    addAction (exitAction);

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::WindowSoftkeysVisibleHint;
    setWindowFlags(flags);
#endif

    connect(lineEdit, SIGNAL(textChanged(QString)),
            wigglyWidget, SLOT(setText(QString)));
    connect(lineEdit, SIGNAL(textChanged(QString)),
            this, SLOT(qtTextChanged(QString)));
    if (!smallScreen){
        lineEdit->setText(tr("Hello world!"));
    }
    else{
        lineEdit->setText(tr("Hello!"));
    }
    setWindowTitle(tr("Wiggly"));
    resize(360, 145);
}
//! [0]

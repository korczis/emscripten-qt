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

#include "mainwindow.h"
#include "scribblearea.h"

//! [0]
MainWindow::MainWindow()
{
    scribbleArea = new ScribbleArea;
    setCentralWidget(scribbleArea);

    createActions();
    createMenus();

    setWindowTitle(tr("Scribble"));
    resize(500, 500);
}
//! [0]

//! [1]
void MainWindow::closeEvent(QCloseEvent *event)
//! [1] //! [2]
{
    if (ifMaybeSaveIsTrue == CloseWindow)
    {
	// We've already queried maybeSave, and the result was true; really close
        event->accept();
    }
    else
    {
	    event->ignore();
	    ifMaybeSaveIsTrue = CloseWindow;
	    maybeSave();
    }
}
//! [2]

//! [3]
void MainWindow::open()
//! [3] //! [4]
{
    ifMaybeSaveIsTrue = OpenFile;
    maybeSave();
}
//! [4]

//! [5]
void MainWindow::save()
//! [5] //! [6]
{
    QAction *action = qobject_cast<QAction *>(sender());
    QByteArray fileFormat = action->data().toByteArray();
    saveFile(fileFormat);
}
//! [6]

//! [7]
void MainWindow::penColor()
//! [7] //! [8]
{
    AsyncDialogHelper::getColor(this, SLOT(penColorChanged(const QColor&)), scribbleArea->penColor());
}
//! [8]

void MainWindow::penColorChanged(const QColor& newColor)
{
    if (newColor.isValid())
        scribbleArea->setPenColor(newColor);
}

//! [9]
void MainWindow::penWidth()
//! [9] //! [10]
{
    bool ok;
    int newWidth = QInputDialog::getInteger(this, tr("Scribble"),
                                            tr("Select pen width:"),
                                            scribbleArea->penWidth(),
                                            1, 50, 1, &ok);
    if (ok)
        scribbleArea->setPenWidth(newWidth);
}
//! [10]

//! [11]
void MainWindow::about()
//! [11] //! [12]
{
    QMessageBox::about(this, tr("About Scribble"),
            tr("<p>The <b>Scribble</b> example shows how to use QMainWindow as the "
               "base widget for an application, and how to reimplement some of "
               "QWidget's event handlers to receive the events generated for "
               "the application's widgets:</p><p> We reimplement the mouse event "
               "handlers to facilitate drawing, the paint event handler to "
               "update the application and the resize event handler to optimize "
               "the application's appearance. In addition we reimplement the "
               "close event handler to intercept the close events before "
               "terminating the application.</p><p> The example also demonstrates "
               "how to use QPainter to draw an image in real time, as well as "
               "to repaint widgets.</p>"));
}
//! [12]

void MainWindow::maybeSaveReply(QMessageBox::StandardButton reply)
{
    if (reply == QMessageBox::Save) {
        saveFile("png");
    } else if (reply == QMessageBox::Cancel) {
        maybeSaveWasFalse();
    } else if (reply == QMessageBox::Discard) {
	maybeSaveWasTrue();
    }
}

void MainWindow::openFileNameReply(const QString& fileName)
{
    if (!fileName.isEmpty())
        scribbleArea->openImage(fileName);
}

void MainWindow::saveFileNameReply(const QString& fileName)
{
    if (fileName.isEmpty()) {
        maybeSaveWasFalse();
    } else {
        if (scribbleArea->saveImage(fileName, fileFormat))
	{
		maybeSaveWasTrue();
	}
	else
        {
		maybeSaveWasFalse();
	}
    }
}

//! [13]
void MainWindow::createActions()
//! [13] //! [14]
{
    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    foreach (QByteArray format, QImageWriter::supportedImageFormats()) {
        QString text = tr("%1...").arg(QString(format).toUpper());

        QAction *action = new QAction(text, this);
        action->setData(format);
        connect(action, SIGNAL(triggered()), this, SLOT(save()));
        saveAsActs.append(action);
    }

    printAct = new QAction(tr("&Print..."), this);
    connect(printAct, SIGNAL(triggered()), scribbleArea, SLOT(print()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    penColorAct = new QAction(tr("&Pen Color..."), this);
    connect(penColorAct, SIGNAL(triggered()), this, SLOT(penColor()));

    penWidthAct = new QAction(tr("Pen &Width..."), this);
    connect(penWidthAct, SIGNAL(triggered()), this, SLOT(penWidth()));

    clearScreenAct = new QAction(tr("&Clear Screen"), this);
    clearScreenAct->setShortcut(tr("Ctrl+L"));
    connect(clearScreenAct, SIGNAL(triggered()),
            scribbleArea, SLOT(clearImage()));

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    aboutQtAct = new QAction(tr("About &Qt"), this);
    connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}
//! [14]

//! [15]
void MainWindow::createMenus()
//! [15] //! [16]
{
    saveAsMenu = new QMenu(tr("&Save As"), this);
    foreach (QAction *action, saveAsActs)
        saveAsMenu->addAction(action);

    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(openAct);
    fileMenu->addMenu(saveAsMenu);
    fileMenu->addAction(printAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    optionMenu = new QMenu(tr("&Options"), this);
    optionMenu->addAction(penColorAct);
    optionMenu->addAction(penWidthAct);
    optionMenu->addSeparator();
    optionMenu->addAction(clearScreenAct);

    helpMenu = new QMenu(tr("&Help"), this);
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);

    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(optionMenu);
    menuBar()->addMenu(helpMenu);
}
//! [16]

//! [17]
bool MainWindow::maybeSave()
//! [17] //! [18]
{
    if (scribbleArea->isModified()) {
       AsyncDialogHelper::warning(this, SLOT(maybeSaveReply(QMessageBox::StandardButton)),
                          this, tr("Scribble"),
                          tr("The image has been modified.\n"
                             "Do you want to save your changes?"),
                          QMessageBox::Save | QMessageBox::Discard
			  | QMessageBox::Cancel);
    }
    else
    {
        // Need to call maybeSaveWasTrue() asynchronously, in case we're in a callback from closeEvent (which we just ignored)
        // and *really* want to close the window (which we won't be able to do until the next event loop, as
        // we just ignored a close event!)
	QTimer::singleShot(0, this, SLOT(maybeSaveWasTrue()));
    }
    return true; // TODO - remove this - maybeSave won't return anything when we're through!
}
//! [18]

//! [19]
bool MainWindow::saveFile(const QByteArray &fileFormat)
//! [19] //! [20]
{
    QString initialPath = QDir::currentPath() + "/untitled." + fileFormat;
    this->fileFormat = fileFormat;

    AsyncDialogHelper::getSaveFileName(this, SLOT(saveFileNameReply(const QString&)),
			       this, tr("Save As"),
                               initialPath,
                               tr("%1 Files (*.%2);;All Files (*)")
                               .arg(QString(fileFormat.toUpper()))
                               .arg(QString(fileFormat)));
    return true; // TODO - remove this - saveFile won't return anything when we're through!
}
//! [20]

void MainWindow::maybeSaveWasTrue()
{
    switch (ifMaybeSaveIsTrue)
    {	
	case OpenFile:
	    AsyncDialogHelper::getOpenFileName(this, SLOT(openFileNameReply(const QString&)), 
                                   this,
                                   tr("Open File"), QDir::currentPath());
	    break;
        case CloseWindow:
            qDebug() << "Calling close";
	    QMainWindow::close();
            break;
    }
    ifMaybeSaveIsTrue = DoNothing;
}

void MainWindow::maybeSaveWasFalse()
{
    ifMaybeSaveIsTrue = DoNothing;
}

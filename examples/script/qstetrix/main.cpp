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

#include "tetrixboard.h"

#include <QtGui>
#include <QtScript/QtScript>
#include <QtUiTools/QUiLoader>

#ifndef QT_NO_SCRIPTTOOLS
#include <QtScriptTools/QtScriptTools>
#endif
QScriptEngine *engine;

struct QtMetaObject : private QObject
{
public:
    static const QMetaObject *get()
        { return &static_cast<QtMetaObject*>(0)->staticQtMetaObject; }
};

//! [0]
class TetrixUiLoader : public QUiLoader
{
public:
    TetrixUiLoader(QObject *parent = 0)
        : QUiLoader(parent)
        { }
    virtual QWidget *createWidget(const QString &className, QWidget *parent = 0,
                                  const QString &name = QString())
    {
        if (className == QLatin1String("TetrixBoard")) {
            QWidget *board = new TetrixBoard(parent);
            board->setObjectName(name);
            return board;
        }
        return QUiLoader::createWidget(className, parent, name);
    }
};
//! [0]

static QScriptValue evaluateFile(QScriptEngine &engine, const QString &fileName)
{
    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    return engine.evaluate(file.readAll(), fileName);
}

#ifndef EMSCRIPTEN_NATIVE
int main(int argc, char *argv[])
#else
int emscriptenQtSDLMain(int argc, char *argv[])
#endif
{
    Q_INIT_RESOURCE(tetrix);

//! [1]
    QApplication *app = new QApplication(argc, argv);
    engine = new QScriptEngine;

    QScriptValue *Qt  = new QScriptValue(engine->newQMetaObject(QtMetaObject::get()));
    Qt->setProperty("App", engine->newQObject(app));
    engine->globalObject().setProperty("Qt", *Qt);
//! [1]

#if !defined(QT_NO_SCRIPTTOOLS)
    //QScriptEngineDebugger *debugger = new QScriptEngineDebugger;
    //debugger->attachTo(engine);
    //QMainWindow *debugWindow = debugger->standardWindow();
    //debugWindow->resize(1024, 640);
#endif

//! [2]
    evaluateFile(*engine, ":/tetrixpiece.js");
    evaluateFile(*engine, ":/tetrixboard.js");
    evaluateFile(*engine, ":/tetrixwindow.js");
//! [2]

//! [3]
    TetrixUiLoader loader;
    QFile uiFile(":/tetrixwindow.ui");
    uiFile.open(QIODevice::ReadOnly);
    QWidget *ui = loader.load(&uiFile);
    uiFile.close();

    QScriptValue *ctor  = new QScriptValue(engine->evaluate("TetrixWindow"));
    QScriptValue *scriptUi  = new QScriptValue(engine->newQObject(ui, QScriptEngine::ScriptOwnership));
    QScriptValue *tetrix  = new QScriptValue(ctor->construct(QScriptValueList() << *scriptUi));
//! [3]

    QPushButton *debugButton = ui->findChild<QPushButton*>("debugButton");
#if !defined(QT_NO_SCRIPTTOOLS)
    //QObject::connect(debugButton, SIGNAL(clicked()),
    //                 debugger->action(QScriptEngineDebugger::InterruptAction),
    //                 SIGNAL(triggered()));
    //QObject::connect(debugButton, SIGNAL(clicked()),
    //                 debugWindow, SLOT(show()));
#else
    debugButton->hide();
#endif

//! [4]
    ui->resize(550, 370);
    ui->show();

    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));
    return app->exec();
//! [4]
}

#ifdef EMSCRIPTEN_NATIVE
#include <QtGui/emscripten-qt-sdl.h>
int main(int argc, char *argv[])
{
        EmscriptenQtSDL::setAttemptedLocalEventLoopCallback(EmscriptenQtSDL::TRIGGER_ASSERT);
        EmscriptenQtSDL::run(640, 480, argc, argv);
        delete engine;
        return 0;
}
#endif

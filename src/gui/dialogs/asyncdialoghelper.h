#ifndef ASYNCDIALOGHELPER
#define ASYNCDIALOGHELPER

#include <QInputDialog>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

namespace AsyncDialogHelper
{
    void getInt(QObject* receiver, const char* signal, QWidget * parent, const QString & title, const QString & label, int value = 0, int min = -2147483647, int max = 2147483647, int step = 1, Qt::WindowFlags flags = 0)
    {
        qDebug() << "Gobbles!";
        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setIntValue(value);
        inputDialog->setIntMinimum(min);
        inputDialog->setIntMaximum(max);
        inputDialog->setIntStep(step);
        inputDialog->setModal(true);
        QObject::connect(inputDialog, SIGNAL(intValueSelected(int)), receiver, signal);
        QObject::connect(inputDialog, SIGNAL(intValueSelected(int)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
}

QT_END_NAMESPACE

QT_END_HEADER

#endif
#ifndef ASYNCDIALOGHELPER
#define ASYNCDIALOGHELPER

#include <QInputDialog>
#include <QColorDialog>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

namespace AsyncDialogHelper
{
    void getInt(QObject* receiver, const char* signal, QWidget * parent, const QString & title, const QString & label, int value = 0, int min = -2147483647, int max = 2147483647, int step = 1, Qt::WindowFlags flags = 0)
    {
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
    void getDouble (QObject* receiver, const char* signal, QWidget * parent, const QString & title, const QString & label, double value = 0, double min = -2147483647, double max = 2147483647, int decimals = 1, Qt::WindowFlags flags = 0 )
    {
        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setDoubleValue(value);
        inputDialog->setDoubleMinimum(min);
        inputDialog->setDoubleMaximum(max);
        inputDialog->setDoubleDecimals(decimals);
        inputDialog->setModal(true);
        QObject::connect(inputDialog, SIGNAL(doubleValueSelected(double)), receiver, signal);
        QObject::connect(inputDialog, SIGNAL(doubleValueSelected(double)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getItem (QObject* receiver, const char* signal, QWidget * parent, const QString & title, const QString & label, const QStringList & items, int current = 0, bool editable = true, Qt::WindowFlags flags = 0, Qt::InputMethodHints inputMethodHints = Qt::ImhNone )
    {
        QString text(items.value(current));

        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setComboBoxItems(items);
        inputDialog->setTextValue(text);
        inputDialog->setComboBoxEditable(editable);
        inputDialog->setInputMethodHints(inputMethodHints);
        inputDialog->setModal(true);
        QObject::connect(inputDialog, SIGNAL(textValueSelected(const QString&)), receiver, signal);
        QObject::connect(inputDialog, SIGNAL(textValueSelected(const QString&)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getText (QObject* receiver, const char* signal, QWidget * parent, const QString & title, const QString & label, QLineEdit::EchoMode mode = QLineEdit::Normal, const QString & text = QString(), bool * ok = 0, Qt::WindowFlags flags = 0, Qt::InputMethodHints inputMethodHints = Qt::ImhNone )
    {
        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setTextValue(text);
        inputDialog->setTextEchoMode(mode);
        inputDialog->setInputMethodHints(inputMethodHints);
        QObject::connect(inputDialog, SIGNAL(textValueSelected(const QString&)), receiver, signal);
        QObject::connect(inputDialog, SIGNAL(textValueSelected(const QString&)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getColor(QObject* receiver, const char* signal, const QColor &initial, QWidget *parent, const QString &title,
                              QColorDialog::ColorDialogOptions options)
    {
        QColorDialog *colorDialog = new QColorDialog(parent);
        if (!title.isEmpty())
            colorDialog->setWindowTitle(title);
        colorDialog->setOptions(options);
        colorDialog->setCurrentColor(initial);
        QObject::connect(colorDialog, SIGNAL(colorSelected(const QColor&)), receiver, signal);
        QObject::connect(colorDialog, SIGNAL(colorSelected(const QColor&)), colorDialog, SLOT(deleteLater()));
        colorDialog->show();
    }
}


QT_END_NAMESPACE

QT_END_HEADER

#endif
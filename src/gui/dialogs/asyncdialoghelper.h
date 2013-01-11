#ifndef ASYNCDIALOGHELPER
#define ASYNCDIALOGHELPER

#include <QInputDialog>
#include <QColorDialog>
#include <QFontDialog>
#include <QFileDialog>
#include <QMessageBox>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

namespace AsyncDialogHelper
{
    namespace Private
    {
        // Rather annoyingly, QMessageBox doesn't emit a signal telling us which StandardButton was chosen
        // (only the QAbstractButton).  This class translates from QAbstractButton to StandardButton and
        // forwards the result.
        class AbstractButtonToStandardButton : public QObject
        {
            Q_OBJECT
        public:
            AbstractButtonToStandardButton(QObject* parent, QObject* standardButtonReceiver, const char* standardButtonReceiverSlot) : QObject(parent)
            {
                connect(this, SIGNAL(standardButtonClicked(QMessageBox::StandardButton)), standardButtonReceiver, standardButtonReceiverSlot);
            };
        public slots:
            void forwardStandardButton(QAbstractButton* abstractButton)
            {
                QMessageBox* sendingMessageBox = qobject_cast<QMessageBox*>(sender());
                Q_ASSERT(sendingMessageBox);
                QMessageBox::StandardButton standardButton = sendingMessageBox->standardButton(abstractButton);
                emit standardButtonClicked(standardButton);
            }
        signals:
            void standardButtonClicked(QMessageBox::StandardButton standardButton);
        };

        void showMessageBox(QMessageBox::Icon icon, QObject* receiver, const char* slot, QWidget *parent, const QString &title,
                         const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                         QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
        {
            QMessageBox *messageBox = new QMessageBox(icon, title, text, buttons, parent);
            messageBox->setModal(true);

            Private::AbstractButtonToStandardButton *abstractButtonToStandardButton = new Private::AbstractButtonToStandardButton(messageBox, receiver, slot);

            QObject::connect(messageBox, SIGNAL(buttonClicked(QAbstractButton*)), abstractButtonToStandardButton, SLOT(forwardStandardButton(QAbstractButton*)));
            QObject::connect(messageBox, SIGNAL(finished(int)), messageBox, SLOT(deleteLater()));
            messageBox->show();
        }
    }
    void getInt(QObject* receiver, const char* slot, QWidget * parent, const QString & title, const QString & label, int value = 0, int min = -2147483647, int max = 2147483647, int step = 1, Qt::WindowFlags flags = 0)
    {
        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setIntValue(value);
        inputDialog->setIntMinimum(min);
        inputDialog->setIntMaximum(max);
        inputDialog->setIntStep(step);
        inputDialog->setModal(true);
        QObject::connect(inputDialog, SIGNAL(intValueSelected(int)), receiver, slot);
        QObject::connect(inputDialog, SIGNAL(finished(int)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getDouble (QObject* receiver, const char* slot, QWidget * parent, const QString & title, const QString & label, double value = 0, double min = -2147483647, double max = 2147483647, int decimals = 1, Qt::WindowFlags flags = 0 )
    {
        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setDoubleValue(value);
        inputDialog->setDoubleMinimum(min);
        inputDialog->setDoubleMaximum(max);
        inputDialog->setDoubleDecimals(decimals);
        inputDialog->setModal(true);
        QObject::connect(inputDialog, SIGNAL(doubleValueSelected(double)), receiver, slot);
        QObject::connect(inputDialog, SIGNAL(finished(int)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getItem (QObject* receiver, const char* slot, QWidget * parent, const QString & title, const QString & label, const QStringList & items, int current = 0, bool editable = true, Qt::WindowFlags flags = 0, Qt::InputMethodHints inputMethodHints = Qt::ImhNone )
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
        QObject::connect(inputDialog, SIGNAL(textValueSelected(const QString&)), receiver, slot);
        QObject::connect(inputDialog, SIGNAL(finished(int)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getText (QObject* receiver, const char* slot, QWidget * parent, const QString & title, const QString & label, QLineEdit::EchoMode mode = QLineEdit::Normal, const QString & text = QString(), bool * ok = 0, Qt::WindowFlags flags = 0, Qt::InputMethodHints inputMethodHints = Qt::ImhNone )
    {
        QInputDialog *inputDialog = new QInputDialog(parent, flags);
        inputDialog->setWindowTitle(title);
        inputDialog->setLabelText(label);
        inputDialog->setTextValue(text);
        inputDialog->setTextEchoMode(mode);
        inputDialog->setInputMethodHints(inputMethodHints);
        QObject::connect(inputDialog, SIGNAL(textValueSelected(const QString&)), receiver, slot);
        QObject::connect(inputDialog, SIGNAL(finished(int)), inputDialog, SLOT(deleteLater()));
        inputDialog->show();
    }
    void getColor(QObject* receiver, const char* slot, const QColor &initial, QWidget *parent, const QString &title,
                              QColorDialog::ColorDialogOptions options)
    {
        QColorDialog *colorDialog = new QColorDialog(parent);
        if (!title.isEmpty())
            colorDialog->setWindowTitle(title);
        colorDialog->setOptions(options);
        colorDialog->setCurrentColor(initial);
        QObject::connect(colorDialog, SIGNAL(colorSelected(const QColor&)), receiver, slot);
        QObject::connect(colorDialog, SIGNAL(finished(int)), colorDialog, SLOT(deleteLater()));
        colorDialog->show();
    }
    void getFont(QObject* receiver, const char* slot, const QFont &initial, QWidget *parent,
                                  const QString &title = QString(), QFontDialog::FontDialogOptions options = 0)
    {
        QFontDialog *fontDialog = new QFontDialog(parent);
        fontDialog->setOptions(options);
        fontDialog->setCurrentFont(initial);
        if (!title.isEmpty())
            fontDialog->setWindowTitle(title);
        QObject::connect(fontDialog, SIGNAL(fontSelected(const QFont&)), receiver, slot);
        QObject::connect(fontDialog, SIGNAL(finished(int)), fontDialog, SLOT(deleteLater()));
        fontDialog->show();
    }
    void getExistingDirectory(QObject* receiver, const char* slot, QWidget *parent,
                                          const QString &caption,
                                          const QString &dir,
                                          QFileDialog::Options options)
    {
        QFileDialog *fileDialog = new QFileDialog(parent, caption, QDir::currentPath());
        fileDialog->setOptions(options);
        fileDialog->setFileMode(options & QFileDialog::ShowDirsOnly ? QFileDialog::DirectoryOnly : QFileDialog::Directory);
        QObject::connect(fileDialog, SIGNAL(fileSelected(const QString&)), receiver, slot);
        QObject::connect(fileDialog, SIGNAL(finished(int)), fileDialog, SLOT(deleteLater()));
        fileDialog->show();
    }
    void getOpenFileName(QObject* receiver, const char* slot, QWidget *parent,
                                const QString &caption,
                                const QString &dir,
                                const QString &filter,
                                const QString& selectedFilter,
                                QFileDialog::Options options)
    {
        QFileDialog *fileDialog = new QFileDialog(parent, caption, dir);
        fileDialog->setOptions(options);
        fileDialog->setFileMode(QFileDialog::ExistingFile);
        fileDialog->setFilter(filter);
        if (!selectedFilter.isEmpty())
            fileDialog->selectNameFilter(selectedFilter);
        QObject::connect(fileDialog, SIGNAL(fileSelected(const QString&)), receiver, slot);
        QObject::connect(fileDialog, SIGNAL(finished(int)), fileDialog, SLOT(deleteLater()));
        fileDialog->show();
    }
    void getOpenFileNames(QObject* receiver, const char* slot, QWidget *parent,
                                const QString &caption,
                                const QString &dir,
                                const QString &filter,
                                const QString& selectedFilter,
                                QFileDialog::Options options)
    {
        QFileDialog *fileDialog = new QFileDialog(parent, caption, dir);
        fileDialog->setOptions(options);
        fileDialog->setFileMode(QFileDialog::ExistingFiles);
        fileDialog->setFilter(filter);
        if (!selectedFilter.isEmpty())
            fileDialog->selectNameFilter(selectedFilter);
        QObject::connect(fileDialog, SIGNAL(filesSelected(const QStringList&)), receiver, slot);
        QObject::connect(fileDialog, SIGNAL(finished(int)), fileDialog, SLOT(deleteLater()));
        fileDialog->show();
    }
    void getSaveFileName(QObject* receiver, const char* slot, QWidget *parent,
                                const QString &caption,
                                const QString &dir,
                                const QString &filter,
                                const QString& selectedFilter,
                                QFileDialog::Options options)
    {
        QFileDialog *fileDialog = new QFileDialog(parent, caption, dir);
        fileDialog->setAcceptMode(QFileDialog::AcceptSave);
        fileDialog->setOptions(options);
        fileDialog->setFileMode(QFileDialog::AnyFile);
        fileDialog->setFilter(filter);
        if (!selectedFilter.isEmpty())
            fileDialog->selectNameFilter(selectedFilter);
        QObject::connect(fileDialog, SIGNAL(fileSelected(const QString&)), receiver, slot);
        QObject::connect(fileDialog, SIGNAL(finished(int)), fileDialog, SLOT(deleteLater()));
        fileDialog->show();
    }
    void critical(QObject* receiver, const char* slot, QWidget *parent, const QString &title,
                         const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                         QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
    {
        Private::showMessageBox(QMessageBox::Critical, receiver, slot, parent, title, text, buttons, defaultButton);
    }
    void information(QObject* receiver, const char* slot, QWidget *parent, const QString &title,
                         const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                         QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
    {
        Private::showMessageBox(QMessageBox::Information, receiver, slot, parent, title, text, buttons, defaultButton);
    }
    void question(QObject* receiver, const char* slot, QWidget *parent, const QString &title,
                         const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                         QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
    {
        Private::showMessageBox(QMessageBox::Question, receiver, slot, parent, title, text, buttons, defaultButton);
    }
    void warning(QObject* receiver, const char* slot, QWidget *parent, const QString &title,
                         const QString& text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                         QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
    {
        Private::showMessageBox(QMessageBox::Warning, receiver, slot, parent, title, text, buttons, defaultButton);
    }
}


QT_END_NAMESPACE

QT_END_HEADER

#endif
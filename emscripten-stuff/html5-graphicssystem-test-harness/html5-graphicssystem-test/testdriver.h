#include <QtCore/QObject>

class Html5GraphicsSystemTests;
class QWidget;

class TestDriver : public QObject
{
Q_OBJECT
public:
    TestDriver();
    void beginRunAllTestsAsync();
private slots:
    void runNextTest();
private:
    int m_testIndex;
    QWidget *m_testWidget;
    Html5GraphicsSystemTests *m_tests;
    int findNextTestMethodIndex();
};

#include <QtCore/QObject>

class Html5GraphicsSystemTests;
class TestWidget;

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
    TestWidget *m_testWidget;
    Html5GraphicsSystemTests *m_tests;
    int findNextTestMethodIndex();
};

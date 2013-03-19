#include <QtCore/QObject>

class Html5GraphicsSystemTests;

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
    Html5GraphicsSystemTests *m_tests;
};

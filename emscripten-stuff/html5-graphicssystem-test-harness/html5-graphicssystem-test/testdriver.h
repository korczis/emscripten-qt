#include <QtCore/QObject>
#include <QtCore/QMetaMethod>

class Html5GraphicsSystemTests;
class TestWidget;
class QPainter;

class TestDriver : public QObject
{
Q_OBJECT
public:
    TestDriver(bool usingHtml5Canvas);
    void beginRunAllTestsAsync();
    void runTestWithPainter(QPainter *painter);
private slots:
    void runNextTest();
private:
    int m_testIndex;
    QMetaMethod m_currentTestMethod;
    TestWidget *m_testWidget;
    Html5GraphicsSystemTests *m_tests;
    bool m_usingHtml5Canvas;
    int findNextTestMethodIndex();
};

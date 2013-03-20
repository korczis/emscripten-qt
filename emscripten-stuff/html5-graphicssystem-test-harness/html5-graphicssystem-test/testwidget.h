#include <QtGui/QWidget>

class TestDriver;

class TestWidget : public QWidget
{
Q_OBJECT
public:
    TestWidget(TestDriver *testDriver);
protected:
    void paintEvent ( QPaintEvent * event );
private:
    TestDriver *m_testDriver;
};

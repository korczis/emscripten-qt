#include <QtGui/QWidget>

class TestWidget : public QWidget
{
Q_OBJECT
protected:
    void paintEvent ( QPaintEvent * event );
};

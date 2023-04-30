#include <QWidget>
#include <QTimer>
#include <QLCDNumber>
//
class DigitalClock : public QLCDNumber
{
    Q_OBJECT

public:
    DigitalClock(QWidget *parent, QTimer *timer);

private slots:
    void showTime();
};

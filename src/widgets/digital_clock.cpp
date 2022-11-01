#include <QTimer>
#include <QTime>
#include <QString>
//
#include "digital_clock.hpp"

DigitalClock::DigitalClock(QWidget *parent) : QLCDNumber(parent)
{
    setSegmentStyle(Flat);
    setDigitCount(8);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &DigitalClock::showTime);
    timer->start(1000);

    showTime();
}

void DigitalClock::showTime()
{
    QTime time = QTime::currentTime();
    QString text = time.toString("hh:mm:ss");
    display(text);
}

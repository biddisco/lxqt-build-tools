#include <QString>
#include <QTime>
#include <QTimer>
//
#include "widgets/digital_clock.hpp"

DigitalClock::DigitalClock(QWidget* parent, QTimer* timer)
  : QLCDNumber(parent)
{
  setSegmentStyle(Flat);
  setDigitCount(8);

  if (timer) { connect(timer, &QTimer::timeout, this, &DigitalClock::showTime); }
  else
  {
    QTimer* local_timer = new QTimer(this);
    connect(local_timer, &QTimer::timeout, this, &DigitalClock::showTime);
    local_timer->start(1000);
  }
  showTime();
}

void DigitalClock::showTime()
{
  QTime time = QTime::currentTime();
  QString text = time.toString("hh:mm:ss");
  display(text);
}

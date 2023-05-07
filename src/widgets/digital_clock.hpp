#include <QLCDNumber>
#include <QTimer>
#include <QWidget>
//
class DigitalClock : public QLCDNumber
{
  Q_OBJECT

  public:
  DigitalClock(QWidget* parent, QTimer* timer);

  private slots:
  void showTime();
};

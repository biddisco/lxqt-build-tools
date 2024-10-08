// STL
#include <cmath>
// Qt
#include <QLocale>
// Qwt
#include <QwtText>
//
#include "plot/ohlc_date_scaledraw.hpp"

// ----------------------------------------------------------------------------
ohlc_date_scaledraw::ohlc_date_scaledraw(Qt::TimeSpec timeSpec)
  : QwtDateScaleDraw(timeSpec)
{
  setDateFormat(QwtDate::Minute, "hh:mm");
  setDateFormat(QwtDate::Hour, "hh:mm");
  setDateFormat(QwtDate::Day, "ddd\ndd MMM");
  setDateFormat(QwtDate::Week, "ddd\ndd MMM");
  setDateFormat(QwtDate::Month, "MMM");
  setDateFormat(QwtDate::Year, "yyyy");
}

// ----------------------------------------------------------------------------
QwtText ohlc_date_scaledraw::label(double value) const
{
  const QDateTime dt = toDateTime(value);
  auto interval = intervalType(scaleDiv());
  if (interval == QwtDate::Hour && dt.time().hour() == 0)
  {
    interval = QwtDate::IntervalType(int(interval) + 1);
  }
  if (interval == QwtDate::Day && dt.date().day() == 1) { return QLocale().toString(dt, "MMM"); }
  if (interval == QwtDate::Month && dt.date().month() == 1)
  {
    return QLocale().toString(dt, "yyyy");
  }
  const QString fmt = dateFormatOfDate(dt, interval);
  return QLocale().toString(dt, fmt);
}

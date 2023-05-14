#pragma once

#include <QDateTime>
#include <QLocale>

// ----------------------------------------------------------------------------
// unixtime * 1000 is msecs since 1970/1/1
static std::string msecs_unix_to_calendar_time(uint64_t unixmsecs)
{
  QDateTime dt = QDateTime::fromMSecsSinceEpoch(unixmsecs);
  return QLocale().toString(dt, "yyyy-MM-dd hh:mm:ss").toStdString();
}

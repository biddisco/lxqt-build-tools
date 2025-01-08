#pragma once

#include <chrono>
#include <iomanip>
#include <string>
//
#include <QDateTime>
#include <QLocale>
//

// ----------------------------------------------------------------------------
// namespace grox::date {

//   std::chrono::system_clock::time_point datetime(std::string const& s)
//   {
//     std::istringstream in{s};
//     std::chrono::year y;
//     std::chrono::month_day md;
//     if (in.peek() == 'X')
//     {
//       in >> std::chrono::parse("XXXX-%m-%d", md);
//       if (in.fail())
//         throw std::runtime_error(
//             "Unable to parse a date of the form XXXX-mm-dd out of \"" + s + '"');
//       y = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(
//                                           std::chrono::system_clock::now())}
//               .year();
//     }
//     else
//     {
//       in >> std::chrono::parse("%Y-", y) >> std::chrono::parse("%m-%d", md);
//       if (in.fail())
//         throw std::runtime_error(
//             "Unable to parse a date of the form yyyy-mm-dd out of \"" + s + '"');
//     }
//     auto date = y / md;
//     if (!date.ok()) throw std::runtime_error("Parsed invalid date out of \"" + s + '"');
//     return std::chrono::sys_days{date};
//   }

// }    // namespace grox::date

// ----------------------------------------------------------------------------
// unixtime * 1000 is msecs since 1970/1/1
static std::string msecs_unix_to_calendar_time(uint64_t unixmsecs)
{
  QDateTime dt = QDateTime::fromMSecsSinceEpoch(unixmsecs);
  return QLocale().toString(dt, "yyyy-MM-dd hh:mm:ss").toStdString();
}

static std::string secs_unix_to_calendar_time(uint64_t unixsecs)
{
  QDateTime dt = QDateTime::fromSecsSinceEpoch(unixsecs);
  return QLocale().toString(dt, "yyyy-MM-dd hh:mm:ss").toStdString();
}

#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

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
static std::string msecs_unix_to_calendar_time_local(uint64_t unixmsecs)
{
  // Convert milliseconds to seconds and nanoseconds
  auto seconds = unixmsecs / 1000;
  auto remaining_milliseconds = unixmsecs % 1000;

  // Convert seconds since epoch to time_t
  std::time_t time = static_cast<std::time_t>(seconds);

  // Convert to a tm structure (UTC)
  // std::tm tm = *std::gmtime(&time);
  // Convert to a tm structure (local time)
  std::tm tm = *std::localtime(&time);

  // Format the time as "yyyy-MM-dd hh:mm:ss" and append milliseconds
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  // oss << '.' << std::setfill('0') << std::setw(3) << remaining_milliseconds;

  return oss.str();
}

// ----------------------------------------------------------------------------
static std::string secs_unix_to_calendar_time_local(uint64_t unixsecs)
{
  // Convert seconds since epoch to time_t
  std::time_t time = static_cast<std::time_t>(unixsecs);

  // Convert to a tm structure (UTC)
  // std::tm tm = *std::gmtime(&time);
  // Convert to a tm structure (local time)
  std::tm tm = *std::localtime(&time);

  // Format the time as "yyyy-MM-dd hh:mm:ss"
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

  return oss.str();
}

// ----------------------------------------------------------------------------
static std::string getCurrentUtcTime(std::string const format = "%Y-%m-%d %H:%M:%S")
{
  // Get current time in UTC
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);

  // Format the time as (for example) "yyyy-MM-dd hh:mm:ss"
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&now_time_t), format.c_str());

  return oss.str();
}

#pragma once

// STL
#include <exception>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

/// Definitions of different candlestick data resolutions

// ----------------------------------------------------------------------------
struct candle_res
{
  // this is the actual resolution of the candle
  double res_;
  // this is used when resampling to know which (higher) res to use
  double base_;
  // A simple name that will appear in menus
  char const* name_;
  // operators to make access easy
  constexpr operator double() const
  {
    return res_;
  }
  constexpr operator const char*() const
  {
    return name_;
  }
  bool operator<(const candle_res& other)
  {
    return res_ < other.res_;
  }
  bool operator>(const candle_res& other)
  {
    return res_ > other.res_;
  }
  bool operator==(const candle_res& other)
  {
    return res_ == other.res_;
  }

  friend std::ostream& operator<<(std::ostream& os, const candle_res& res)
  {
    return os << res.name_;
  }
};

// ----------------------------------------------------------------------------
class ohlc_data_resolutions
{
  public:
  static constexpr candle_res minute = {60 * 1000, 1, "1m"};
  static constexpr candle_res minute3 = {minute * 3, minute, "3m"};
  static constexpr candle_res minute5 = {minute * 5, minute, "5m"};
  static constexpr candle_res minute10 = {minute * 10, minute5, "10m"};
  static constexpr candle_res minute15 = {minute * 15, minute5, "15m"};
  static constexpr candle_res minute30 = {minute * 30, minute15, "30m"};
  static constexpr candle_res hour = {minute * 60, minute30, "1h"};
  static constexpr candle_res hour2 = {hour * 2, hour, "2h"};
  static constexpr candle_res hour4 = {hour * 4, hour2, "4h"};
  static constexpr candle_res hour6 = {hour * 6, hour2, "6h"};
  static constexpr candle_res hour12 = {hour * 12, hour6, "12h"};
  static constexpr candle_res day = {hour * 24, hour12, "1d"};
  static constexpr candle_res day2 = {day * 2, day, "2d"};
  static constexpr candle_res day3 = {day * 3, day, "3d"};
  static constexpr candle_res day7 = {day * 7, day, "7d"};
  static constexpr candle_res day15 = {day * 15, day3, "15d"};

  // for easy access to array of all available resolutions
  static const std::vector<candle_res>& available_resolutions()
  {
    static const std::vector<candle_res> resolutions = {minute, minute3, minute5, minute10,
      minute15, minute30, hour, hour2, hour4, hour6, hour12, day, day2, day3, day7, day15};
    return resolutions;
  }

  static candle_res get_resolution(double res)
  {
    for (const auto& r : available_resolutions())
    {
      if (r.res_ == res)
        return r;
    }
    throw std::runtime_error("Resolution not found");
  }

  // return best resolution -
  // gcd(min3, min5)  = min1
  // gcd(min5, min15) = min5
  // gcd(hour4, hour6) = hour2
  static candle_res gcd(candle_res a, candle_res b)
  {
    if (a > b)
      std::swap(a, b);
    while (a.res_ > ohlc_data_resolutions::minute)
    {
      while (b.res_ >= a.res_)
      {
        if (b.res_ == a.res_)
          return b;
        b = get_resolution(b.base_);
      }
      a = get_resolution(a.base_);
    }
    return ohlc_data_resolutions::minute;
  }
};

using variant_param_types = std::variant<double, int, bool, candle_res>;
using variant_param_list = std::vector<std::tuple<std::string, variant_param_types>>;

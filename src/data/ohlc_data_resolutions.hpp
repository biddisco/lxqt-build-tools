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
  constexpr operator double() const { return res_; }
  constexpr operator char const*() const { return name_; }
  bool operator<(candle_res const& other) { return res_ < other.res_; }
  bool operator>(candle_res const& other) { return res_ > other.res_; }
  bool operator==(candle_res const& other) { return res_ == other.res_; }

  friend std::ostream& operator<<(std::ostream& os, candle_res const& res)
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
  static std::vector<candle_res> const& available_resolutions()
  {
    static std::vector<candle_res> const resolutions = {minute, minute3, minute5, minute10,
        minute15, minute30, hour, hour2, hour4, hour6, hour12, day, day2, day3, day7, day15};
    return resolutions;
  }

  static candle_res get_resolution(double res)
  {
    for (auto const& r : available_resolutions())
    {
      if (r.res_ == res) return r;
    }
    throw std::runtime_error("Resolution not found");
  }

  // return best resolution -
  // gcd(min3, min5)  = min1
  // gcd(min5, min15) = min5
  // gcd(hour4, hour6) = hour2
  static candle_res gcd(candle_res a, candle_res b)
  {
    if (a > b) std::swap(a, b);
    while (a.res_ > ohlc_data_resolutions::minute)
    {
      while (b.res_ >= a.res_)
      {
        if (b.res_ == a.res_) return b;
        b = get_resolution(b.base_);
      }
      a = get_resolution(a.base_);
    }
    return ohlc_data_resolutions::minute;
  }
};

// ----------------------------------------------------------------------------
struct candle_data
{
  static constexpr std::array<char const*, 10> durations = {
      "1h", "1d", "1w", "2w", "1m", "6m", "1y", "2y", "4y", "all"};
  candle_res res_;
  std::uint64_t numSamples_;

  static std::uint64_t samples(candle_res res, std::string timestring)
  {
    std::uint64_t samples = 0;
    if (timestring == "1h") { samples = (60 * 60 * 1000ll) / res; }
    else if (timestring == "1d") { samples = (24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "1w") { samples = (7 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "2w") { samples = (14 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "1m") { samples = (30 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "6m") { samples = (182 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "1y") { samples = (365 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "2y") { samples = (2 * 365 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "4y") { samples = (4 * 365 * 24 * 60 * 60 * 1000ll) / res; }
    else if (timestring == "all") { samples = std::numeric_limits<std::uint64_t>::max(); }
    return samples;
  }

  std::string as_string() const
  {
    if (numSamples_ <= (60 * 60 * 1000ll) / res_)
      return "1h";
    else if (numSamples_ <= (24 * 60 * 60 * 1000ll) / res_)
      return "1d";
    else if (numSamples_ <= (7 * 24 * 60 * 60 * 1000ll) / res_)
      return "1w";
    else if (numSamples_ <= (14 * 24 * 60 * 60 * 1000ll) / res_)
      return "2w";
    else if (numSamples_ <= (30 * 24 * 60 * 60 * 1000ll) / res_)
      return "1m";
    else if (numSamples_ <= (182 * 24 * 60 * 60 * 1000ll) / res_)
      return "6m";
    else if (numSamples_ <= (365 * 24 * 60 * 60 * 1000ll) / res_)
      return "1y";
    else if (numSamples_ <= (2 * 365 * 24 * 60 * 60 * 1000ll) / res_)
      return "2y";
    else if (numSamples_ <= (4 * 365 * 24 * 60 * 60 * 1000ll) / res_)
      return "4y";
    return "all";
  }

  friend std::ostream& operator<<(std::ostream& os, candle_data const& data)
  {
    return os << data.res_.name_;
  }
};

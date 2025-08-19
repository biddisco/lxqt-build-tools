#pragma once

// STL
#include <algorithm>
//
#include "currency/ohlctv_sample.hpp"

// ----------------------------------------------------------------------------
struct ohlc_heikin_ashi
{
  bool first_;
  ohlctv_sample prev_;

  ohlc_heikin_ashi()
    : first_(true)
    , prev_()
  {
  }

  ohlc_heikin_ashi(ohlctv_sample const& ohlc)
    : first_(false)
    , prev_(ohlc)
  {
    // if inital ohlc sample is empty, set flag
    if (prev_.time == 0) { first_ = true; }
  }

  ohlctv_sample operator()(ohlctv_sample const& ohlc)
  {
    // first point in plot needs a prev open/close
    if (first_)
    {
      prev_ = ohlc;
      first_ = false;
    }
    //
    double close = 0.25 * (ohlc.open + ohlc.high + ohlc.low + ohlc.close);
    double open = 0.50 * (prev_.open + prev_.close);
    double high = std::max(std::max(ohlc.open, ohlc.close), ohlc.high);
    double low = std::min(std::min(ohlc.open, ohlc.close), ohlc.low);
    ohlctv_sample result(ohlc.time, open, high, low, close, ohlc.volume);
    prev_ = result;
    return result;
  }
};

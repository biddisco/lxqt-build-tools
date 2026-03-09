#pragma once

#include <limits>
//
#include "currency/ohlctv_sample.hpp"
#include "indicators/indicator_types.hpp"
// #include "data/ohlc_data_resolutions.hpp"

namespace indicators::kernels {

  // ----------------------------------------------------------------------------
  struct heikin_ashi_transition
  {
    bool first_;
    bool prev_green_;
    ohlctv_sample prev_sample_;

    // ------------------------------
    heikin_ashi_transition()
      : first_(true)
      , prev_green_(false)
    {
    }

    indicators::buy_sell_event_type operator()(ohlctv_sample const& ohlc)
    {
      // first point in plot needs a prev open/close
      if (first_)
      {
        prev_green_ = (ohlc.open < ohlc.close);
        first_ = false;
      }
      else
      {
        double close = 0.25 * (ohlc.open + ohlc.high + ohlc.low + ohlc.close);
        double open = 0.50 * (prev_sample_.open + prev_sample_.close);
        double high = std::max(std::max(ohlc.open, ohlc.close), ohlc.high);
        double low = std::min(std::min(ohlc.open, ohlc.close), ohlc.low);
        prev_sample_ = {ohlc.time, open, high, low, close, ohlc.volume};

        bool green = (prev_sample_.open < prev_sample_.close);
        if (green != prev_green_)
        {
          prev_green_ = green;
          return (
              green ? indicators::buy_sell_event_type::buy : indicators::buy_sell_event_type::sell);
        }
        prev_green_ = green;
      }
      return indicators::buy_sell_event_type::empty;
    }
  };

}    // namespace indicators::kernels

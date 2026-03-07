#pragma once

#include <algorithm>
#include <cmath>

#include <boost/circular_buffer.hpp>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Average True Range (ATR) indicator.
  ///
  /// Measures market volatility by computing an exponentially smoothed average
  /// of the True Range. The True Range for each period is the greatest of:
  ///   1. Current High - Current Low
  ///   2. |Current High - Previous Close|
  ///   3. |Current Low  - Previous Close|
  ///
  /// Output: single ATR value (overlay: no_overlay, separate axis)
  class average_true_range : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(average_true_range, operator_type);

    // ---------------------------------------
    /// Default constructor
    average_true_range(int window_size = 14)
      : indicator_base("Average True Range (ATR)", "Average True Range", {overlay_type::no_overlay})
      , window_size_(window_size)
      , atr_(0)
      , prev_close_(0)
      , count_(0)
      , first_(true)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Window size", 14},                                             // 1
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      atr_ = 0;
      prev_close_ = 0;
      count_ = 0;
      first_ = true;
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      double true_range;
      if (first_)
      {
        // first sample: TR = High - Low
        true_range = ohlc.high - ohlc.low;
        prev_close_ = ohlc.close;
        first_ = false;
      }
      else
      {
        // TR = max(H-L, |H-prevC|, |L-prevC|)
        double hl = ohlc.high - ohlc.low;
        double hpc = std::abs(ohlc.high - prev_close_);
        double lpc = std::abs(ohlc.low - prev_close_);
        true_range = std::max({hl, hpc, lpc});
        prev_close_ = ohlc.close;
      }

      // Wilder's smoothing: ATR = ((ATR_prev * (N-1)) + TR) / N
      if (count_ < window_size_)
      {
        atr_ = ((atr_ * count_) + true_range) / (count_ + 1);
        count_++;
      }
      else { atr_ = ((atr_ * (window_size_ - 1)) + true_range) / window_size_; }

      return atr_;
    }

    // ---------------------------------------
    inline double getLastResult() { return atr_; }

private:
    int window_size_;
    double atr_;
    double prev_close_;
    int count_;
    bool first_;
  };

}    // namespace indicators

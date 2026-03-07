#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>

#include <boost/circular_buffer.hpp>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Commodity Channel Index (CCI) indicator.
  ///
  /// The CCI measures how far the current typical price deviates from its
  /// statistical mean. Values above +100 suggest overbought conditions;
  /// values below -100 suggest oversold conditions.
  ///
  /// Computation:
  ///   Typical Price (TP) = (High + Low + Close) / 3
  ///   CCI = (TP - SMA(TP, N)) / (0.015 * Mean Deviation)
  ///
  /// Output: single CCI value (overlay: no_overlay, separate axis)
  class commodity_channel_index : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(commodity_channel_index, operator_type);

    // ---------------------------------------
    /// Default constructor
    commodity_channel_index(int window_size = 20)
      : indicator_base(
            "Commodity Channel Index (CCI)", "Commodity Channel Index", {overlay_type::no_overlay})
      , window_size_(window_size)
      , buffer_(window_size)
      , cci_(0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Window size", 20},                                             // 1
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      buffer_ = boost::circular_buffer<double>(window_size_);
      cci_ = 0;
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      // Typical Price = (High + Low + Close) / 3
      double tp = (ohlc.high + ohlc.low + ohlc.close) / 3.0;
      buffer_.push_back(tp);

      // Simple Moving Average of TP
      double sum = std::accumulate(buffer_.begin(), buffer_.end(), 0.0);
      double sma = sum / buffer_.size();

      // Mean Deviation = average of |TP_i - SMA|
      double dev_sum = 0.0;
      for (auto const& val : buffer_) { dev_sum += std::abs(val - sma); }
      double mean_dev = dev_sum / buffer_.size();

      // CCI = (TP - SMA) / (0.015 * Mean Deviation)
      if (mean_dev > 0.0) { cci_ = (tp - sma) / (0.015 * mean_dev); }
      else { cci_ = 0.0; }

      return cci_;
    }

    // ---------------------------------------
    inline double getLastResult() { return cci_; }

private:
    int window_size_;
    boost::circular_buffer<double> buffer_;
    double cci_;
  };

}    // namespace indicators

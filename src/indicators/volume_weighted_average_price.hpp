#pragma once

#include <boost/circular_buffer.hpp>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Volume Weighted Average Price (VWAP) indicator.
  ///
  /// A trading benchmark that gives the average price a security has traded at
  /// over a rolling window, weighted by volume.
  ///
  /// Computation:
  ///   Typical Price = (High + Low + Close) / 3
  ///   VWAP = sum(TP * Volume, over window) / sum(Volume, over window)
  ///
  /// The rolling window defaults to 20 bars. Using a window keeps the VWAP
  /// responsive to recent price action rather than converging to a long-term
  /// average.
  ///
  /// Output: single VWAP value (overlay: price axis)
  class volume_weighted_average_price : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_V2(volume_weighted_average_price)

    // ---------------------------------------
    /// Default constructor
    volume_weighted_average_price(int window_size = 20)
      : indicator_base("VWAP", "Volume Weighted Average Price", {overlay_type::price})
      , window_size_(window_size)
      , tp_vol_buf_(window_size)
      , vol_buf_(window_size)
      , vwap_(0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", 20},                                       // 1
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      tp_vol_buf_ = boost::circular_buffer<double>(window_size_);
      vol_buf_ = boost::circular_buffer<double>(window_size_);
      vwap_ = 0;
    }

    // ---------------------------------------
    /// Named output: "vwap"
    output_descriptors get_output_descriptors() const override
    {
      return {{"vwap", overlay_type::price}};
    }

    // ---------------------------------------
    sample_result process_sample(market_sample const& sample) override
    {
      auto const& ohlc = std::get<ohlctv_sample>(sample);
      return operator()(ohlc);
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      double typical_price = (ohlc.high + ohlc.low + ohlc.close) / 3.0;
      double tp_vol = typical_price * ohlc.volume;

      tp_vol_buf_.push_back(tp_vol);
      vol_buf_.push_back(ohlc.volume);

      // Sum over the rolling window
      double sum_tp_vol = 0;
      double sum_vol = 0;
      for (std::size_t i = 0; i < tp_vol_buf_.size(); ++i)
      {
        sum_tp_vol += tp_vol_buf_[i];
        sum_vol += vol_buf_[i];
      }

      if (sum_vol != 0.0) { vwap_ = sum_tp_vol / sum_vol; }

      return vwap_;
    }

    // ---------------------------------------
    inline double getLastResult() { return vwap_; }

private:
    int window_size_;
    boost::circular_buffer<double> tp_vol_buf_;
    boost::circular_buffer<double> vol_buf_;
    double vwap_;
  };

}    // namespace indicators

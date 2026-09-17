#pragma once

#include <boost/circular_buffer.hpp>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Williams %R indicator.
  ///
  /// A momentum oscillator that measures the level of the close relative
  /// to the highest high over a lookback period. It oscillates between
  /// -100 and 0:
  ///   %R = ((Highest High - Close) / (Highest High - Lowest Low)) * -100
  ///
  /// Values near 0 indicate overbought conditions; values near -100
  /// indicate oversold conditions.
  ///
  /// Output: single Williams %R value (overlay: no_overlay, separate axis)
  class williams_percent_r : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_V2(williams_percent_r)

    // ---------------------------------------
    /// Default constructor
    williams_percent_r(int window_size = 14)
      : indicator_base("Williams %R", "Williams Percent Range", {overlay_type::no_overlay})
      , window_size_(window_size)
      , highs_(window_size)
      , lows_(window_size)
      , williams_r_(0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", 14},                                       // 1
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      highs_ = boost::circular_buffer<double>(window_size_);
      lows_ = boost::circular_buffer<double>(window_size_);
      williams_r_ = 0;
    }

    // ---------------------------------------
    /// Named output: "wr"
    output_descriptors get_output_descriptors() const override
    {
      return {{"wr", overlay_type::no_overlay}};
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
      highs_.push_back(ohlc.high);
      lows_.push_back(ohlc.low);

      if (highs_.full())
      {
        double highest_high = *std::max_element(highs_.begin(), highs_.end());
        double lowest_low = *std::min_element(lows_.begin(), lows_.end());
        double range = highest_high - lowest_low;

        if (range != 0.0) { williams_r_ = ((highest_high - ohlc.close) / range) * -100.0; }
        else { williams_r_ = 0.0; }
      }
      else { williams_r_ = 0.0; }

      return williams_r_;
    }

    // ---------------------------------------
    inline double getLastResult() { return williams_r_; }

private:
    int window_size_;
    boost::circular_buffer<double> highs_;
    boost::circular_buffer<double> lows_;
    double williams_r_;
  };

}    // namespace indicators
